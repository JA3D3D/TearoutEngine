// Processor-level tests: parameters -> song, state round trip, capture, seed files, MIDI out, drag files.
#include "../src/plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using namespace juce;

static int checks = 0, failed = 0;
#define CHECK(c, m) do { ++checks; if (! (c)) { ++failed; std::printf ("FAIL: %s\n", m); } } while (0)

static void setP (tep::TearoutProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void run (tep::TearoutProcessor& p, int blocks, MidiBuffer* in = nullptr, int* ons = nullptr, int* offs = nullptr, double* energy = nullptr)
{
    AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer midi;
        if (in && b == 0) midi = *in;
        p.processBlock (buf, midi);
        for (const auto m : midi)
        {
            if (ons && m.getMessage().isNoteOn()) ++*ons;
            if (offs && m.getMessage().isNoteOff()) ++*offs;
        }
        if (energy) *energy += buf.getRMSLevel (0, 0, 512);
    }
}

struct FakeHead : AudioPlayHead
{
    double ppq = 0, bpm = 150;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setIsPlaying (playing);
        i.setBpm (bpm);
        i.setPpqPosition (ppq);
        return i;
    }
};

static File writeTestTone (double hz, const String& name)
{
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile (name);
    f.deleteFile();
    const double sr = 44100.0;
    AudioBuffer<float> buf (2, (int) (sr * 2.0));
    double ph = 0;
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        ph += hz / sr;
        double v = 0;
        for (int h = 1; h <= 10; ++h) v += std::sin (MathConstants<double>::twoPi * h * ph) / h;
        const float x = (float) std::tanh (1.5 * v) * 0.7f;
        buf.setSample (0, i, x);
        buf.setSample (1, i, x);
    }
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    auto writer = wav.createWriterFor (os, AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (writer) writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    return f;
}

static double blockEnergy (tep::TearoutProcessor& p, int blocks, int* notesOn = nullptr, int* wrongChannel = nullptr, FakeHead* head = nullptr)
{
    AudioBuffer<float> buf (2, 512);
    double e = 0;
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer midi;
        p.processBlock (buf, midi);
        e += buf.getRMSLevel (0, 0, 512);
        for (const auto m : midi)
        {
            if (m.getMessage().isNoteOn())
            {
                if (notesOn) ++*notesOn;
                if (wrongChannel && m.getMessage().getChannel() != 1) ++*wrongChannel;
            }
        }
        if (head) head->ppq += 512.0 / (48000.0 * 60.0 / head->bpm);
    }
    return e;
}

