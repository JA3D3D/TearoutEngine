#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace tep
{
using namespace juce;

static const char* kLayerTitles[] = { "Chords", "Sub", "Bass A", "Bass B", "Lead", "Arp", "Drums", "FX" };

static StringArray toStringArray (const std::vector<std::string>& v)
{
    StringArray a;
    for (auto& s : v) a.add (String::fromUTF8 (s.c_str()));
    return a;
}

AudioProcessorValueTreeState::ParameterLayout TearoutProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return String (roundToInt (v * 100.0f)) + " %"; });

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::genre, 1 }, "Genre", toStringArray (te::genreChoiceNames()), 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::key, 1 }, "Key", toStringArray (te::keyChoiceNames()), 5));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::progression, 1 }, "Progression",
                                                         toStringArray (te::progressionChoiceNames()), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::chordLength, 1 }, "Chord Length",
                                                         StringArray { "1/2 Bar", "1 Bar", "2 Bars" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::bars, 1 }, "Bars",
                                                         StringArray { "4 Bars", "8 Bars", "16 Bars", "32 Bars" }, 1));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::songSeed, 1 }, "Song Seed", 1, 99999, 1));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::grooveSeed, 1 }, "Groove Seed", 1, 99999, 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::volume, 1 }, "Preview Volume",
                                                        NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -6.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::midiOut, 1 }, "MIDI Out",
                                                         StringArray { "Off", "Selected Layer", "All Layers", "Chords", "Sub / 808",
                                                                       "Bass A", "Bass B", "Lead", "Arp", "Drums", "FX" }, 0));
    for (int l = 0; l < te::kNumLayers; ++l)
    {
        const String t = kLayerTitles[l];
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::layer (l, "pattern"), 1 }, t + " Pattern",
                                                             toStringArray (te::patternChoiceNames ((te::Layer) l)), 0));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::layer (l, "octave"), 1 }, t + " Octave", -2, 2, 0));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::layer (l, "movement"), 1 }, t + " Movement",
                                                            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f, percent));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::layer (l, "length"), 1 }, t + " Length",
                                                            NormalisableRange<float> (0.25f, 1.0f, 0.01f),
                                                            te::defaultLength ((te::Layer) l), percent));
    }
    return { p.begin(), p.end() };
}

TearoutProcessor::TearoutProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TEAROUT", createLayout())
{
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (param))
            apvts.addParameterListener (rp->getParameterID(), this);
    midiScratch.reserve (4096);
    formats.registerBasicFormats();
    for (auto& p : soundPtr) p.store (nullptr);
    regenerateNow();
    startTimerHz (30);
}

TearoutProcessor::~TearoutProcessor()
{
    stopTimer();
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (param))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

bool TearoutProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void TearoutProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    synth.prepare (sampleRate);
    player = te::SongPlayer();
    renderBuffer.setSize (2, jmax (512, samplesPerBlock), false, true, true);
}

// ---- settings / generation -------------------------------------------------------------------------
te::Settings TearoutProcessor::currentSettings() const
{
    auto get = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };
    te::Settings s;
    s.genre = (te::Genre) jlimit (0, te::kNumGenres - 1, roundToInt (get (ids::genre)));
    s.keyRoot = roundToInt (get (ids::key));
    s.progression = roundToInt (get (ids::progression));
    s.chordLength = roundToInt (get (ids::chordLength));
    static const int barValues[] = { 4, 8, 16, 32 };
    s.bars = barValues[jlimit (0, 3, roundToInt (get (ids::bars)))];
    s.songSeed = (uint32_t) roundToInt (get (ids::songSeed));
    s.grooveSeed = (uint32_t) roundToInt (get (ids::grooveSeed));
    s.patternPools = engineVersion >= 2;
    for (int l = 0; l < te::kNumLayers; ++l)
    {
        auto& ls = s.layers[(size_t) l];
        ls.pattern = roundToInt (get (ids::layer (l, "pattern")));
        ls.octave = roundToInt (get (ids::layer (l, "octave")));
        ls.movement = get (ids::layer (l, "movement"));
        ls.length = get (ids::layer (l, "length"));
    }
    return s;
}

void TearoutProcessor::regenerateNow()
{
    dirty.store (false);
    auto s = std::make_shared<const te::Song> (te::generate (currentSettings(), seed.valid ? &seed : nullptr));
    publish (s);
}

