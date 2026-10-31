// TEAROUT ENGINE engine tests: music theory, every genre x progression x pattern, seeds, MIDI I/O, player.
#include "../src/core/Engine.h"
#include "../src/core/MidiFile.h"
#include "../src/core/Synth.h"
#include "../src/core/Sample.h"
#include "../src/core/Instrument.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>

using namespace te;
using namespace te::pat;

static int g_checks = 0, g_fail = 0;
static std::map<std::string, int> g_failures;
#define CHECK(cond, msg)                                                         \
    do {                                                                         \
        ++g_checks;                                                              \
        if (! (cond)) {                                                          \
            ++g_fail;                                                            \
            if (g_failures[msg]++ < 3) std::printf ("FAIL: %s  [%s:%d]\n", std::string (msg).c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

static std::string ctx;

// ---- helpers ----------------------------------------------------------------------------------------
static bool inSet (const std::vector<int>& pcs, int p) { return std::find (pcs.begin(), pcs.end(), pc (p)) != pcs.end(); }

static bool covers (const std::vector<Note>& notes, int total)
{
    std::vector<std::pair<int, int>> iv;
    for (auto& n : notes) iv.push_back ({ n.start, n.end() });
    std::sort (iv.begin(), iv.end());
    int reach = 0;
    for (auto& x : iv)
    {
        if (x.first > reach) return false;
        reach = std::max (reach, x.second);
    }
    return reach >= total;
}

static bool monophonic (const std::vector<Note>& notes)
{
    for (size_t i = 0; i + 1 < notes.size(); ++i)
        if (notes[i].end() > notes[i + 1].start + GLIDE_OVERLAP || notes[i + 1].start == notes[i].start) return false;
    return true;
}

static std::set<int> onsets (const std::vector<Note>& notes)
{
    std::set<int> s;
    for (auto& n : notes) s.insert (n.start);
    return s;
}

static bool sortedAndValid (const std::vector<Note>& notes, int total)
{
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& n = notes[i];
        if (n.pitch < 0 || n.pitch > 127 || n.start < 0 || n.length <= 0 || n.end() > total) return false;
        if (n.velocity < 1 || n.velocity > 127) return false;
        if (i > 0 && notes[i - 1].start > n.start) return false;
    }
    return true;
}

// in the last 1/8 before a chord change (or the loop end) the bass may approach the next chord
static bool inApproachWindow (const Song& s, int t, int& nextIndex)
{
    for (size_t i = 0; i <= s.chords.size(); ++i)
    {
        int b = i < s.chords.size() ? s.chords[i].start : s.totalTicks;
        if (b == 0) continue;
        if (t >= b - EIGHTH && t < b)
        {
            nextIndex = (int) (i % s.chords.size());
            return true;
        }
    }
    return false;
}

static bool bassPitchOk (const Song& s, const Note& n)
{
    const auto* ev = s.chordAt (n.start);
    if (pc (n.pitch) == ev->chord.bassPc()) return true;
    int ni;
    if (inApproachWindow (s, n.start, ni))
    {
        const auto& next = s.chords[(size_t) ni];
        if (pc (n.pitch) == next.chord.bassPc()) return true;
        if (inSet (keyScale (s.keyRoot, s.mode), n.pitch)) return true;
    }
    return false;
}

static Settings base (Genre g)
{
    Settings s;
    s.genre = g;
    s.keyRoot = 5;
    s.bars = 8;
    return s;
}

// ---- theory -----------------------------------------------------------------------------------------
static void testTheory()
{
    std::vector<Chord> ch;
    CHECK (parseProgression ("i VI III VII", 5, Mode::Minor, ch), "parse i VI III VII");
    CHECK (ch.size() == 4 && ch[0].name (true) == "Fm" && ch[1].name (true) == "Db" && ch[2].name (true) == "Ab" && ch[3].name (true) == "Eb",
           "F minor i-VI-III-VII = Fm Db Ab Eb");
    CHECK (parseProgression ("i bII V isus2", 5, Mode::Minor, ch) && ch[1].name (true) == "Gb" && ch[2].name (true) == "C"
               && ch[3].name (true) == "Fsus2", "bII / harmonic V / sus2");
    CHECK (parseProgression ("I V vi IV", 0, Mode::Major, ch) && ch[2].name (false) == "Am" && ch[3].name (false) == "F", "C major axis");
    CHECK (parseProgression ("Imaj7 iii7 vi7 IVmaj7", 0, Mode::Major, ch) && ch[0].name (false) == "Cmaj7" && ch[1].name (false) == "Em7",
           "sevenths");
    CHECK (parseProgression ("i bVI bIII bVII", 9, Mode::Minor, ch) && ch[1].name (false) == "F" && ch[2].name (false) == "C"
               && ch[3].name (false) == "G", "flat numerals from the major scale");
    CHECK (! parseProgression ("x y", 0, Mode::Major, ch), "reject junk");
    for (auto& def : progressionCatalogue())
        for (int k = 0; k < 12; ++k)
            CHECK (parseProgression (def.numerals, k, def.mode, ch) && ch.size() == 4, "catalogue parses, 4 chords");

    // chord scale over V in minor drops the b9 / avoid notes
    Chord v;
    parseNumeral ("V", 5, Mode::Minor, v); // C major in F minor
    auto cs = chordScale (v, 5, Mode::Minor);
    CHECK (! inSet (cs, 1) && ! inSet (cs, 5) && inSet (cs, 4) && inSet (cs, 0), "chord-scale over V avoids Db and F");

    // voicing stays in range and moves smoothly
    std::vector<int> prev;
    parseProgression ("i VI III VII", 5, Mode::Minor, ch);
    for (int pass = 0; pass < 2; ++pass)
        for (auto& c : ch)
        {
            auto vo = voiceChord (c.pitchClasses(), c.root, 53, 77, prev, 4);
            CHECK (vo.front() >= 53 && vo.back() <= 77, "voicing in range");
            for (int p : vo) CHECK (c.contains (p), "voicing uses chord tones");
            if (! prev.empty())
            {
                int move = 0;
                for (size_t i = 0; i < std::min (vo.size(), prev.size()); ++i) move += std::abs (vo[i] - prev[i]);
                CHECK (move <= 9, "voice leading moves at most a few semitones");
            }
            prev = vo;
        }
}

// ---- the big sweep ----------------------------------------------------------------------------------
struct SweepStats { int songs = 0; int notes = 0; };

static void checkSong (const Song& s, const Settings& st, SweepStats& stats)
{
    stats.songs++;
    const int total = s.totalTicks;
    CHECK (total == s.bars * BAR, "total ticks");
    // chord timeline covers the drop with no holes
    int reach = 0;
    for (auto& e : s.chords)
    {
        CHECK (e.start == reach, "chords contiguous");
        reach = e.end();
        CHECK (! e.voicing.empty(), "voicing present");
        CHECK (e.bassPitch == e.subPitch + 12, "bass is sub + 12");
        CHECK (pc (e.subPitch) == e.chord.bassPc(), "sub pitch is the chord root");
    }
    CHECK (reach == total, "chords reach the end");

    for (int l = 0; l < kNumLayers; ++l)
    {
        const auto& notes = s.layers[(size_t) l];
        stats.notes += (int) notes.size();
        CHECK (sortedAndValid (notes, total), std::string ("valid notes ") + layerName ((Layer) l));
        if (s.pattern[(size_t) l] == 0) CHECK (notes.empty(), "Off means no notes");
    }

    const auto& sub = s.layers[(size_t) Layer::Sub];
    const auto& A = s.layers[(size_t) Layer::BassA];
    const auto& B = s.layers[(size_t) Layer::BassB];
    const int bp = s.pattern[(size_t) Layer::BassB];
    const int ap = s.pattern[(size_t) Layer::BassA];

    // sub / bass: harmony
    for (auto& n : sub) CHECK (bassPitchOk (s, n), "sub plays the root (or approaches the next)");
    for (auto& n : A) CHECK (bassPitchOk (s, n), "bass A plays the root (or approaches the next)");
    for (auto& n : B) CHECK (bassPitchOk (s, n), "bass B plays the root (or approaches the next)");
    CHECK (monophonic (sub), "sub is monophonic");
    CHECK (monophonic (A), "bass A is monophonic");
    CHECK (monophonic (B), "bass B is monophonic");

    // gapless sustain: the mid line has no holes (except the offbeat pattern)
    const bool fullLength = st.layers[(size_t) Layer::BassA].length >= 0.999f && st.layers[(size_t) Layer::BassB].length >= 0.999f;
    if (fullLength && ap != BassOff && ap != BassOffbeat8)
    {
        std::vector<Note> both = A;
        if (bp != BOff && bp != BDouble) both.insert (both.end(), B.begin(), B.end());
        CHECK (covers (both, total), "bass line is gapless");
    }
    if (bp == BDouble && ap != BassOff && st.layers[(size_t) Layer::BassA].length == st.layers[(size_t) Layer::BassB].length
        && st.layers[(size_t) Layer::BassA].octave == st.layers[(size_t) Layer::BassB].octave
        && st.layers[(size_t) Layer::BassA].movement == st.layers[(size_t) Layer::BassB].movement)
        CHECK (A == B, "Double copies A");
    if (bp != BOff && bp != BDouble && ap != BassOff)
    {
        // A and B take turns: never both at once
        for (auto& a : A)
            for (auto& b : B)
            {
                int ov = std::min (a.end(), b.end()) - std::max (a.start, b.start);
                CHECK (ov <= 0, "A and B take turns");
            }
    }
    if (st.layers[(size_t) Layer::Sub].length >= 0.999f)
    {
        const int sp = s.pattern[(size_t) Layer::Sub];
        if (sp == SubHold || sp == SubHoldKick || sp == SubPulse4) CHECK (covers (sub, total), "sub is gapless");
    }

    // chords: only chord tones (of this chord, or of the next one inside a push)
    for (auto& n : s.layers[(size_t) Layer::Chords])
    {
        const auto* ev = s.chordAt (n.start);
        bool ok = ev->chord.contains (n.pitch);
        if (! ok)
        {
            int ni;
            if (inApproachWindow (s, n.start, ni)) ok = s.chords[(size_t) ni].chord.contains (n.pitch);
        }
        CHECK (ok, "chord layer uses chord tones");
    }
    // lead: chord tones on beats 1 and 3, everything inside the chord-scale
    for (auto& n : s.layers[(size_t) Layer::Lead])
    {
        const auto* ev = s.chordAt (n.start);
        if (n.start % (BAR / 2) == 0) CHECK (ev->chord.contains (n.pitch), "lead: chord tone on strong beats");
        bool ok = inSet (chordScale (ev->chord, s.keyRoot, s.mode), n.pitch) || ev->chord.contains (n.pitch);
        if (! ok)
        {
            int ni;
            if (inApproachWindow (s, n.start, ni))
                ok = inSet (chordScale (s.chords[(size_t) ni].chord, s.keyRoot, s.mode), n.pitch);
        }
        CHECK (ok, "lead: notes in the chord-scale");
        // a note sounding across a chord change must fit the new chord
        for (auto& e : s.chords)
            if (n.start < e.start && n.end() > e.start + GLIDE_OVERLAP)
                CHECK (e.chord.contains (n.pitch), "lead: held note fits the next chord");
    }
    CHECK (monophonic (s.layers[(size_t) Layer::Lead]), "lead is monophonic");
    // arp: chord tones only
    for (auto& n : s.layers[(size_t) Layer::Arp])
        CHECK (s.chordAt (n.start)->chord.contains (n.pitch), "arp: chord tones");
    // drums: kick on every downbeat; snare on beat 3 in half-time patterns
    const int dp = s.pattern[(size_t) Layer::Drums];
    if (dp != DrOff)
    {
        std::set<std::pair<int, int>> hits;
        for (auto& n : s.layers[(size_t) Layer::Drums]) hits.insert ({ n.pitch, n.start });
        for (int bar = 0; bar < s.bars; ++bar)
        {
            CHECK (hits.count ({ drum::Kick, bar * BAR }), "kick on every downbeat");
            if (dp == DrHalfTime || dp == DrHalfTimeHeavy || dp == DrTrap)
                CHECK (hits.count ({ drum::Snare, bar * BAR + 8 * STEP }), "half-time snare on beat 3");
            if (dp == DrHardstyle || dp == DrFourFloor)
                for (int b = 0; b < 4; ++b) CHECK (hits.count ({ drum::Kick, bar * BAR + b * BEAT }), "four-on-the-floor kick");
            if (dp == DrBackbeat)
                CHECK (hits.count ({ drum::Snare, bar * BAR + BEAT }) && hits.count ({ drum::Snare, bar * BAR + 3 * BEAT }), "backbeat");
        }
    }
    for (auto& n : s.layers[(size_t) Layer::FX])
        CHECK (n.pitch == fx::Impact || n.pitch == fx::Downlifter || n.pitch == fx::Riser || n.pitch == fx::ReverseCrash
                   || n.pitch == fx::NoiseSweep, "fx pads");
}

static void testSweep()
{
    SweepStats stats;
    const int progCount = kProgressionFirst + (int) progressionCatalogue().size();
    for (int g = 0; g < kNumGenres; ++g)
        for (int prog = 0; prog < progCount; ++prog)
            for (int cl = 0; cl < 3; ++cl)
                for (int bars : { 4, 8, 16, 32 })
                    for (double mv : { 0.0, 0.35, 0.7, 1.0 })
                    {
                        if (bars == 32 && mv != 0.7) continue;
                        Settings s = base ((Genre) g);
                        s.progression = prog;
                        s.chordLength = cl;
                        s.bars = bars;
                        s.songSeed = (uint32_t) (11 + prog);
                        s.grooveSeed = (uint32_t) (3 + cl);
                        for (auto& l : s.layers) l.movement = (float) mv;
                        Song song = generate (s);
                        checkSong (song, s, stats);
                    }
    // every pattern of every layer, in every genre, with movement on and off
    for (int g = 0; g < kNumGenres; ++g)
        for (int l = 0; l < kNumLayers; ++l)
            for (int p = 0; p <= (int) patternsFor ((Layer) l).size(); ++p)
                for (double mv : { 0.0, 0.5, 1.0 })
                    for (float len : { 1.0f, 0.5f })
                        for (int oct : { 0, -1, 2 })
                        {
                            Settings s = base ((Genre) g);
                            s.songSeed = (uint32_t) (100 + p);
                            s.grooveSeed = (uint32_t) (200 + l);
                            s.layers[(size_t) l].pattern = p;
                            s.layers[(size_t) l].length = len;
                            s.layers[(size_t) l].octave = oct;
                            for (auto& x : s.layers) x.movement = (float) mv;
                            if (l == (int) Layer::BassA)
                                for (int bpat = 0; bpat <= (int) BCount; ++bpat)
                                {
                                    s.layers[(size_t) Layer::BassB].pattern = bpat;
                                    Song song = generate (s);
                                    if (oct == 0) checkSong (song, s, stats);
                                }
                            else
                            {
                                Song song = generate (s);
                                if (oct == 0) checkSong (song, s, stats);
                                else
                                    for (int k = 0; k < kNumLayers; ++k)
                                        CHECK (sortedAndValid (song.layers[(size_t) k], song.totalTicks), "valid with octave shift");
                            }
                        }
    std::printf ("  sweep: %d drops, %d notes checked\n", stats.songs, stats.notes);
}

// ---- movement only adds groove ----------------------------------------------------------------------
static void testMovementOnlyAddsGroove()
{
    int compared = 0;
    for (int g = 0; g < kNumGenres; ++g)
        for (int prog : { 0, 2, 4, 17, 22 })
            for (int seed : { 1, 2, 3 })
                for (double mv : { 0.3, 0.6, 1.0 })
                {
                    Settings s0 = base ((Genre) g);
                    s0.progression = prog;
                    s0.grooveSeed = (uint32_t) seed;
                    for (auto& l : s0.layers) l.movement = 0.0f;
                    Settings s1 = s0;
                    for (auto& l : s1.layers) l.movement = (float) mv;
                    Song a = generate (s0), b = generate (s1);
                    ++compared;
                    for (int l : { (int) Layer::Sub, (int) Layer::BassA, (int) Layer::BassB, (int) Layer::Lead, (int) Layer::Arp })
                    {
                        auto o0 = onsets (a.layers[(size_t) l]);
                        auto o1 = onsets (b.layers[(size_t) l]);
                        bool subset = true;
                        for (int t : o0) if (! o1.count (t)) subset = false;
                        CHECK (subset, std::string ("movement never removes a base hit: ") + layerName ((Layer) l));
                    }
                    // chords: the same harmony, onsets kept except inside a 1/8 push
                    for (size_t i = 0; i < a.chords.size(); ++i)
                    {
                        CHECK (a.chords[i].chord.sameAs (b.chords[i].chord) && a.chords[i].start == b.chords[i].start,
                               "movement never changes the harmony");
                    }
                    auto c0 = onsets (a.layers[(size_t) Layer::Chords]);
                    auto c1 = onsets (b.layers[(size_t) Layer::Chords]);
                    for (int t : c0)
                        CHECK (c1.count (t) || c1.count (t - EIGHTH), "chord hits kept (or pushed 1/8 early)");
                    // the sub holds stay holds: total sounding time unchanged
                    long sa = 0, sb = 0;
                    for (auto& n : a.layers[(size_t) Layer::Sub]) sa += n.length;
                    for (auto& n : b.layers[(size_t) Layer::Sub]) sb += n.length;
                    CHECK (std::labs (sa - sb) <= (long) (GLIDE_OVERLAP * 4 * (a.bars / 4 + 1)), "sub stays sustained");
                }
    std::printf ("  movement: %d pairs compared\n", compared);
}

static void testVariationKeepsTheIdea()
{
    for (int g = 0; g < kNumGenres; ++g)
    {
        Settings s = base ((Genre) g);
        for (auto& l : s.layers) l.movement = 0.6f;
        Song a = generate (s);
        Song again = generate (s);
        bool same = true;
        for (int l = 0; l < kNumLayers; ++l) same = same && a.layers[(size_t) l] == again.layers[(size_t) l];
        CHECK (same, "deterministic");

        Settings v = s;
        v.grooveSeed = 77;
        Song b = generate (v);
        CHECK (a.chordNames == b.chordNames && a.progressionLabel == b.progressionLabel, "VARIATION keeps the progression");
        bool voicings = a.chords.size() == b.chords.size();
        for (size_t i = 0; voicings && i < a.chords.size(); ++i) voicings = a.chords[i].voicing == b.chords[i].voicing;
        CHECK (voicings, "VARIATION keeps the voicings");
        CHECK (a.pattern == b.pattern, "VARIATION keeps the patterns");

        // with movement at zero, VARIATION changes nothing at all
        Settings z = s, zv = s;
        for (auto& l : z.layers) l.movement = 0.0f;
        for (auto& l : zv.layers) l.movement = 0.0f;
        zv.grooveSeed = 999;
        Song za = generate (z), zb = generate (zv);
        bool identical = true;
        for (int l = 0; l < kNumLayers; ++l) identical = identical && za.layers[(size_t) l] == zb.layers[(size_t) l];
        CHECK (identical, "no movement = no variation");

        // explicit progression: GENERATE (song seed) keeps the chords
        Settings e1 = s, e2 = s;
        e1.progression = e2.progression = kProgressionFirst + 0;
        e2.songSeed = 4242;
        CHECK (generate (e1).chordNames == generate (e2).chordNames, "explicit progression survives GENERATE");
    }
    // Auto: different song seeds reach several progressions per genre
    for (int g = 0; g < kNumGenres; ++g)
    {
        std::set<std::string> seen;
        for (uint32_t seed = 1; seed <= 40; ++seed)
        {
            Settings s = base ((Genre) g);
            s.songSeed = seed;
            seen.insert (generate (s).progressionLabel);
        }
        CHECK (seen.size() >= 3, "Auto picks from the genre's progressions");
    }
}

static void testHoldsAreHolds()
{
    for (int g = 0; g < kNumGenres; ++g)
        for (int cl = 0; cl < 3; ++cl)
        {
            Settings s = base ((Genre) g);
            s.chordLength = cl;
            s.progression = kProgressionFirst + 0;
            s.layers[(size_t) Layer::Sub].pattern = SubHold + 1;
            s.layers[(size_t) Layer::BassA].pattern = BassLongHold + 1;
            s.layers[(size_t) Layer::BassB].pattern = BOff + 1;
            s.layers[(size_t) Layer::Chords].pattern = ChHold + 1;
            for (auto& l : s.layers) l.movement = 0.0f;
            Song song = generate (s);
            const auto& sub = song.layers[(size_t) Layer::Sub];
            const auto& a = song.layers[(size_t) Layer::BassA];
            CHECK (sub.size() == song.chords.size(), "Hold: one sub note per chord");
            CHECK (a.size() == song.chords.size(), "Long Hold: one bass note per chord");
            for (size_t i = 0; i < std::min (sub.size(), song.chords.size()); ++i)
                CHECK (sub[i].start == song.chords[i].start && sub[i].length == song.chords[i].length, "Hold lasts the whole chord");
            size_t voices = song.chords.empty() ? 0 : song.chords[0].voicing.size() + (genreProfile ((Genre) g).doubleRootBelow ? 1 : 0);
            CHECK (song.layers[(size_t) Layer::Chords].size() == song.chords.size() * voices, "Hold chords: one strike per chord");
        }
}

// ---- seeds ------------------------------------------------------------------------------------------
static std::vector<Note> triadSeed (const char* numerals, int key, Mode mode, bool withBass)
{
    std::vector<Chord> ch;
    parseProgression (numerals, key, mode, ch);
    std::vector<Note> notes;
    for (size_t i = 0; i < ch.size(); ++i)
    {
        auto v = voiceChord (ch[i].pitchClasses(), ch[i].root, 55, 72, {}, 3);
        for (int p : v) notes.push_back ({ p, (int) i * BAR + 3, BAR - 10, 90 });
        if (withBass)
            for (int k = 0; k < 4; ++k)
                notes.push_back ({ placeInRange (ch[i].root, 36), (int) i * BAR + k * BEAT, BEAT - 4, 100 });
    }
    return notes;
}

static void testSeeds()
{
    {
        auto a = analyseSeed (triadSeed ("i VI III VII", 5, Mode::Minor, true));
        CHECK (a.valid && a.bars == 4, "seed: 4 bars");
        CHECK (a.keyRoot == 5 && a.mode == Mode::Minor, "seed: F minor detected");
        CHECK (a.chords.size() == 4 && a.chords[0].chord.name (true) == "Fm" && a.chords[1].chord.name (true) == "Db"
                   && a.chords[2].chord.name (true) == "Ab" && a.chords[3].chord.name (true) == "Eb", "seed: Fm Db Ab Eb");
        CHECK (a.bassLine.size() == 16, "seed: bass line found");
        CHECK (covers (a.bassLine, 4 * BAR), "seed: bass line gapless");
    }
    {
        auto a = analyseSeed (triadSeed ("I V vi IV", 0, Mode::Major, false));
        CHECK (a.keyRoot == 0 && a.mode == Mode::Major, "seed: C major detected");
        CHECK (a.chords.size() == 4 && a.chords[2].chord.name (false) == "Am", "seed: C G Am F");
    }
    {
        auto a = analyseSeed (triadSeed ("i iv VI V", 9, Mode::Minor, true));
        CHECK (a.keyRoot == 9 && a.mode == Mode::Minor, "seed: A minor with harmonic V");
        CHECK (a.chords.size() == 4 && a.chords[3].chord.name (false) == "E", "seed: E major dominant");
    }
    {
        // a bare bassline: F F Ab Eb
        std::vector<Note> notes;
        int roots[] = { 41, 41, 44, 39 };
        for (int bar = 0; bar < 4; ++bar)
            for (int k = 0; k < 8; ++k)
                notes.push_back ({ roots[bar], bar * BAR + k * EIGHTH, EIGHTH - 6, 100 });
        auto a = analyseSeed (notes);
        CHECK (a.valid && a.keyRoot == 5, "seed: bassline in F");
        CHECK (! a.chords.empty() && a.chords.front().chord.root == 5, "seed: bassline harmony starts on F");
    }
    {
        // From Seed progression, transposed by KEY
        auto a = analyseSeed (triadSeed ("i VI III VII", 5, Mode::Minor, true));
        Settings s = base (Genre::MelodicDubstep);
        s.progression = kProgressionSeed;
        s.keyRoot = 5;
        Song song = generate (s, &a);
        CHECK (song.chordNames == "Fm  Db  Ab  Eb", "From Seed uses the seed chords");
        s.keyRoot = 7;
        song = generate (s, &a);
        CHECK (song.chordNames == "Gm  Eb  Bb  F", "From Seed transposes with KEY");
        // seed rhythm patterns
        s.keyRoot = 5;
        s.layers[(size_t) Layer::BassA].pattern = BassFromSeed + 1;
        s.layers[(size_t) Layer::Lead].pattern = LeadFromSeed + 1;
        s.layers[(size_t) Layer::Chords].pattern = ChFromSeed + 1;
        for (auto& l : s.layers) l.movement = 0.0f;
        SweepStats st;
        song = generate (s, &a);
        checkSong (song, s, st);
        auto on = onsets (song.layers[(size_t) Layer::BassA]);
        bool all = true;
        for (auto& n : a.bassLine) for (int rep = 0; rep < 2; ++rep) if (! on.count (n.start + rep * a.bars * BAR)) all = false;
        CHECK (all, "Bass From Seed follows the seed rhythm");
        CHECK (! song.layers[(size_t) Layer::Lead].empty(), "Lead From Seed produces a line");
        // without a seed, seed patterns fall back with a hint
        Song none = generate (s, nullptr);
        CHECK (! none.hints.empty(), "no-seed hint");
        CHECK (none.pattern[(size_t) Layer::BassA] != BassFromSeed, "no-seed fallback");
    }
}

// ---- MIDI files -------------------------------------------------------------------------------------
static void testMidi()
{
    Settings s = base (Genre::Tearout);
    for (auto& l : s.layers) l.movement = 0.8f;
    Song song = generate (s);
    for (int l = 0; l < kNumLayers; ++l)
    {
        if (song.layers[(size_t) l].empty()) continue;
        auto bytes = writeMidi ({ { layerName ((Layer) l), song.layers[(size_t) l], 0 } }, 150.0);
        std::vector<Note> back;
        double bpm;
        std::string err;
        CHECK (readMidi (bytes, back, bpm, err), "MIDI reads back");
        CHECK (std::fabs (bpm - 150.0) < 0.01, "tempo round trip");
        auto orig = song.layers[(size_t) l];
        auto key = [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; };
        std::sort (orig.begin(), orig.end(), key);
        std::sort (back.begin(), back.end(), key);
        CHECK (orig == back, std::string ("MIDI round trip exact: ") + layerName ((Layer) l));
    }
    std::vector<Note> notes;
    double bpm;
    std::string err;
    CHECK (! readMidi ({ 'n', 'o', 'p', 'e' }, notes, bpm, err), "reject non-MIDI");
    std::vector<uint8_t> junk (100, 0xFF);
    CHECK (! readMidi (junk, notes, bpm, err), "reject junk");
}

// ---- player + synth ---------------------------------------------------------------------------------
static void testPlayer()
{
    for (int g = 0; g < kNumGenres; ++g)
    {
        Settings s = base ((Genre) g);
        s.bars = 4;
        Song song = generate (s);
        auto data = makePlayData (song);
        PreviewSynth synth;
        synth.prepare (48000.0);
        SongPlayer player;
        const double tps = 150.0 / 60.0 * PPQ / 48000.0;
        std::vector<float> L (512), R (512);
        double tick = 0, peak = 0, energy = 0;
        bool finite = true;
        std::vector<MidiOutEvent> midi;
        int ons = 0, offs = 0;
        const long totalSamples = (long) std::floor (song.totalTicks * 2 / tps); // two loops, exactly
        for (long done = 0; done < totalSamples;)
        {
            const int n = (int) std::min<long> (512, totalSamples - done);
            midi.clear();
            player.process (synth, data.get(), L.data(), R.data(), n, tick, tps, true, 0xFF, &midi, 0xFF);
            for (auto& e : midi) (e.on ? ons : offs)++;
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) finite = false;
                peak = std::max (peak, (double) std::fabs (L[(size_t) i]));
                energy += L[(size_t) i] * L[(size_t) i];
            }
            tick += n * tps;
            done += n;
        }
        midi.clear();
        player.process (synth, data.get(), L.data(), R.data(), 512, tick, tps, false, 0xFF, &midi, 0xFF);
        for (auto& e : midi) (e.on ? ons : offs)++;
        CHECK (finite, "synth output finite");
        CHECK (peak <= 1.0 && peak > 0.05, "synth level sane");
        CHECK (energy > 1.0, "synth makes sound");
        CHECK (ons == offs, "every MIDI note-on gets a note-off");
        size_t expectedOns = 0;
        for (auto& l : song.layers) expectedOns += l.size();
        CHECK ((size_t) ons == expectedOns * 2, "every note plays once per loop");
    }
}