int main()
{
    ScopedJuceInitialiser_GUI init;

    // ---- parameters drive the song ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        setP (p, tep::ids::genre, 3); // trap
        setP (p, tep::ids::bars, 2);  // 16
        p.regenerateNow();
        auto s = p.getSong();
        CHECK (s && s->bars == 16, "BARS param reaches the engine");
        CHECK (s->pattern[(size_t) te::Layer::Drums] == te::pat::DrTrap, "GENRE param reaches the engine");
        const auto chords = s->chordNames;
        p.newVariation();
        CHECK (p.getSong()->chordNames == chords, "VARIATION keeps chords");
        setP (p, tep::ids::progression, (float) te::kProgressionFirst + 2);
        p.regenerateNow();
        const auto fixed = p.getSong()->chordNames;
        p.generateNew();
        CHECK (p.getSong()->chordNames == fixed, "GENERATE keeps an explicit progression");
    }

    // ---- preview audio + MIDI out ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        double silent = 0;
        run (p, 20, nullptr, nullptr, nullptr, &silent);
        CHECK (silent == 0.0, "silent when stopped");
        setP (p, tep::ids::midiOut, 2);
        p.setPreviewPlaying (true);
        int ons = 0, offs = 0;
        double energy = 0;
        run (p, 400, nullptr, &ons, &offs, &energy);
        CHECK (energy > 1.0, "preview makes sound");
        CHECK (ons > 20, "MIDI out sends notes");
        p.setPreviewPlaying (false);
        run (p, 4, nullptr, &ons, &offs, nullptr);
        CHECK (ons == offs, "MIDI out: every note-on gets a note-off");
        p.setMute (2, true);
        CHECK (p.isMuted (2) && ! p.isMuted (1), "mute mask");
    }

    // ---- capture a clip from MIDI input ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        p.setCapture (true);
        // Fm (bar 1) then Db (bar 2) as block chords, two hits per bar, at 150 bpm: 1 bar = 76800 samples
        const int barSamples = 76800;
        std::vector<std::pair<int, MidiMessage>> events;
        auto chord = [&] (std::initializer_list<int> notes, int barIndex) {
            for (int s = 0; s < 2; ++s)
            {
                const int start = 3000 + barIndex * barSamples + s * (barSamples / 2);
                const int stop = start + barSamples / 2 - 2000;
                for (int n : notes)
                {
                    events.push_back ({ start, MidiMessage::noteOn (1, n, (uint8) 100) });
                    events.push_back ({ stop, MidiMessage::noteOff (1, n) });
                }
            }
        };
        chord ({ 41, 65, 68, 72 }, 0);
        chord ({ 37, 65, 68, 73 }, 1);
        for (int k = 0; k * 512 < 2 * barSamples + 6000; ++k)
        {
            MidiBuffer in;
            for (auto& e : events)
                if (e.first >= k * 512 && e.first < (k + 1) * 512) in.addEvent (e.second, e.first - k * 512);
            run (p, 1, &in);
        }
        CHECK (p.isCapturing(), "capturing");
        p.setCapture (false);
        CHECK (p.getSeed().valid, "capture produces a seed");
        CHECK (p.getSeed().chords.size() >= 2, "captured chords detected");
        std::printf ("  captured seed: %s\n", p.getSeed().summary.c_str());
        CHECK (roundToInt (p.apvts.getRawParameterValue (tep::ids::progression)->load()) == te::kProgressionSeed,
               "seed switches PROGRESSION to From Seed");
    }

    // ---- seed file + state round trip + drag files ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        std::vector<te::Chord> ch;
        te::parseProgression ("i VI III VII", 2, te::Mode::Minor, ch); // D minor
        std::vector<te::Note> notes;
        for (size_t i = 0; i < ch.size(); ++i)
            for (int pcs : ch[i].pitchClasses())
                notes.push_back ({ te::placeInRange (pcs, 57), (int) i * te::BAR, te::BAR, 100 });
        auto bytes = te::writeMidi ({ { "seed", notes, 0 } }, 140.0);
        auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("te_seed_test.mid");
        f.replaceWithData (bytes.data(), bytes.size());
        String msg;
        CHECK (p.loadSeedFile (f, msg), "seed file loads");
        CHECK (p.getSong()->chordNames == "Dm  Bb  F  C", "seed file chords used");
        CHECK (roundToInt (p.apvts.getRawParameterValue (tep::ids::key)->load()) == 2, "seed sets KEY");

        setP (p, tep::ids::layer (2, "movement"), 0.8f);
        setP (p, tep::ids::layer (4, "pattern"), 3);
        p.setSolo (5, true);
        p.regenerateNow();
        MemoryBlock state;
        p.getStateInformation (state);

        tep::TearoutProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        q.regenerateNow();
        CHECK (q.getSeed().valid && q.getSeed().summary == p.getSeed().summary, "seed survives save/load");
        CHECK (std::abs (q.apvts.getRawParameterValue (tep::ids::layer (2, "movement"))->load() - 0.8f) < 0.011f, "params survive save/load");
        CHECK (q.isSoloed (5), "solo survives save/load");
        bool same = true;
        for (int l = 0; l < te::kNumLayers; ++l) same = same && p.getSong()->layers[(size_t) l] == q.getSong()->layers[(size_t) l];
        CHECK (same, "the restored plugin generates the identical drop");

        for (int l = -1; l < te::kNumLayers; ++l)
        {
            auto mf = p.writeLayerMidi (l);
            if (l >= 0 && p.getSong()->layers[(size_t) l].empty()) { CHECK (mf == File(), "Off layer gives no file"); continue; }
            CHECK (mf.existsAsFile(), "drag file written");
            MemoryBlock mb;
            mf.loadFileAsData (mb);
            std::vector<uint8_t> b ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
            std::vector<te::Note> back;
            double bpm;
            std::string err;
            CHECK (te::readMidi (b, back, bpm, err, false), "drag file is valid MIDI");
            if (l >= 0) CHECK (back.size() == p.getSong()->layers[(size_t) l].size(), "drag file has the layer's notes");
        }
        auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("te_save_test");
        dir.deleteRecursively();
        CHECK (p.saveAllMidi (dir, msg), "SAVE .MID writes files");
        CHECK (dir.getNumberOfChildFiles (File::findFiles, "*.mid") >= 3, "SAVE .MID writes one file per layer");
        p.clearSeed();
        CHECK (! p.getSeed().valid, "seed cleared");
        CHECK (roundToInt (p.apvts.getRawParameterValue (tep::ids::progression)->load()) == te::kProgressionAuto, "clear seed resets PROGRESSION");
    }


    // ---- 1.1: sound slots ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        auto tone = writeTestTone (34.65, "TE test growl - C.wav"); // named C, really C#0
        String msg;
        CHECK (p.loadLayerSound ((int) te::Layer::BassA, tone, msg), "a WAV loads into BASS A");
        std::printf ("  %s\n", msg.toRawUTF8());
        CHECK (p.getSound ((int) te::Layer::BassA).root == 25, "root detected from the audio (C#0)");
        CHECK (p.getSound ((int) te::Layer::BassA).file.isAChildOf (tep::TearoutProcessor::soundsFolder()), "sound copied into the sounds folder");
        CHECK (p.soundFileForDrag ((int) te::Layer::BassA).existsAsFile(), "SOUND drag file exists");
        CHECK (! p.loadLayerSound ((int) te::Layer::Drums, tone, msg), "drums don't take a sound");

        // Simpler-ready MIDI: shifted so Simpler's C3 root plays the right pitch
        auto readNotes = [] (const File& f) {
            MemoryBlock mb;
            f.loadFileAsData (mb);
            std::vector<uint8_t> b ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
            std::vector<te::Note> n;
            double bpm;
            std::string err;
            te::readMidi (b, n, bpm, err, false);
            std::sort (n.begin(), n.end(), [] (const te::Note& a, const te::Note& c) { return a.start != c.start ? a.start < c.start : a.pitch < c.pitch; });
            return n;
        };
        auto songNotes = p.getSong()->layers[(size_t) te::Layer::BassA];
        auto shifted = readNotes (p.writeLayerMidi ((int) te::Layer::BassA));
        CHECK (! shifted.empty() && shifted.size() == songNotes.size() && shifted[0].pitch == songNotes[0].pitch + 35, "Simpler-ready MIDI is shifted by (C3 - root)");
        p.setSimplerMidi ((int) te::Layer::BassA, false);
        auto plain = readNotes (p.writeLayerMidi ((int) te::Layer::BassA));
        CHECK (! plain.empty() && plain[0].pitch == songNotes[0].pitch, "Simpler-ready off = true pitches");
        p.setSimplerMidi ((int) te::Layer::BassA, true);
        p.nudgeRoot ((int) te::Layer::BassA, 1);
        CHECK (p.getSound ((int) te::Layer::BassA).root == 26, "root nudge");
        p.nudgeRoot ((int) te::Layer::BassA, -1);

        // the preview plays the sound
        p.setPreviewPlaying (true);
        CHECK (blockEnergy (p, 200) > 1.0, "preview plays with a user sound");

        // saved with the Set
        MemoryBlock state;
        p.getStateInformation (state);
        tep::TearoutProcessor q;
        q.prepareToPlay (48000, 512);
        q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q.getSound ((int) te::Layer::BassA).data != nullptr && q.getSound ((int) te::Layer::BassA).root == 25, "sound + root survive save/load");
        q.clearLayerSound ((int) te::Layer::BassA);
        CHECK (q.getSound ((int) te::Layer::BassA).data == nullptr && ! q.soundFileForDrag ((int) te::Layer::BassA).existsAsFile(), "sound cleared");
        tone.deleteFile();
    }

    // ---- 1.1: PLAY / STOP, follow Live, layer ON/OFF, MIDI OUT per layer ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        FakeHead head;
        p.setPlayHead (&head);
        p.setPreviewPlaying (false);
        CHECK (blockEnergy (p, 100, nullptr, nullptr, &head) == 0.0, "STOP is silent even while Live plays");
        setP (p, tep::ids::midiOut, 5); // Bass A only
        int ons = 0, wrong = 0;
        blockEnergy (p, 300, &ons, &wrong, &head);
        CHECK (ons > 5 && wrong == 0, "MIDI OUT keeps following Live with the preview silent (one layer, channel 1)");
        setP (p, tep::ids::midiOut, 0);
        p.setPreviewPlaying (true);
        CHECK (blockEnergy (p, 100, nullptr, nullptr, &head) > 0.5, "PLAY follows Live");
        for (int l = 0; l < te::kNumLayers; ++l) p.setMute (l, true);
        blockEnergy (p, 20, nullptr, nullptr, &head);
        CHECK (blockEnergy (p, 40, nullptr, nullptr, &head) < 0.05, "all layers OFF cuts the sound right away");
        for (int l = 0; l < te::kNumLayers; ++l) p.setMute (l, false);
        p.setFollowLive (false);
        head.playing = false;
        CHECK (blockEnergy (p, 100) > 0.5, "own loop when not following Live");
        p.setPlayHead (nullptr);
    }

    // ---- 1.1: GENERATE = a new drop; 1.0 sets keep their patterns ----
    {
        tep::TearoutProcessor p;
        p.prepareToPlay (48000, 512);
        int different = 0;
        for (int k = 0; k < 6; ++k)
        {
            auto before = p.getSong();
            p.generateNew();
            auto after = p.getSong();
            int d = before->progressionLabel != after->progressionLabel ? 2 : 0;
            for (int l = 0; l < te::kNumLayers; ++l) if (before->pattern[(size_t) l] != after->pattern[(size_t) l]) ++d;
            if (d >= 2) ++different;
        }
        CHECK (different >= 5, "GENERATE gives a clearly different drop");

        setP (p, tep::ids::songSeed, 500);
        p.regenerateNow();
        MemoryBlock state;
        p.getStateInformation (state);
        auto xml = AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        if (auto* extra = xml->getChildByName ("TE_EXTRA")) extra->removeAttribute ("engine");
        MemoryBlock legacy;
        AudioProcessor::copyXmlToBinary (*xml, legacy);
        tep::TearoutProcessor q;
        q.setStateInformation (legacy.getData(), (int) legacy.getSize());
        q.regenerateNow();
        bool defaults = true;
        for (int l = 0; l < te::kNumLayers; ++l)
            defaults = defaults && q.getSong()->pattern[(size_t) l] == te::genreProfile (q.currentSettings().genre).defaults[l];
        CHECK (defaults, "a 1.0 Set opens with the same patterns it had");
    }

    std::printf ("%d plugin checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