void TearoutProcessor::publish (std::shared_ptr<const te::Song> s)
{
    song = s;
    songBpm.store (s->bpm);
    auto data = te::makePlayData (*s);
    playData.store (data.get());
    keepAlive.push_back ({ data, Time::getMillisecondCounter() });
    if (onSongChanged) onSongChanged();
}

static void setParam (AudioProcessorValueTreeState& apvts, const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void TearoutProcessor::generateNew()
{
    // a completely new drop: new progression (on Auto), new patterns for every layer set to "Genre",
    // new hook, new groove. Keep rolling until it's clearly different from what's playing now.
    engineVersion = 2;
    auto settings = currentSettings();
    settings.patternPools = true;
    auto& rnd = Random::getSystemRandom();
    int seedValue = rnd.nextInt ({ 2, 99999 });
    for (int attempt = 0; attempt < 24 && song; ++attempt)
    {
        settings.songSeed = (uint32_t) seedValue;
        const auto cand = te::generate (settings, seed.valid ? &seed : nullptr);
        int changed = cand.progressionLabel != song->progressionLabel || cand.chordNames != song->chordNames ? 2 : 0;
        for (int l = 0; l < te::kNumLayers; ++l)
            if (cand.pattern[(size_t) l] != song->pattern[(size_t) l]) ++changed;
        if (changed >= 2) break;
        seedValue = rnd.nextInt ({ 2, 99999 });
    }
    setParam (apvts, ids::songSeed, (float) seedValue);
    setParam (apvts, ids::grooveSeed, (float) rnd.nextInt ({ 1, 99999 }));
    regenerateNow();
}

void TearoutProcessor::newVariation()
{
    setParam (apvts, ids::grooveSeed, (float) Random::getSystemRandom().nextInt ({ 1, 99999 }));
    regenerateNow();
}

void TearoutProcessor::parameterChanged (const String& id, float)
{
    if (id == ids::volume || id == ids::midiOut) return;
    dirty.store (true);
}

void TearoutProcessor::timerCallback()
{
    if (dirty.load()) regenerateNow();
    drainCapture();

    // free play data the audio thread can no longer be using
    const auto now = Time::getMillisecondCounter();
    const auto* current = playData.load();
    keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(), [&] (const Retired& r) {
                         return r.data.get() != current && now - r.time > 2000; }), keepAlive.end());
    soundKeepAlive.erase (std::remove_if (soundKeepAlive.begin(), soundKeepAlive.end(), [&] (const RetiredSound& r) {
                              return now - r.time > 2000; }), soundKeepAlive.end());

    // capture finishes on its own when the transport stops after notes came in
    const bool playing = hostPlaying.load();
    if (capturing.load() && timerSawPlaying && ! playing && ! captured.empty())
        setCapture (false);
    timerSawPlaying = playing;
}

// ---- sound slots ----------------------------------------------------------------------------------------
bool TearoutProcessor::layerTakesSound (int layer)
{
    return layer >= 0 && layer < te::kNumLayers && layer != (int) te::Layer::Drums && layer != (int) te::Layer::FX;
}

bool TearoutProcessor::isAudioFile (const File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf");
}

File TearoutProcessor::soundsFolder()
{
   #if JUCE_MAC
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Application Support/TEAROUT ENGINE/Sounds");
   #else
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("TEAROUT ENGINE/Sounds");
   #endif
}

static String noteText (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[((midi % 12) + 12) % 12]) + String (midi / 12 - 2);
}

void TearoutProcessor::publishSound (int layer, std::shared_ptr<const te::SampleData> data)
{
    auto& slot = sounds[(size_t) layer];
    if (slot.data) soundKeepAlive.push_back ({ slot.data, Time::getMillisecondCounter() });
    slot.data = std::move (data);
    soundPtr[(size_t) layer].store (slot.data.get());
}

