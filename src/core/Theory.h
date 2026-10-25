// TEAROUT ENGINE - music theory: scales, chords, roman numerals, voicing
#pragma once

#include "Types.h"

#include <string>
#include <vector>

namespace te
{
struct Chord
{
    int root = 0;                 // absolute pitch class 0..11
    std::vector<int> intervals { 0, 3, 7 }; // semitones above root, ascending; may exceed 11 (add9 = 14)
    int bass = -1;                // slash-chord bass pitch class, -1 = root

    std::vector<int> pitchClasses() const;    // unique, ascending from root order
    bool contains (int pitchClass) const;
    int bassPc() const noexcept { return bass >= 0 ? bass : root; }
    bool hasMinorThird() const;
    bool isPlainTriad() const;               // exactly {0,3,7} / {0,4,7} / {0,3,6}
    std::string suffix() const;               // "", "m", "dim", "sus2", "7", "m7", "maj7", "add9", ...
    std::string name (bool flats) const;      // "F#m7"
    bool sameAs (const Chord& o) const;
};

const std::vector<int>& scaleSteps (Mode mode);               // 7 semitone offsets
std::vector<int> keyScale (int keyRoot, Mode mode);           // 7 absolute pitch classes
// Scale tones that work over this chord: the key scale, with chord tones added and any scale tone
// a semitone away from a non-diatonic chord tone removed. Ascending absolute pitch classes.
std::vector<int> chordScale (const Chord& chord, int keyRoot, Mode mode);

bool useFlats (int keyRoot, Mode mode);
std::string noteName (int pitchClass, bool flats);
std::string keyName (int keyRoot, Mode mode);     // "F minor"
std::string pitchName (int midiPitch, bool flats); // "F1" using Ableton naming (C3 = 60)

// Roman numeral token -> chord in key. Accepts: i, VI, bII, #iv, isus2, Isus4, i5, V7, i7, Imaj7, vi7,
// iadd9, I9, vii° / viio / viidim. Flats/sharps are measured from the MAJOR scale (so bVI = 8 in any mode),
// plain numerals from the given mode's scale (so VI in minor = 8, vi in major = 9).
bool parseNumeral (const std::string& token, int keyRoot, Mode mode, Chord& out);
// Whitespace-separated list of numerals.
bool parseProgression (const std::string& numerals, int keyRoot, Mode mode, std::vector<Chord>& out);

enum class Voicing : int { Triad = 0, Lush, Seventh, OctaveTriad };
// Adds genre colour to plain triads only (explicit 7ths / sus chords are left alone).
Chord colourChord (const Chord& c, Voicing v, int keyRoot, Mode mode);

int placeInRange (int pitchClass, int lo) noexcept;         // lowest pitch >= lo with this pitch class
int nearestWithPc (int pitchClass, int target) noexcept;     // pitch with this pc closest to target
int nearestInSet (const std::vector<int>& pcs, int target, int bias = 0) noexcept; // closest pitch whose pc is in pcs
int stepInSet (const std::vector<int>& pcs, int from, int steps) noexcept; // move N set-steps up/down from pitch

// Close-position voicing of the given pitch classes inside [lo, hi], led smoothly from prev.
std::vector<int> voiceChord (const std::vector<int>& pcs, int rootPc, int lo, int hi,
                             const std::vector<int>& prev, int maxNotes);
} // namespace te
