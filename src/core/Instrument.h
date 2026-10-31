// TEAROUT ENGINE - "DRAG INSTRUMENT": a layer's sound as an Ableton Simpler preset (no JUCE dependency)
#pragma once

#include "Sample.h"
#include "Types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace te
{
struct InstrumentSpec
{
    std::string userName;      // device title in Live, e.g. "TEAROUT BASS A"
    std::string sampleName;    // sample part name
    std::string wavPath;       // absolute path of the exported WAV
    uint64_t fileSize = 0;
    int frames = 0;
    double sampleRate = 44100.0;
    int rootNote = 60;
    int loopStart = -1, loopEnd = -1;
    int voicesIndex = 5;       // Simpler voice menu index (0 = 1 voice, 5 = 6, 7 = 8)
    bool glide = false;        // mono legato glide (like the preview's basses and lead)
    double glideMs = 50.0;
    double attackMs = 0.1, releaseMs = 50.0;
    double volumeDb = -6.0;
};

// Simpler behaviour that matches the preview for this layer (voices, glide, envelope).
// userSample: the layer plays a loaded sound (2 ms attack / short release in the preview); otherwise it is the
// rendered built-in voice, whose attack and decay are already in the audio.
InstrumentSpec instrumentDefaults (Layer layer, bool userSample = false);
// The register a layer's sound is rendered / rooted at.
int instrumentRoot (Layer layer);
// Simpler preset XML (uncompressed). Live expects it gzip-compressed in an .adv file.
std::string makeSimplerPresetXml (const InstrumentSpec& spec);
// The audio to write for an instrument: the preview's level (sample gain applied for user sounds) normalised to
// -1 dBFS, plus the Simpler volume that puts it back at the preview's balance.
struct InstrumentAudio
{
    std::vector<float> left, right;
    double volumeDb = -6.0;
};
InstrumentAudio instrumentAudio (const SampleData& data, bool applySampleGain);
// The built-in preview voice of a layer, rendered as a playable sample (used when no sound is loaded).
std::shared_ptr<SampleData> renderLayerVoice (Layer layer, double sampleRate);
} // namespace te
