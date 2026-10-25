#include "Catalogue.h"

namespace te
{
using namespace pat;

// ---- patterns ------------------------------------------------------------------------------------
const std::vector<PatternInfo>& patternsFor (Layer layer)
{
    static const std::vector<PatternInfo> chords {
        { "Off", "No chords in this drop." },
        { "Hold", "Each chord held for its full length. Supersaw / pad style." },
        { "3-3-2 Stabs", "Chord re-struck in a 3+3+2 eighth rhythm, no gaps. Future-bass / melodic drop feel." },
        { "1/8 Pulse", "Chord on every 1/8. Use LENGTH to make it choppy." },
        { "Offbeat 1/8", "Chord on the offbeats between kicks. Hardstyle / house." },
        { "Half-time Stabs", "Sparse dark stabs on 1, the and-of-2 and 3-and. Tearout chords." },
        { "From Seed", "Re-strikes the chords on your seed clip's chord rhythm.", true },
    };
    static const std::vector<PatternInfo> sub {
        { "Off", "No sub." },
        { "Hold", "One long note per chord on the root. The classic sustained sub." },
        { "Hold + Kick", "Root re-struck on every kick, held until the next one. 808 / riddim sub." },
        { "Follow Bass A", "Plays BASS A's exact rhythm, on the root, one octave under." },
        { "Offbeat 1/8", "Root on every offbeat 1/8, between the kicks. Hardstyle reverse bass." },
        { "1/4 Pulse", "Root on every beat, no gaps." },
    };
    static const std::vector<PatternInfo> bassA {
        { "Off", "No main mid-bass." },
        { "Long Hold", "One long sustain per chord. Movement re-strikes it in the groove." },
        { "Hold + Restrike", "Hold, re-struck on the snare (beat 3). Solid quarter-note weight." },
        { "3-3-2 Ride", "Sustain pattern 3+3+2 eighths, no gaps. The riding tearout feel." },
        { "4-2-2 Ride", "Half-bar hold then two 1/8s, no gaps." },
        { "1/8 Pump", "Continuous eighth notes on the root." },
        { "Offbeat 1/8", "Offbeat eighths between the kicks. Hardstyle / house bass." },
        { "Triplet Bounce", "Long-short triplet bounce, no gaps. Riddim swing." },
        { "From Seed", "Your seed clip's bass rhythm, gaps filled, on the current chords.", true },
    };
    static const std::vector<PatternInfo> bassB {
        { "Off", "BASS A plays the whole line on one sound." },
        { "Double A", "Same notes as BASS A, for stacking a second sound." },
        { "Answers", "Takes beats 3-4 of bars 2 and 4: call-and-response between two sounds." },
        { "Swap Every Hit", "A and B alternate on every note: the bass hops between patches." },
        { "Restrikes", "A keeps the long downbeat holds, B takes every re-strike." },
        { "Half & Half", "A plays beats 1-2, B plays beats 3-4 of every bar." },
    };
    static const std::vector<PatternInfo> lead {
        { "Off", "No lead." },
        { "Hook", "Short, singable 2-bar hook, repeated and answered. Chord tones on strong beats." },
        { "Long Notes", "Big held melody notes on chord tones. Supersaw lead." },
        { "1/8 Riff", "Driving eighth-note riff on chord tones. Hardstyle lead." },
        { "Call & Response", "A short call, then a reply at the end of the next bar." },
        { "From Seed", "Your seed clip's top-line rhythm and contour, fitted to the current chords.", true },
    };
    static const std::vector<PatternInfo> arp {
        { "Off", "No arp." },
        { "Up 1/16", "Chord tones upward in 1/16ths, restarting on every chord." },
        { "Down 1/16", "Chord tones downward in 1/16ths." },
        { "Up-Down 1/16", "Up then back down in 1/16ths." },
        { "Up 2 Oct 1/16", "Upward across two octaves in 1/16ths." },
        { "Up 1/8", "Upward in 1/8ths. Sparse trap arp." },
        { "Up 1/16T", "Upward in 1/16 triplets." },
        { "3-3-2 Pluck", "Plucks grouped 3+3+2 sixteenths. Future-bass pluck rhythm." },
    };
    static const std::vector<PatternInfo> drums {
        { "Off", "No drums." },
        { "Half-time", "Kick on 1, snare on 3, 1/8 hats. Dubstep / riddim." },
        { "Half-time Heavy", "Half-time with syncopated kicks and clap-stacked snare. Tearout." },
        { "Trap", "Kick on 1 and the a-of-3, clap on 3, 1/8 hats with rolls." },
        { "Four-on-Floor", "Kick every beat, clap on 2 and 4, offbeat open hats." },
        { "Hardstyle", "Kick every beat, clap on 2 and 4, offbeat hats." },
        { "Backbeat", "Kick 1 and 3, snare 2 and 4. Pop / common." },
    };
    static const std::vector<PatternInfo> fxp {
        { "Off", "No FX triggers." },
        { "Impacts", "Impact on the first downbeat and every 8 bars." },
        { "Impacts + Risers", "Impacts, plus a riser and reverse crash into every 8-bar boundary." },
        { "Sweeps", "Downlifter on the drop, noise sweeps into every 4-bar phrase." },
    };
    switch (layer)
    {
        case Layer::Chords: return chords;
        case Layer::Sub:    return sub;
        case Layer::BassA:  return bassA;
        case Layer::BassB:  return bassB;
        case Layer::Lead:   return lead;
        case Layer::Arp:    return arp;
        case Layer::Drums:  return drums;
        case Layer::FX:     return fxp;
        case Layer::Count:  break;
    }
    return chords;
}

std::vector<std::string> patternChoiceNames (Layer layer)
{
    std::vector<std::string> out { "Genre" };
    for (auto& p : patternsFor (layer))
        out.push_back (p.name);
    return out;
}

const char* layerName (Layer layer)
{
    static const char* names[] = { "CHORDS", "SUB", "BASS A", "BASS B", "LEAD", "ARP", "DRUMS", "FX" };
    return names[(int) layer];
}

const char* layerHelp (Layer layer)
{
    static const char* help[] = {
        "The progression itself: holds, stabs or offbeat chords.",
        "Sine sub / 808 on the chord root. Long holds, or locked to the kick.",
        "Main mid-bass (growl / reese). Sustain or gapless sustain pattern on the root.",
        "Second bass sound. Doubles A, or takes turns with A so A + B make one continuous line.",
        "The hook: chord tones on strong beats, repeated and answered.",
        "Arpeggiated chord tones (plucks).",
        "Drum Rack notes: C1 kick, D1 snare, D#1 clap, F#1 hat, A#1 open hat, C#2 crash.",
        "FX pads: C2 impact, D2 downlifter, E2 riser, F2 reverse crash, G2 noise sweep.",
    };
    return help[(int) layer];
}

bool layerUsesOctave (Layer layer) { return layer != Layer::Drums && layer != Layer::FX; }
bool layerUsesLength (Layer layer) { return layer != Layer::Drums && layer != Layer::FX; }

float defaultLength (Layer layer)
{
    switch (layer)
    {
        case Layer::Lead: return 0.9f;
        case Layer::Arp:  return 0.55f;
        default:          return 1.0f;
    }
}

// ---- progressions --------------------------------------------------------------------------------
const std::vector<ProgressionDef>& progressionCatalogue()
{
    static const std::vector<ProgressionDef> cat {
        { "i VI III VII",          Mode::Minor, "Emotional" },        // 0
        { "VI VII i i",            Mode::Minor, "Lift" },             // 1
        { "i VII VI VII",          Mode::Minor, "Trance" },           // 2
        { "i i VI VII",            Mode::Minor, "Rising" },           // 3
        { "i VI VII i",            Mode::Minor, "Anthem" },           // 4
        { "i iv VI VII",           Mode::Minor, "Hardstyle" },        // 5
        { "i VI iv V",             Mode::Minor, "Drama (harmonic V)" },// 6
        { "i VII VI V",            Mode::Minor, "Andalusian" },       // 7
        { "i VII VI III",          Mode::Minor, "Dark descent" },     // 8
        { "i VI III v",            Mode::Minor, "Moody" },            // 9
        { "i VII VI v",            Mode::Minor, "Minor descent" },    // 10
        { "iv i VII VII",          Mode::Minor, "Trap turn" },        // 11
        { "i iv v i",              Mode::Minor, "Minor classic" },    // 12
        { "i VI i VI",             Mode::Minor, "Two-chord" },        // 13
        { "i isus2 i isus2",       Mode::Minor, "One-chord sus" },    // 14
        { "i i i i",               Mode::Minor, "Pedal (one chord)" },// 15
        { "i i i bII",             Mode::Minor, "Phrygian push" },    // 16
        { "i bII i bII",           Mode::Minor, "Phrygian swing" },   // 17
        { "i VII i VII",           Mode::Minor, "Two-chord VII" },    // 18
        { "i iv i iv",             Mode::Minor, "Two-chord iv" },     // 19
        { "I V vi IV",             Mode::Major, "Pop axis" },         // 20
        { "vi IV I V",             Mode::Major, "Axis (minor start)" },// 21
        { "IV I V vi",             Mode::Major, "Lift" },             // 22
        { "I iii vi IV",           Mode::Major, "Bright" },           // 23
        { "Imaj7 iii7 vi7 IVmaj7", Mode::Major, "Lush 7ths" },        // 24
        { "I vi IV V",             Mode::Major, "Classic" },          // 25
        { "IV V vi vi",            Mode::Major, "Climb" },            // 26
        { "vi V IV V",             Mode::Major, "Turnaround" },       // 27
    };
    return cat;
}

std::string prettyNumerals (const char* numerals)
{
    std::string s (numerals), out;
    for (char c : s)
    {
        if (c == ' ') out += "\xE2\x80\x93"; // en dash
        else out += c;
    }
    return out;
}

std::vector<std::string> progressionChoiceNames()
{
    std::vector<std::string> out { "Auto (genre pick)", "From Seed MIDI" };
    for (auto& p : progressionCatalogue())
        out.push_back (prettyNumerals (p.numerals) + "  \xC2\xB7  " + p.feel);
    return out;
}

// ---- genres --------------------------------------------------------------------------------------
const GenreProfile& genreProfile (Genre g)
{
    static const std::vector<std::vector<int>> halfTimeGroove {
        { 192 }, { 144, 288 }, { 72, 336 }, { 240, 96 }, { 48, 264, 360, 168 } };
    static const std::vector<std::vector<int>> riddimGroove {
        { 192 }, { 128, 320 }, { 64, 256 }, { 352, 96 }, { 160, 288, 224, 32 } };
    static const std::vector<std::vector<int>> trapGroove {
        { 240 }, { 168, 336 }, { 72, 144 }, { 264, 360 }, { 192, 96, 48 } };
    static const std::vector<std::vector<int>> hardGroove {
        { 72, 168, 264, 360 }, { 192 }, { 96, 288 }, { 48, 144, 240, 336 } };
    static const std::vector<std::vector<int>> commonGroove {
        { 192 }, { 96, 288 }, { 144, 336 }, { 240, 48 }, { 72, 168, 264, 360 } };

    static const GenreProfile profiles[] = {
        { "Melodic Dubstep", "MELODIC", 150.0, Voicing::Lush, true, { 53, 77 }, { 65, 84 }, 72, 26, 38,
          { ChHold, SubHold, BassHoldRestrike, BDouble, LeadHook, ArpUp16, DrHalfTime, FxImpacts },
          halfTimeGroove,
          { { 0, 3 }, { 1, 2 }, { 2, 1.5 }, { 3, 1.5 }, { 21, 2 }, { 22, 1.5 }, { 24, 1 }, { 26, 1 } },
          { 0, 2, 3 },
          "150 half-time. Emotional minor loops, lush held chords, sustained sub, doubled mid-bass, hook + 1/16 arp." },
        { "Tearout Dubstep", "TEAROUT", 150.0, Voicing::Triad, false, { 48, 67 }, { 72, 88 }, 72, 26, 38,
          { ChOff, SubHold, BassRide332, BAnswers, LeadOff, ArpOff, DrHalfTimeHeavy, FxImpacts },
          halfTimeGroove,
          { { 15, 4 }, { 16, 3 }, { 17, 1.5 }, { 13, 1.5 }, { 18, 1 }, { 19, 1 } },
          { 2, 4 },
          "150 half-time. Root-centred pedal or Phrygian push, long sub, 3-3-2 sustain ride, A/B call-and-response." },
        { "Riddim", "RIDDIM", 150.0, Voicing::Triad, false, { 48, 67 }, { 72, 88 }, 72, 26, 38,
          { ChOff, SubHoldKick, BassTriplet, BSwap, LeadOff, ArpOff, DrHalfTime, FxImpacts },
          riddimGroove,
          { { 15, 4 }, { 16, 2.5 }, { 17, 1.5 }, { 13, 1 } },
          { 2, 4 },
          "150 half-time. One-note repetition, triplet bounce, sub on the kick, A/B swapping every hit." },
        { "Trap", "TRAP", 140.0, Voicing::Seventh, false, { 55, 76 }, { 63, 82 }, 72, 26, 38,
          { ChHold, SubHoldKick, BassOff, BOff, LeadHook, ArpUp8, DrTrap, FxImpacts },
          trapGroove,
          { { 13, 2 }, { 14, 1.5 }, { 11, 1.5 }, { 9, 1.5 }, { 8, 1.5 }, { 7, 1.5 }, { 10, 1 }, { 0, 1 } },
          { 4, 1 },
          "140 straight. 808 on every kick, sustained 7th chords, sparse 1/8 arp, hats with rolls." },
        { "Hardstyle Techno", "HARDSTYLE", 150.0, Voicing::OctaveTriad, true, { 52, 76 }, { 67, 86 }, 69, 26, 38,
          { ChOffbeat8, SubOffbeat8, BassOffbeat8, BOff, LeadRiff8, ArpUp16, DrHardstyle, FxImpactsRisers },
          hardGroove,
          { { 2, 2 }, { 1, 1.5 }, { 7, 1.5 }, { 5, 1.5 }, { 6, 1.5 }, { 4, 1.5 }, { 16, 1 } },
          { 0, 1 },
          "150 four-on-the-floor. Offbeat reverse bass and offbeat chords on the roots, 1/8 riff lead, 1/16 arp." },
        { "Common", "COMMON", 128.0, Voicing::Triad, false, { 55, 74 }, { 64, 82 }, 67, 26, 38,
          { ChHold, SubHold, BassPump8, BOff, LeadHook, ArpUpDown16, DrBackbeat, FxImpacts },
          commonGroove,
          { { 20, 3 }, { 21, 2 }, { 22, 2 }, { 23, 1.5 }, { 25, 2 }, { 27, 1 }, { 0, 1.5 }, { 12, 1 } },
          { 0, 1, 3 },
          "The progressions in every song: I-V-vi-IV and friends, held chords, 1/8 bass, hook, up-down arp." },
    };
    return profiles[(int) g];
}

std::vector<std::string> genreChoiceNames()
{
    std::vector<std::string> out;
    for (int g = 0; g < kNumGenres; ++g)
        out.push_back (genreProfile ((Genre) g).name);
    return out;
}

std::vector<std::string> keyChoiceNames()
{
    return { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
}

// ---- lead rhythm templates ---------------------------------------------------------------------
const std::vector<std::vector<Hit>>& leadTemplates (int leadPattern)
{
    static const std::vector<std::vector<Hit>> hook {
        { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 4 }, { 12, 4 }, { 16, 3 }, { 19, 3 }, { 22, 2 }, { 24, 8 } },
        { { 0, 2 }, { 2, 2 }, { 4, 4 }, { 8, 2 }, { 10, 2 }, { 12, 4 }, { 16, 2 }, { 18, 2 }, { 20, 4 }, { 24, 4 }, { 28, 4 } },
        { { 0, 4 }, { 6, 2 }, { 8, 2 }, { 10, 4 }, { 14, 2 }, { 16, 4 }, { 22, 2 }, { 24, 8 } },
        { { 0, 6 }, { 6, 6 }, { 12, 4 }, { 16, 6 }, { 22, 2 }, { 24, 8 } },
        { { 0, 2 }, { 3, 1 }, { 4, 2 }, { 6, 2 }, { 8, 4 }, { 16, 2 }, { 19, 1 }, { 20, 2 }, { 22, 2 }, { 24, 4 }, { 28, 2 } },
    };
    static const std::vector<std::vector<Hit>> longNotes {
        { { 0, 8 }, { 8, 8 }, { 16, 12 }, { 28, 4 } },
        { { 0, 12 }, { 12, 4 }, { 16, 16 } },
        { { 0, 6 }, { 6, 10 }, { 16, 6 }, { 22, 10 } },
    };
    static const std::vector<std::vector<Hit>> riff {
        { { 0, 2 }, { 2, 2 }, { 4, 4 }, { 8, 2 }, { 10, 2 }, { 12, 2 }, { 14, 2 }, { 16, 2 }, { 18, 2 }, { 20, 4 }, { 24, 2 }, { 26, 2 }, { 28, 4 } },
        { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 4 }, { 16, 2 }, { 18, 2 }, { 20, 2 }, { 22, 2 }, { 24, 8 } },
    };
    static const std::vector<std::vector<Hit>> callResponse {
        { { 0, 2 }, { 2, 2 }, { 4, 4 }, { 8, 4 }, { 24, 2 }, { 26, 2 }, { 28, 4 } },
        { { 0, 3 }, { 3, 3 }, { 6, 6 }, { 20, 2 }, { 22, 2 }, { 24, 8 } },
    };
    switch (leadPattern)
    {
        case LeadLong:         return longNotes;
        case LeadRiff8:        return riff;
        case LeadCallResponse: return callResponse;
        default:               return hook;
    }
}
} // namespace te
