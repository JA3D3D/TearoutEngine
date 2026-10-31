// TEAROUT ENGINE - audio processor: parameters, generation, preview playback, capture, MIDI out
#pragma once

#include "../core/Engine.h"
#include "../core/Instrument.h"
#include "../core/MidiFile.h"
#include "../core/Sample.h"
#include "../core/Synth.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>

namespace tep
{
namespace ids
{
inline const juce::String genre = "genre", key = "key", progression = "progression", chordLength = "chordLength",
                          bars = "bars", songSeed = "songSeed", grooveSeed = "grooveSeed", volume = "volume",
                          midiOut = "midiOut";
inline juce::String layer (int l, const char* what) { return "l" + juce::String (l) + "_" + what; }
} // namespace ids

class TearoutProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer
{
public:
    TearoutProcessor();
    ~TearoutProcessor() override;

    // ---- AudioProcessor -------------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TEAROUT ENGINE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- engine access (message thread) -------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    std::shared_ptr<const te::Song> getSong() const { return song; }
    te::Settings currentSettings() const;
    void regenerateNow();
    void generateNew();   // GENERATE: new song seed (progression on Auto, hook, arp start)
    void newVariation();  // VARIATION: new groove seed (movement only)

    // seed MIDI
    bool loadSeedFile (const juce::File& file, juce::String& message);
    void loadSeedNotes (const std::vector<te::Note>& notes, const juce::String& sourceName);
    void clearSeed();
    const te::SeedAnalysis& getSeed() const { return seed; }
    juce::String getSeedSource() const { return seedSource; }

    // capture from MIDI input (put the clip on this track and play it)
    void setCapture (bool shouldCapture);
    bool isCapturing() const { return capturing.load(); }
    int capturedNoteCount() const { return (int) captured.size(); }

    // ---- sound slots: your own sample per pitched layer (1.1) -----------------------------------------
    struct SoundSlot
    {
        juce::File file;              // our copy, kept in the TEAROUT ENGINE sounds folder
        int root = 60;                // MIDI note the sample was recorded at
        juce::String rootInfo;        // "detected C0 (32.7 Hz)"
        std::shared_ptr<const te::SampleData> data;
    };
    static bool layerTakesSound (int layer);
    static bool isAudioFile (const juce::File& f);
    static juce::File soundsFolder();
    bool loadLayerSound (int layer, const juce::File& file, juce::String& message);
    void clearLayerSound (int layer);
    void nudgeRoot (int layer, int semitones);
    const SoundSlot& getSound (int layer) const { return sounds[(size_t) juce::jlimit (0, te::kNumLayers - 1, layer)]; }

    // ---- DRAG INSTRUMENT (1.2): a layer's sound as an Ableton Simpler preset ---------------------------------
    // The loaded sound (or the built-in voice, rendered) is written as a WAV with its root and sustain loop, plus an
    // .adv Simpler preset with the preview's voices / glide / envelope. Dragged MIDI is always at true pitch, so it
    // plays in tune on that Simpler (and on Serum or any other synth).
    struct InstrumentFiles { juce::File preset, wav; juce::String message; };
    InstrumentFiles exportInstrument (int layer) const;   // message thread
    static juce::File instrumentsFolder();

    // ---- playback ----------------------------------------------------------------------------------------
    // PLAY is the master switch for the preview sound. With FOLLOW LIVE on it locks to Live's transport
    // while Live plays; otherwise it runs its own loop.
    void setFollowLive (bool on) { followLive.store (on); }
    bool isFollowingLive() const { return followLive.load(); }
    void setPreviewPlaying (bool on) { previewOn.store (on); }
    bool isPreviewPlaying() const { return previewOn.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    bool isRunning() const { return runningNow.load(); }
    double playheadTicks() const { return playTick.load(); }
    double currentBpm() const { return bpmNow.load(); }

    // per-layer mute / solo for the preview (saved with the plugin state)
    void setMute (int layer, bool m);
    void setSolo (int layer, bool s);
    bool isMuted (int layer) const { return (muteMask.load() >> layer) & 1u; }
    bool isSoloed (int layer) const { return (soloMask.load() >> layer) & 1u; }
    std::atomic<int> selectedLayer { 1 }; // UI state (also picks the MIDI OUT layer in "Selected Layer" mode)

    // MIDI export
    juce::File writeLayerMidi (int layer) const;   // temp file for drag & drop (layer -1 = all layers)
    bool saveAllMidi (const juce::File& folder, juce::String& message) const;
    juce::String fileStem() const;

    std::function<void()> onSongChanged; // called on the message thread after every regeneration

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void publish (std::shared_ptr<const te::Song> s);
    uint32_t audibleMask() const;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::shared_ptr<const te::Song> song;
    std::atomic<const te::PlayData*> playData { nullptr };
    struct Retired { std::shared_ptr<const te::PlayData> data; juce::uint32 time; };
    std::vector<Retired> keepAlive;
    std::atomic<bool> dirty { true };

    te::SeedAnalysis seed;
    juce::String seedSource;

    te::PreviewSynth synth;
    te::SongPlayer player;
    std::vector<te::MidiOutEvent> midiScratch;
    juce::AudioBuffer<float> renderBuffer;
    double sampleRate = 44100.0;
    double internalTick = 0;
    std::atomic<bool> previewOn { false }, hostPlaying { false }, runningNow { false };
    std::atomic<double> playTick { 0 }, bpmNow { 140.0 }, songBpm { 150.0 };
    bool timerSawPlaying = false;
    std::atomic<uint32_t> muteMask { 0 }, soloMask { 0 };
    int lastMidiOutMode = 0;

    // sound slots
    juce::AudioFormatManager formats;
    std::array<SoundSlot, te::kNumLayers> sounds;
    std::array<std::atomic<const te::SampleData*>, te::kNumLayers> soundPtr;
    struct RetiredSound { std::shared_ptr<const te::SampleData> data; juce::uint32 time; };
    std::vector<RetiredSound> soundKeepAlive;
    void publishSound (int layer, std::shared_ptr<const te::SampleData> data);
    bool readSound (int layer, const juce::File& file, int forcedRoot, const juce::String& forcedInfo, juce::String& message);
    std::atomic<bool> followLive { true };
    int engineVersion = 2; // 1 = a set saved by 1.0 (fixed genre patterns until GENERATE is pressed)

    // capture
    struct CapEvent { double tick; int pitch; int velocity; bool on; };
    std::atomic<bool> capturing { false };
    juce::AbstractFifo capFifo { 4096 };
    std::vector<CapEvent> capBuffer = std::vector<CapEvent> (4096);
    double captureClock = 0;
    bool captureClockStarted = false;
    std::vector<te::Note> captured;
    std::map<int, std::pair<double, int>> captureOpen;
    void drainCapture();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TearoutProcessor)
};
} // namespace tep