// ---- 1.1: GENERATE picks new patterns, user sounds -----------------------------------------------------
static void testGenerateVaries()
{
    for (int g = 0; g < kNumGenres; ++g)
    {
        std::set<std::vector<int>> combos;
        std::set<std::string> progs;
        for (uint32_t seed = 2; seed <= 40; ++seed)
        {
            Settings s = base ((Genre) g);
            s.songSeed = seed;
            Song song = generate (s);
            combos.insert (std::vector<int> (song.pattern.begin(), song.pattern.end()));
            progs.insert (song.progressionLabel);
            for (int l = 0; l < kNumLayers; ++l)
            {
                bool inPool = false;
                for (auto& pp : genrePatternPool ((Genre) g, (Layer) l)) inPool = inPool || pp.first == song.pattern[(size_t) l];
                CHECK (inPool, "GENERATE only picks patterns the genre uses");
            }
        }
        CHECK (combos.size() >= ((Genre) g == Genre::Trap ? 3u : 5u), "GENERATE gives different layer patterns");
        Settings one = base ((Genre) g);
        Song first = generate (one);
        for (int l = 0; l < kNumLayers; ++l)
            CHECK (first.pattern[(size_t) l] == genreProfile ((Genre) g).defaults[l], "a fresh plugin starts on the classic patterns");
        Settings legacy = base ((Genre) g);
        legacy.songSeed = 777;
        legacy.patternPools = false;
        Song old = generate (legacy);
        for (int l = 0; l < kNumLayers; ++l)
            CHECK (old.pattern[(size_t) l] == genreProfile ((Genre) g).defaults[l], "1.0 sets keep their patterns");
    }
}

