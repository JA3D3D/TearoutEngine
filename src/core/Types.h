// TEAROUT ENGINE - core types (no JUCE dependency)
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace te
{
// ---- time base -------------------------------------------------------------------------------
constexpr int PPQ    = 96;          // ticks per quarter note
constexpr int STEP   = PPQ / 4;     // 1/16  = 24
constexpr int EIGHTH = PPQ / 2;     // 1/8   = 48
constexpr int TRIP8  = PPQ / 3;     // 1/8T  = 32
constexpr int TRIP16 = PPQ / 6;     // 1/16T = 16
constexpr int T32    = PPQ / 8;     // 1/32  = 12
constexpr int BEAT   = PPQ;
constexpr int BAR    = PPQ * 4;     // 4/4 only
constexpr int GLIDE_OVERLAP = T32;  // overlap that makes a mono/legato synth slide

struct Note
{
    int pitch    = 60;
    int start    = 0;   // ticks
    int length   = STEP;
    int velocity = 100;

    int end() const noexcept { return start + length; }
    bool operator== (const Note& o) const noexcept
    {
        return pitch == o.pitch && start == o.start && length == o.length && velocity == o.velocity;
    }
};

enum class Mode : int { Minor = 0, Major = 1 };

enum class Genre : int
{
    MelodicDubstep = 0,
    Tearout,
    Riddim,
    Trap,
    Hardstyle,
    Common,
    Count
};
constexpr int kNumGenres = (int) Genre::Count;

enum class Layer : int
{
    Chords = 0,
    Sub,
    BassA,
    BassB,
    Lead,
    Arp,
    Drums,
    FX,
    Count
};
constexpr int kNumLayers = (int) Layer::Count;

inline int pc (int pitch) noexcept { return ((pitch % 12) + 12) % 12; }

// Drum Rack / GM-ish note map (Ableton Drum Rack: C1 = 36)
namespace drum
{
constexpr int Kick = 36, Rim = 37, Snare = 38, Clap = 39, ClosedHat = 42, Tom = 45, OpenHat = 46, Crash = 49;
}
// FX pad map (C2 = 48 upwards)
namespace fx
{
constexpr int Impact = 48, Downlifter = 50, Riser = 52, ReverseCrash = 53, NoiseSweep = 55;
}
} // namespace te
