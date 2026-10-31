// TEAROUT ENGINE - user sounds: root-note detection and sustain-loop preparation (no JUCE dependency)
#pragma once

#include "Synth.h"

#include <memory>
#include <string>
#include <vector>

namespace te
{
struct PitchGuess
{
    int midi = -1;          // detected MIDI note, -1 = none
    double hz = 0.0;
    double confidence = 0;  // 0..1
};

// Fundamental of a held, pitched sound (YIN on the steady part). Works down to ~20 Hz (sub / 808).
PitchGuess detectPitch (const std::vector<float>& mono, double sampleRate);

// Note name in a file name: "Growl 808 - C", "bass_F#1", "Sub Db0", "reese (G#)". Returns pitch class
// (0-11) or -1; octave is set when the name has one (Ableton numbering: C3 = 60), else -99.
int noteFromFileName (const std::string& fileName, int& octave);

struct RootChoice
{
    int midi = 60;
    bool detected = false;    // true = from the audio, false = from the name / default
    std::string reason;       // short human explanation for the UI
};
RootChoice chooseRoot (const std::vector<float>& mono, double sampleRate, const std::string& fileName);

// Builds playable sample data: finds a crossfaded sustain loop over the steady part, levels the sound.
std::shared_ptr<SampleData> makeSampleData (std::vector<float> left, std::vector<float> right, double sampleRate, int rootNote);
} // namespace te