static std::vector<float> testTone (double hz, double seconds, double sr, bool growl)
{
    std::vector<float> x ((size_t) (seconds * sr));
    double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / sr;
        ph += hz / sr;
        double v = 0;
        for (int h = 1; h <= 12; ++h) v += std::sin (2 * 3.14159265358979 * h * ph) / h * (growl ? (1.0 + 0.6 * std::sin (2 * 3.14159265358979 * (2.5 * t + h * 0.3))) : 1.0);
        x[i] = (float) std::tanh ((growl ? 2.5 : 0.8) * v);
    }
    return x;
}

static void testSamples()
{
    const double sr = 44100.0;
    for (double hz : { 32.70, 34.65, 41.20, 55.0, 98.0, 146.83, 220.0, 440.0 })
        for (bool growl : { false, true })
        {
            auto g = detectPitch (testTone (hz, 2.0, sr, growl), sr);
            const int want = (int) std::lround (69 + 12 * std::log2 (hz / 440.0));
            CHECK (g.midi == want, "pitch detection finds the note");
            if (g.midi != want) std::printf ("    %.2f Hz growl=%d -> %d (%.2f Hz, conf %.2f)\n", hz, (int) growl, g.midi, g.hz, g.confidence);
        }
    {
        std::vector<float> noise (88200);
        uint32_t st = 1;
        for (auto& v : noise) { st = st * 1664525u + 1013904223u; v = (float) ((int32_t) st) / 2147483648.0f; }
        auto r = chooseRoot (noise, sr, "Noise Sweep.wav");
        CHECK (! r.reason.empty(), "noise gets a root without crashing");
    }
    struct NameCase { const char* name; int pc; int oct; };
    for (auto c : { NameCase { "Cymatics - Sustained Growl 808 - C.wav", 0, -99 }, NameCase { "DS_TD_bass_synth_sustain_C.wav", 0, -99 },
                    NameCase { "Bass F#1 long.wav", 6, 1 }, NameCase { "reese_Bb0.wav", 10, 0 }, NameCase { "Am pad.wav", 9, -99 },
                    NameCase { "Bass Growl.wav", -1, -99 }, NameCase { "Sub_D-1.wav", 2, -1 }, NameCase { "growl_C#min.aif", 1, -99 },
                    NameCase { "/Users/x/Samples/Yoi Bass - G.wav", 7, -99 } })
    {
        int oct;
        const int got = noteFromFileName (c.name, oct);
        CHECK (got == c.pc && (c.pc < 0 || oct == c.oct), "note from file name");
        if (got != c.pc || (c.pc >= 0 && oct != c.oct)) std::printf ("    %s -> %d oct %d\n", c.name, got, oct);
    }
    {
        auto tone = testTone (34.65, 3.0, sr, true);
        auto r = chooseRoot (tone, sr, "DS_TD_bass_synth_sustain_C.wav");
        CHECK (r.midi == 25 && r.detected, "the audio wins over a wrong file name");
        auto smp = makeSampleData (tone, tone, sr, r.midi);
        CHECK (smp->loopStart > 0 && smp->loopEnd > smp->loopStart && smp->loopEnd <= (int) smp->left.size(), "sustain loop found");
        CHECK (smp->gain > 0.05f && smp->gain < 6.0f, "sound levelled");
        // play it through the synth on BASS A for a long hold: it must keep sounding past the sample's end
        PreviewSynth synth;
        synth.prepare (sr);
        synth.setLayerSample ((int) Layer::BassA, smp);
        synth.noteOn ((int) Layer::BassA, 30, 110);
        std::vector<float> L (512), R (512);
        double lateEnergy = 0;
        bool finite = true;
        for (int b = 0; b < (int) (6.0 * sr / 512); ++b)
        {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            synth.render (L.data(), R.data(), 512);
            for (float v : L) { finite = finite && std::isfinite (v); if (b * 512 > 4.0 * sr) lateEnergy += v * v; }
        }
        CHECK (finite, "sample voice output finite");
        CHECK (lateEnergy > 1.0, "a held sample keeps sounding through its loop");
        synth.noteOff ((int) Layer::BassA, 30);
    }
}