bool TearoutProcessor::readSound (int layer, const File& file, int forcedRoot, const String& forcedInfo, String& message)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (! reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
    {
        message = "Couldn't read " + file.getFileName() + " as audio.";
        return false;
    }
    const int len = (int) jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    const int chans = (int) jlimit (1u, 2u, reader->numChannels);
    AudioBuffer<float> buf (chans, len);
    reader->read (&buf, 0, len, 0, true, chans > 1);
    std::vector<float> L ((size_t) len), R ((size_t) len), mono ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        L[(size_t) i] = buf.getSample (0, i);
        R[(size_t) i] = buf.getSample (chans > 1 ? 1 : 0, i);
        mono[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
    }
    auto& slot = sounds[(size_t) layer];
    if (forcedRoot >= 0)
    {
        slot.root = forcedRoot;
        slot.rootInfo = forcedInfo;
    }
    else
    {
        const auto rc = te::chooseRoot (mono, reader->sampleRate, file.getFileName().toStdString());
        slot.root = rc.midi;
        slot.rootInfo = String::fromUTF8 (rc.reason.c_str());
    }
    slot.file = file;
    publishSound (layer, te::makeSampleData (std::move (L), std::move (R), reader->sampleRate, slot.root));
    return true;
}

bool TearoutProcessor::loadLayerSound (int layer, const File& file, String& message)
{
    if (! layerTakesSound (layer))
    {
        message = "Drums and FX don't take a sound. Pick a pitched layer (chords, sub, bass, lead, arp).";
        return false;
    }
    if (! file.existsAsFile())
    {
        message = "File not found.";
        return false;
    }
    // keep our own copy so the Set still finds it if the original moves
    auto dir = soundsFolder();
    dir.createDirectory();
    File copy = file;
    if (! file.isAChildOf (dir))
    {
        copy = dir.getChildFile (file.getFileName());
        if (copy.existsAsFile() && copy.getSize() != file.getSize()) copy = copy.getNonexistentSibling (false);
        if (! copy.existsAsFile() && ! file.copyFileTo (copy)) copy = file;
    }
    if (! readSound (layer, copy, -1, {}, message)) return false;
    const auto& slot = sounds[(size_t) layer];
    message = file.getFileNameWithoutExtension() + " on " + String (te::layerName ((te::Layer) layer)) + "  (root " + noteText (slot.root) + ": "
            + slot.rootInfo + ")";
    return true;
}

void TearoutProcessor::clearLayerSound (int layer)
{
    if (layer < 0 || layer >= te::kNumLayers) return;
    publishSound (layer, nullptr);
    sounds[(size_t) layer].file = File();
    sounds[(size_t) layer].rootInfo = {};
}

void TearoutProcessor::nudgeRoot (int layer, int semitones)
{
    if (layer < 0 || layer >= te::kNumLayers) return;
    auto& slot = sounds[(size_t) layer];
    if (! slot.data) return;
    slot.root = jlimit (0, 127, slot.root + semitones);
    slot.rootInfo = "set by hand";
    auto copy = std::make_shared<te::SampleData> (*slot.data);
    copy->rootNote = slot.root;
    publishSound (layer, copy);
}

void TearoutProcessor::setSimplerMidi (int layer, bool on)
{
    if (layer >= 0 && layer < te::kNumLayers) sounds[(size_t) layer].simplerMidi = on;
}

File TearoutProcessor::soundFileForDrag (int layer) const
{
    const auto& slot = getSound (layer);
    return slot.data && slot.file.existsAsFile() ? slot.file : File();
}

// ---- seed ---------------------------------------------------------------------------------------------
bool TearoutProcessor::loadSeedFile (const File& file, String& message)
{
    MemoryBlock mb;
    if (! file.loadFileAsData (mb))
    {
        message = "Couldn't read " + file.getFileName();
        return false;
    }
    std::vector<uint8_t> bytes ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    std::vector<te::Note> notes;
    double bpm = 0;
    std::string err;
    if (! te::readMidi (bytes, notes, bpm, err))
    {
        message = String (err);
        return false;
    }
    loadSeedNotes (notes, file.getFileNameWithoutExtension());
    message = seed.valid ? String::fromUTF8 (seed.summary.c_str()) : String ("No usable notes in that clip.");
    return seed.valid;
}

void TearoutProcessor::loadSeedNotes (const std::vector<te::Note>& notes, const String& sourceName)
{
    auto a = te::analyseSeed (notes);
    if (! a.valid) return;
    seed = a;
    seedSource = sourceName;
    // the seed becomes the harmony, in its own key; BARS grows to fit it
    setParam (apvts, ids::key, (float) seed.keyRoot);
    setParam (apvts, ids::progression, (float) te::kProgressionSeed);
    const int barsIndex = roundToInt (apvts.getRawParameterValue (ids::bars)->load());
    static const int barValues[] = { 4, 8, 16, 32 };
    if (barValues[jlimit (0, 3, barsIndex)] < seed.bars)
        setParam (apvts, ids::bars, (float) (seed.bars <= 4 ? 0 : seed.bars <= 8 ? 1 : seed.bars <= 16 ? 2 : 3));
    regenerateNow();
}

