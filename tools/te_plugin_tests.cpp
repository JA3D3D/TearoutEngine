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

    std::printf ("%d plugin checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
