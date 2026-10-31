// TEAROUT ENGINE - preview synth and song player (no JUCE dependency, real-time safe after prepare)
#pragma once

#include "Engine.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <vector>

namespace te
{
// A user sample for one layer (pitched from its root note, like Simpler in Classic mode).
struct SampleData
{
    std::vector<float> left, right;
    double sampleRate = 44100.0;
    int rootNote = 60;
    int loopStart = -1, loopEnd = -1; // sample frames; loopEnd > loopStart = sustain loop
    float gain = 1.0f;
};

class PreviewSynth
{
public:
    static constexpr int kMaxVoices = 64;

    void prepare (double sampleRate);
    void noteOn (int layer, int pitch, int velocity);
    void noteOff (int layer, int pitch);
    void allNotesOff (bool hard);
    // Adds into L / R.
    void render (float* L, float* R, int numSamples);
    // Heavier preview: more distortion on basses, chords and kick (used by the CLI renders).
    void setHeavy (bool h) noexcept { heavy = h; }
    // Not real-time safe: call before playback starts.
    void setLayerSample (int layer, std::shared_ptr<const SampleData> s)
    {
        if (layer < 0 || layer >= kNumLayers) return;
        layerSampleOwner[(size_t) layer] = std::move (s);
        layerSample[(size_t) layer] = layerSampleOwner[(size_t) layer].get();
    }
    // Real-time safe: the caller keeps the data alive while it can be in use.
    void setLayerSampleRaw (int layer, const SampleData* s) noexcept { if (layer >= 0 && layer < kNumLayers) layerSample[(size_t) layer] = s; }
    void setLayerGain (int layer, float g) noexcept { if (layer >= 0 && layer < kNumLayers) layerGain[(size_t) layer] = g; }

private:
    struct Voice
    {
        bool active = false;
        bool gate = false;
        int layer = 0;
        int pitch = 0;
        float vel = 0;
        double phase[4] {};
        double freq = 0, target = 0;
        double t = 0;          // seconds since trigger
        float env = 0;
        int stage = 0;         // 0 attack, 1 decay/sustain, 2 release
        float fenv = 1;
        float ic1 = 0, ic2 = 0, jc1 = 0, jc2 = 0; // two SVF states
        float pan = 0;
        uint64_t age = 0;
        uint32_t noise = 22222;
        float lastOut = 0;
    };
    struct LayerCfg
    {
        bool mono;
        float glide, a, d, s, r;
        float cutoff, envAmt, fdecay, res, gain, drive;
        int wave; // 0 sine, 1 saw+square, 2 square, 3 supersaw, 4 saw+square lead, 5 saw pluck
    };

    static const LayerCfg& cfg (int layer);
    Voice* allocate (int layer);
    void renderVoice (Voice& v, float* L, float* R, int n);
    float noise (Voice& v);
    void svf (float x, float g, float k, float& ic1, float& ic2, float& lp, float& bp);

    double sr = 44100.0;
    bool heavy = false;
    std::array<std::shared_ptr<const SampleData>, kNumLayers> layerSampleOwner {};
    std::array<const SampleData*, kNumLayers> layerSample {};
    std::array<float, kNumLayers> layerGain { 1, 1, 1, 1, 1, 1, 1, 1 };
    std::array<Voice, kMaxVoices> voices {};
    uint64_t counter = 0;
    std::array<int, kNumLayers> monoVoice {};
    std::array<std::vector<int>, kNumLayers> monoStack {};
};

struct PlayEvent
{
    int tick;
    uint8_t layer, pitch, velocity;
    bool on;
};

struct PlayData
{
    std::vector<PlayEvent> events; // sorted: by tick, note-offs before note-ons
    int totalTicks = 0;
};

std::shared_ptr<const PlayData> makePlayData (const Song& song);

struct MidiOutEvent
{
    int sampleOffset;
    int layer;
    int pitch;
    int velocity;
    bool on;
};

class SongPlayer
{
public:
    SongPlayer() { scratch.reserve (4096); }
    // Convenience for offline use (CLI / tests). The plugin swaps PlayData itself.
    void setSong (std::shared_ptr<const Song> song) { data = makePlayData (*song); }
    void setData (std::shared_ptr<const PlayData> d) { data = std::move (d); }

    // Renders one block (clears L/R first). startTick is the timeline position of the first sample
    // (any value: it is wrapped onto the loop). midiOut, if given, receives events for layers in midiMask.
    void process (PreviewSynth& synth, float* L, float* R, int numSamples, double startTick, double ticksPerSample,
                  bool playing, uint32_t layerMask, std::vector<MidiOutEvent>* midiOut, uint32_t midiMask = 0);
    void process (PreviewSynth& synth, const PlayData* d, float* L, float* R, int numSamples, double startTick,
                  double ticksPerSample, bool playing, uint32_t layerMask, std::vector<MidiOutEvent>* midiOut,
                  uint32_t midiMask = 0);

    void releaseAll (PreviewSynth& synth, std::vector<MidiOutEvent>* midiOut, uint32_t midiMask, int offset);
    double lastPositionTicks() const noexcept { return lastPos; }

private:
    std::shared_ptr<const PlayData> data;
    const PlayData* lastData = nullptr;
    bool wasPlaying = false;
    double expected = -1;
    double lastPos = 0;
    std::array<std::bitset<128>, kNumLayers> heldSynth {}, heldMidi {};
    uint32_t lastLayerMask = 0xFFFFFFFFu, lastMidiMask = 0;
    void releaseMasked (PreviewSynth& synth, std::vector<MidiOutEvent>* midiOut, uint32_t layerMask, uint32_t midiMask);
    std::vector<MidiOutEvent> scratch;
};
} // namespace te