void TearoutProcessor::clearSeed()
{
    seed = te::SeedAnalysis {};
    seedSource = {};
    if (roundToInt (apvts.getRawParameterValue (ids::progression)->load()) == te::kProgressionSeed)
        setParam (apvts, ids::progression, (float) te::kProgressionAuto);
    regenerateNow();
}

void TearoutProcessor::setCapture (bool on)
{
    if (on)
    {
        captured.clear();
        captureOpen.clear();
        capFifo.reset();
        captureClockStarted = false;
        capturing.store (true);
        return;
    }
    if (! capturing.exchange (false)) return;
    drainCapture();
    double last = 0;
    for (auto& n : captured) last = jmax (last, (double) n.end());
    for (auto& o : captureOpen)
        captured.push_back ({ o.first, (int) std::lround (o.second.first), jmax (te::STEP, (int) std::lround (last - o.second.first)), o.second.second });
    captureOpen.clear();
    if (! captured.empty())
        loadSeedNotes (captured, "Captured MIDI");
}

void TearoutProcessor::drainCapture()
{
    int start1, size1, start2, size2;
    capFifo.prepareToRead (capFifo.getNumReady(), start1, size1, start2, size2);
    auto handle = [this] (const CapEvent& e) {
        if (e.on)
            captureOpen[e.pitch] = { e.tick, e.velocity };
        else
        {
            auto it = captureOpen.find (e.pitch);
            if (it == captureOpen.end()) return;
            te::Note n;
            n.pitch = e.pitch;
            n.start = (int) std::lround (it->second.first);
            n.length = jmax (1, (int) std::lround (e.tick - it->second.first));
            n.velocity = it->second.second;
            captured.push_back (n);
            captureOpen.erase (it);
        }
    };
    for (int i = 0; i < size1; ++i) handle (capBuffer[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) handle (capBuffer[(size_t) (start2 + i)]);
    capFifo.finishedRead (size1 + size2);
}

// ---- audio ----------------------------------------------------------------------------------------------
uint32_t TearoutProcessor::audibleMask() const
{
    const uint32_t solo = soloMask.load();
    if (solo != 0) return solo;
    return ~muteMask.load() & 0xFFu;
}

void TearoutProcessor::setMute (int layer, bool m)
{
    auto v = muteMask.load();
    muteMask.store (m ? (v | (1u << layer)) : (v & ~(1u << layer)));
}

void TearoutProcessor::setSolo (int layer, bool s)
{
    auto v = soloMask.load();
    soloMask.store (s ? (v | (1u << layer)) : (v & ~(1u << layer)));
}

void TearoutProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const auto* data = playData.load();

    bool playing = false;
    double ppq = 0;
    double bpm = songBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) if (*b > 1.0) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    hostPlaying.store (playing);
    bpmNow.store (bpm);
    const double tps = bpm / 60.0 * te::PPQ / sampleRate;

    // ---- capture incoming MIDI ----
    if (capturing.load())
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! msg.isNoteOnOrOff()) continue;
            if (! playing && ! captureClockStarted && msg.isNoteOn())
            {
                captureClock = -meta.samplePosition * tps;
                captureClockStarted = true;
            }
            const double t = playing ? ppq * te::PPQ + meta.samplePosition * tps : captureClock + meta.samplePosition * tps;
            if (capFifo.getFreeSpace() > 0)
            {
                int s1, z1, s2, z2;
                capFifo.prepareToWrite (1, s1, z1, s2, z2);
                if (z1 > 0) capBuffer[(size_t) s1] = { t, msg.getNoteNumber(), msg.getVelocity(), msg.isNoteOn() };
                capFifo.finishedWrite (z1);
            }
        }
        if (! playing && captureClockStarted) captureClock += n * tps;
    }
    midi.clear();

    // ---- transport ----
    // PLAY = the preview is audible. FOLLOW LIVE = lock to Live's transport while it runs.
    // MIDI OUT keeps following Live even with the preview silent, so a linked Serum track still plays.
    const int mo = roundToInt (apvts.getRawParameterValue (ids::midiOut)->load());
    const uint32_t midiMask = mo == 0 ? 0u : (mo == 1 ? (1u << jlimit (0, 7, selectedLayer.load()))
                                                       : (mo == 2 ? 0xFFu : (1u << jlimit (0, 7, mo - 3))));
    if (mo != lastMidiOutMode)
    {
        for (int ch = 1; ch <= 8; ++ch) midi.addEvent (MidiMessage::allNotesOff (ch), 0);
        lastMidiOutMode = mo;
    }
    const bool audible = previewOn.load();
    const bool hostRun = playing && followLive.load();
    double startTick = 0;
    bool run = false;
    if (hostRun)
    {
        startTick = ppq * te::PPQ;
        run = audible || midiMask != 0;
    }
    else if (audible)
    {
        startTick = internalTick;
        internalTick += n * tps;
        run = true;
    }
    else
        internalTick = 0;
    if (capturing.load()) run = false; // stay quiet while recording a seed clip
    runningNow.store (run);
    for (int l = 0; l < te::kNumLayers; ++l)
        synth.setLayerSampleRaw (l, soundPtr[(size_t) l].load());

    // ---- render (in chunks the scratch buffer can hold) ----
    const float gain = Decibels::decibelsToGain (apvts.getRawParameterValue (ids::volume)->load(), -36.0f);
    const uint32_t mask = audible ? audibleMask() : 0u;
    int done = 0;
    while (done < n)
    {
        const int chunk = jmin (n - done, renderBuffer.getNumSamples());
        midiScratch.clear();
        player.process (synth, data, renderBuffer.getWritePointer (0), renderBuffer.getWritePointer (1), chunk,
                        startTick + done * tps, tps, run, mask, &midiScratch, midiMask);
        for (auto& e : midiScratch)
        {
            const int ch = mo == 2 ? e.layer + 1 : 1;
            midi.addEvent (e.on ? MidiMessage::noteOn (ch, e.pitch, (uint8) jlimit (1, 127, e.velocity))
                                : MidiMessage::noteOff (ch, e.pitch), done + e.sampleOffset);
        }
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.copyFrom (c, done, renderBuffer.getReadPointer (jmin (c, 1)), chunk, gain);
        done += chunk;
    }
    if (run && data && data->totalTicks > 0)
        playTick.store (std::fmod (startTick + n * tps, (double) data->totalTicks));
}

