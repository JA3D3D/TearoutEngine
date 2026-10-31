// TEAROUT ENGINE - harmony-first, layer-based drop generator
#pragma once

#include "Catalogue.h"
#include "Theory.h"
#include "Types.h"

#include <array>
#include <string>
#include <vector>

namespace te
{
struct ChordEvent
{
    int start = 0;
    int length = BAR;
    Chord chord;
    std::vector<int> voicing; // concrete chord-layer pitches (filled by the engine)
    int subPitch = -1;        // voice-led sub / 808 pitch for this chord (filled by the engine)
    int bassPitch = -1;       // mid-bass pitch (sub + 12)
    int end() const noexcept { return start + length; }
};

// What a seed MIDI clip told us. Everything is loop-relative (tick 0 = first bar of the clip).
struct SeedAnalysis
{
    bool valid = false;
    int bars = 0;                 // loop length in bars (1, 2, 4, 8 or 16)
    int keyRoot = 0;
    Mode mode = Mode::Minor;
    std::vector<ChordEvent> chords;
    std::vector<Note> bassLine;   // lowest line, monophonic, gapless
    std::vector<Note> topLine;    // highest line, monophonic
    std::vector<int> chordOnsets; // ticks where the seed strikes chords
    std::vector<Note> notes;      // quantised source notes
    std::string summary;          // "F minor · Fm–Db–Ab–Eb · 4 bars"
};

struct LayerSettings
{
    int pattern = 0;       // parameter choice index: 0 = genre default, n = pattern id (n - 1)
    int octave = 0;        // -2 .. +2
    float movement = 0.3f; // 0 .. 1 : groove only (re-strikes, flicks, approach notes, slides)
    float length = 1.0f;   // 0.25 .. 1 : note length relative to its slot (1 = legato, no gaps)
};

struct Settings
{
    Genre genre = Genre::MelodicDubstep;
    int keyRoot = 5;          // F
    int progression = kProgressionAuto;
    int chordLength = 1;      // 0 = 1/2 bar, 1 = 1 bar, 2 = 2 bars
    int bars = 8;             // 4, 8, 16, 32
    uint32_t songSeed = 1;    // GENERATE: progression pick (Auto), hook contour, arp choices
    uint32_t grooveSeed = 1;  // VARIATION: where the movement goes
    bool patternPools = true; // "Genre" layers pick from the genre's pattern pool by song seed (1.1); false = fixed defaults (1.0 sets)
    std::array<LayerSettings, kNumLayers> layers {};

    Settings();
};

struct Song
{
    int bars = 0;
    int totalTicks = 0;
    int loopBars = 0;
    int keyRoot = 0;
    Mode mode = Mode::Minor;
    bool flats = false;
    double bpm = 140.0;
    std::string progressionLabel;            // "i–VI–III–VII"
    std::string chordNames;                  // "Fm  Db  Ab  Eb"
    std::vector<ChordEvent> chords;          // whole timeline
    std::array<std::vector<Note>, kNumLayers> layers;
    std::array<int, kNumLayers> pattern {};  // resolved pattern id per layer
    std::vector<int> kicks;                  // drum skeleton kick ticks (whole timeline)
    std::vector<std::string> hints;          // human-readable notes about the result

    const ChordEvent* chordAt (int tick) const noexcept;
};

Song generate (const Settings& settings, const SeedAnalysis* seed = nullptr);

int barsChoiceToBars (int choiceIndex) noexcept;     // 0..3 -> 4, 8, 16, 32
int chordLengthTicks (int choiceIndex) noexcept;     // 0..2 -> 1/2, 1, 2 bars
int resolvePattern (const Settings& s, Layer layer) noexcept;

// Seed analysis (Seed.cpp)
SeedAnalysis analyseSeed (const std::vector<Note>& notes);
} // namespace te
