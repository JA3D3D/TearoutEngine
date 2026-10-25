// Standard MIDI File read/write (no JUCE dependency)
#pragma once

#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace te
{
struct MidiTrackOut
{
    std::string name;
    std::vector<Note> notes;
    int channel = 0; // 0-15
};

// Type 1 file: a tempo/conductor track, then one track per entry. 96 ticks per quarter.
std::vector<uint8_t> writeMidi (const std::vector<MidiTrackOut>& tracks, double bpm);

// Reads every track, merges all notes (channel 10 skipped when skipDrums), converts to 96 PPQ.
bool readMidi (const std::vector<uint8_t>& data, std::vector<Note>& notes, double& bpm, std::string& error,
               bool skipDrums = true);
} // namespace te