// ---- MIDI export ------------------------------------------------------------------------------------------
String TearoutProcessor::fileStem() const
{
    auto s = song;
    if (! s) return "TEAROUT";
    const auto& g = te::genreProfile (currentSettings().genre);
    String key = String::fromUTF8 (te::keyName (s->keyRoot, s->mode).c_str()).replaceCharacter (' ', '-');
    return "TEAROUT_" + String (g.shortName) + "_" + key + "_" + String (s->bars) + "bars";
}

// A layer with a sound gets "Simpler-ready" MIDI: shifted so Simpler's default root (C3) plays the right pitch
// when the dragged sound is dropped on the same track.
static std::vector<te::MidiTrackOut> tracksFor (const te::Song& s, int layer,
                                                const std::array<TearoutProcessor::SoundSlot, te::kNumLayers>& sounds)
{
    std::vector<te::MidiTrackOut> t;
    for (int l = 0; l < te::kNumLayers; ++l)
        if ((layer < 0 || layer == l) && ! s.layers[(size_t) l].empty())
        {
            auto notes = s.layers[(size_t) l];
            const auto& slot = sounds[(size_t) l];
            if (slot.data && slot.simplerMidi)
            {
                const int shift = 60 - slot.root;
                for (auto& n : notes)
                {
                    n.pitch += shift;
                    while (n.pitch > 127) n.pitch -= 12;
                    while (n.pitch < 0) n.pitch += 12;
                }
            }
            t.push_back ({ std::string ("TE ") + kLayerTitles[l], notes, 0 });
        }
    return t;
}

static bool writeBytes (const File& f, const std::vector<uint8_t>& bytes)
{
    f.getParentDirectory().createDirectory();
    return f.replaceWithData (bytes.data(), bytes.size());
}

File TearoutProcessor::writeLayerMidi (int layer) const
{
    auto s = song;
    if (! s) return {};
    auto tracks = tracksFor (*s, layer, sounds);
    if (tracks.empty()) return {};
    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("TEAROUT ENGINE");
    String name = fileStem() + "_" + (layer < 0 ? String ("ALL") : String (kLayerTitles[layer]).replaceCharacter (' ', '-'));
    auto f = dir.getChildFile (name + ".mid");
    return writeBytes (f, te::writeMidi (tracks, s->bpm)) ? f : File();
}