static void testInstrumentPreset()
{
    for (int l : { (int) Layer::Chords, (int) Layer::Sub, (int) Layer::BassA, (int) Layer::BassB, (int) Layer::Lead, (int) Layer::Arp })
    {
        auto voice = renderLayerVoice ((Layer) l, 44100.0);
        CHECK (voice && voice->left.size() > 4410, "built-in voice renders as a sample");
        CHECK (voice->rootNote == instrumentRoot ((Layer) l), "rendered voice root");
        bool finite = true;
        float peak = 0;
        for (float v : voice->left) { finite = finite && std::isfinite (v); peak = std::max (peak, std::fabs (v)); }
        for (float v : voice->right) { finite = finite && std::isfinite (v); peak = std::max (peak, std::fabs (v)); }
        CHECK (finite && peak > 0.01f, "rendered voice has sound");
        if (l != (int) Layer::Arp) CHECK (voice->loopEnd > voice->loopStart, "sustained voices loop");
        else CHECK (voice->loopEnd <= voice->loopStart, "the arp pluck is a one-shot");

        auto audio = instrumentAudio (*voice, false);
        float apeak = 0;
        for (size_t i = 0; i < audio.left.size(); ++i) apeak = std::max ({ apeak, std::fabs (audio.left[i]), std::fabs (audio.right[i]) });
        CHECK (std::fabs (apeak - 0.891f) < 0.01f && audio.left.size() == voice->left.size(), "instrument audio normalised to -1 dBFS");
        CHECK (std::fabs (audio.volumeDb - (-6.0 - 20.0 * std::log10 (0.891 / peak))) < 0.05, "Simpler volume restores the preview level");

        auto spec = instrumentDefaults ((Layer) l);
        spec.userName = "TEAROUT <test> & \"co\"";
        spec.sampleName = "growl's";
        spec.wavPath = "/Users/me/Library/Application Support/TEAROUT ENGINE/Sounds/Instruments/a & b.wav";
        spec.frames = (int) voice->left.size();
        spec.fileSize = 123456;
        spec.rootNote = voice->rootNote;
        spec.loopStart = voice->loopStart;
        spec.loopEnd = voice->loopEnd;
        const auto xml = makeSimplerPresetXml (spec);
        CHECK (xml.find ("@@") == std::string::npos, "no template markers left");
        CHECK (xml.find ("MinorVersion=\"11.0") != std::string::npos, "preset is tagged for Live 11");
        CHECK (xml.find ("<RootKey Value=\"" + std::to_string (voice->rootNote) + "\"") != std::string::npos, "root key written");
        CHECK (xml.find ("a &amp; b.wav") != std::string::npos && xml.find ("&lt;test&gt;") != std::string::npos, "paths and names escaped");
        const bool mono = l == (int) Layer::Sub || l == (int) Layer::BassA || l == (int) Layer::BassB || l == (int) Layer::Lead;
        CHECK ((xml.find ("<NumVoices Value=\"0\"") != std::string::npos) == mono, "mono layers get one voice");
        CHECK (xml.find ("MpePitchBendUsesTuning") == std::string::npos, "no Live 12-only settings");
        CHECK (xml.find ("<SustainLoop>") != std::string::npos, "sustain loop block present");
    }
    // a loaded sound: its gain is part of the preview level
    SampleData quiet;
    quiet.left.assign (1000, 0.1f);
    quiet.right.assign (1000, -0.1f);
    quiet.gain = 2.0f;
    auto qa = instrumentAudio (quiet, true);
    CHECK (std::fabs (qa.left[10] - 0.891f) < 1e-3f && std::fabs (qa.right[10] + 0.891f) < 1e-3f, "user sound normalised");
    CHECK (std::fabs (qa.volumeDb - (-6.0 - 20.0 * std::log10 (0.891 / 0.2))) < 0.01, "user sound gain kept in the volume");
    CHECK (instrumentDefaults (Layer::BassA, true).attackMs == 2.0 && instrumentDefaults (Layer::BassA).glide, "user sound envelope / glide");
    CHECK (instrumentDefaults (Layer::Chords).voicesIndex == 7 && ! instrumentDefaults (Layer::Chords).glide, "chords are poly");
}

int main()
{
    std::printf ("TEAROUT ENGINE engine tests\n");
    testTheory();
    testSweep();
    testMovementOnlyAddsGroove();
    testVariationKeepsTheIdea();
    testHoldsAreHolds();
    testSeeds();
    testMidi();
    testPlayer();
    testGenerateVaries();
    testSamples();
    testInstrumentPreset();
    std::printf ("%d checks, %d failed\n", g_checks, g_fail);
    if (g_fail)
        for (auto& f : g_failures) std::printf ("  %5d x %s\n", f.second, f.first.c_str());
    return g_fail ? 1 : 0;
}