bool TearoutProcessor::saveAllMidi (const File& folder, String& message) const
{
    auto s = song;
    if (! s) return false;
    int written = 0;
    for (int l = -1; l < te::kNumLayers; ++l)
    {
        auto tracks = tracksFor (*s, l, sounds);
        if (tracks.empty()) continue;
        String name = fileStem() + "_" + (l < 0 ? String ("ALL") : String (kLayerTitles[l]).replaceCharacter (' ', '-'));
        if (writeBytes (folder.getChildFile (name + ".mid"), te::writeMidi (tracks, s->bpm))) ++written;
    }
    message = String (written) + " MIDI files saved to " + folder.getFullPathName();
    return written > 0;
}

// ---- state ----------------------------------------------------------------------------------------------
void TearoutProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree extra ("TE_EXTRA");
    extra.setProperty ("mute", (int) muteMask.load(), nullptr);
    extra.setProperty ("solo", (int) soloMask.load(), nullptr);
    extra.setProperty ("selected", selectedLayer.load(), nullptr);
    extra.setProperty ("engine", engineVersion, nullptr);
    extra.setProperty ("followLive", followLive.load(), nullptr);
    for (int l = 0; l < te::kNumLayers; ++l)
    {
        const auto& slot = sounds[(size_t) l];
        if (! slot.data) continue;
        ValueTree snd ("SOUND");
        snd.setProperty ("layer", l, nullptr);
        snd.setProperty ("file", slot.file.getFullPathName(), nullptr);
        snd.setProperty ("root", slot.root, nullptr);
        snd.setProperty ("rootInfo", slot.rootInfo, nullptr);
        snd.setProperty ("simplerMidi", slot.simplerMidi, nullptr);
        extra.appendChild (snd, nullptr);
    }
    if (seed.valid)
    {
        auto bytes = te::writeMidi ({ { "seed", seed.notes, 0 } }, 120.0);
        MemoryBlock mb (bytes.data(), bytes.size());
        extra.setProperty ("seedMidi", mb.toBase64Encoding(), nullptr);
        extra.setProperty ("seedSource", seedSource, nullptr);
    }
    state.appendChild (extra, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void TearoutProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    auto tree = ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return;
    auto extra = tree.getChildWithName ("TE_EXTRA");
    if (extra.isValid())
    {
        tree.removeChild (extra, nullptr);
        muteMask.store ((uint32_t) (int) extra.getProperty ("mute", 0));
        soloMask.store ((uint32_t) (int) extra.getProperty ("solo", 0));
        selectedLayer.store (jlimit (0, 7, (int) extra.getProperty ("selected", 1)));
        engineVersion = (int) extra.getProperty ("engine", 1);   // sets from 1.0 have no engine tag
        followLive.store ((bool) extra.getProperty ("followLive", true));
        for (int l = 0; l < te::kNumLayers; ++l) clearLayerSound (l);
        for (auto snd : extra)
        {
            if (! snd.hasType ("SOUND")) continue;
            const int l = (int) snd.getProperty ("layer", -1);
            const File f (snd.getProperty ("file").toString());
            if (! layerTakesSound (l) || ! f.existsAsFile()) continue;
            String msg;
            if (readSound (l, f, (int) snd.getProperty ("root", 60), snd.getProperty ("rootInfo").toString(), msg))
                sounds[(size_t) l].simplerMidi = (bool) snd.getProperty ("simplerMidi", true);
        }
        seed = te::SeedAnalysis {};
        seedSource = {};
        MemoryBlock mb;
        if (mb.fromBase64Encoding (extra.getProperty ("seedMidi").toString()) && mb.getSize() > 0)
        {
            std::vector<uint8_t> bytes ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
            std::vector<te::Note> notes;
            double bpm;
            std::string err;
            if (te::readMidi (bytes, notes, bpm, err))
            {
                seed = te::analyseSeed (notes);
                seedSource = extra.getProperty ("seedSource").toString();
            }
        }
    }
    apvts.replaceState (tree);
    dirty.store (true);
    if (MessageManager::getInstanceWithoutCreating() != nullptr
        && MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread())
        regenerateNow();
}

AudioProcessorEditor* TearoutProcessor::createEditor() { return new TearoutEditor (*this); }
} // namespace tep

#if ! defined(TE_SNAPSHOT)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new tep::TearoutProcessor(); }
#endif
