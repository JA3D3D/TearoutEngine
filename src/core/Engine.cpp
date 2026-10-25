#include "Engine.h"

#include "Rng.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace te
{
using namespace pat;

Settings::Settings()
{
    for (int l = 0; l < kNumLayers; ++l)
        layers[(size_t) l].length = defaultLength ((Layer) l);
}

int barsChoiceToBars (int i) noexcept
{
    static const int b[] = { 4, 8, 16, 32 };
    return b[std::clamp (i, 0, 3)];
}

int chordLengthTicks (int i) noexcept
{
    static const int t[] = { BAR / 2, BAR, 2 * BAR };
    return t[std::clamp (i, 0, 2)];
}

int resolvePattern (const Settings& s, Layer layer) noexcept
{
    const int idx = s.layers[(size_t) layer].pattern;
    const int count = (int) patternsFor (layer).size();
    if (idx <= 0 || idx > count)
        return genreProfile (s.genre).defaults[(int) layer];
    return idx - 1;
}

const ChordEvent* Song::chordAt (int tick) const noexcept
{
    if (chords.empty()) return nullptr;
    auto it = std::upper_bound (chords.begin(), chords.end(), tick,
                                [] (int t, const ChordEvent& e) { return t < e.start; });
    if (it == chords.begin()) return &chords.front();
    return &*(it - 1);
}

namespace
{
enum Kind : int { KBase = 0, KRestrike = 1, KFlick = 2, KApproach = 3 };

struct GN
{
    int pitch = 60, start = 0, length = STEP, velocity = 100;
    int kind = KBase;
    int end() const noexcept { return start + length; }
};

struct Seg
{
    int start, length;
    int chordOverride = -1; // index into song.chords (push / anticipation)
};

constexpr double kRoleFactor[4] = { 0.5, 1.0, 0.75, 1.25 };

double clamp01 (double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void sortNotes (std::vector<GN>& v)
{
    std::stable_sort (v.begin(), v.end(), [] (const GN& a, const GN& b) {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });
}

// ---- drum pattern data (1/16 steps, two-bar cycle) ---------------------------------------------
struct DrumBar { std::vector<int> kick, snare, clap, ch, oh; };
struct DrumDef { DrumBar a, b; };

const DrumDef& drumDef (int p)
{
    static const std::vector<int> e8 { 0, 2, 4, 6, 8, 10, 12, 14 };
    static const DrumDef defs[] = {
        { {}, {} }, // Off
        { { { 0 }, { 8 }, {}, e8, {} }, { { 0, 10 }, { 8 }, {}, e8, {} } },                                    // Half-time
        { { { 0, 3 }, { 8 }, { 8 }, { 2, 6, 10, 14 }, {} }, { { 0, 11, 14 }, { 8 }, { 8 }, { 2, 6, 10 }, { 14 } } }, // Heavy
        { { { 0, 10 }, { 8 }, { 8 }, e8, {} }, { { 0, 7, 10 }, { 8 }, { 8 }, { 0, 2, 4, 6, 8, 10, 12, 13, 14, 15 }, {} } }, // Trap
        { { { 0, 4, 8, 12 }, {}, { 4, 12 }, {}, { 2, 6, 10, 14 } }, { { 0, 4, 8, 12 }, {}, { 4, 12 }, {}, { 2, 6, 10, 14 } } }, // 4otf
        { { { 0, 4, 8, 12 }, {}, { 4, 12 }, { 2, 6, 10, 14 }, {} }, { { 0, 4, 8, 12 }, {}, { 4, 12 }, { 2, 6, 10, 14, 15 }, {} } }, // Hardstyle
        { { { 0, 8 }, { 4, 12 }, {}, e8, {} }, { { 0, 8, 10 }, { 4, 12 }, {}, e8, {} } },                       // Backbeat
    };
    return defs[std::clamp (p, 0, (int) DrCount - 1)];
}

bool isHalfTimeDrums (int p) { return p == DrHalfTime || p == DrHalfTimeHeavy || p == DrTrap; }

// ---- generation context ---------------------------------------------------------------------------
struct Ctx
{
    const Settings& s;
    const GenreProfile& g;
    const SeedAnalysis* seed;
    Song& song;
    int total = 0;
    int bars = 0;
    std::array<std::vector<int>, 4> groove; // ordered re-strike points per bar role
    std::set<int> pushTicks;                // chord changes anticipated by 1/8 (chords movement)

    Ctx (const Settings& st, const SeedAnalysis* sd, Song& sg)
        : s (st), g (genreProfile (st.genre)), seed (sd), song (sg) {}

    int role (int bar) const noexcept { return bar % 4; }
    bool phraseEnd (int bar) const noexcept { return (bar % 8) == 7 || bar == bars - 1; }
    double lift (int bar) const noexcept { return (bars >= 8 && bar >= bars / 2) ? 1.25 : 1.0; }
    double mv (Layer l) const noexcept { return clamp01 (s.layers[(size_t) l].movement); }
    double len (Layer l) const noexcept { return std::clamp ((double) s.layers[(size_t) l].length, 0.1, 1.0); }
    const ChordEvent& chordAt (int t) const noexcept { return *song.chordAt (t); }
    int chordIndexAt (int t) const noexcept { return (int) (song.chordAt (t) - song.chords.data()); }

    std::vector<int> chordBoundaries() const
    {
        std::vector<int> b;
        for (size_t i = 1; i < song.chords.size(); ++i)
            b.push_back (song.chords[i].start);
        return b;
    }
};

std::vector<Seg> splitAtChords (const Ctx& c, std::vector<Seg> segs)
{
    auto bounds = c.chordBoundaries();
    std::vector<Seg> out;
    for (auto s : segs)
    {
        if (s.start >= c.total) continue;
        s.length = std::min (s.length, c.total - s.start);
        if (s.length <= 0) continue;
        int cur = s.start;
        int end = s.start + s.length;
        for (int b : bounds)
        {
            if (b > cur && b < end)
            {
                out.push_back ({ cur, b - cur, s.chordOverride });
                cur = b;
            }
        }
        out.push_back ({ cur, end - cur, s.chordOverride });
    }
    return out;
}

std::vector<Seg> perBar (const Ctx& c, const std::vector<std::pair<int, int>>& barSegs)
{
    std::vector<Seg> out;
    for (int bar = 0; bar < c.bars; ++bar)
        for (auto& bs : barSegs)
            out.push_back ({ bar * BAR + bs.first, bs.second });
    return out;
}

std::vector<Seg> chordSegs (const Ctx& c)
{
    std::vector<Seg> out;
    for (auto& e : c.song.chords)
        out.push_back ({ e.start, e.length });
    return out;
}

// Gapless segments from loop-relative onsets, repeated over the timeline.
std::vector<Seg> segsFromOnsets (const Ctx& c, std::vector<int> onsets, int loopTicks)
{
    std::vector<Seg> out;
    if (loopTicks <= 0) return out;
    std::sort (onsets.begin(), onsets.end());
    onsets.erase (std::unique (onsets.begin(), onsets.end()), onsets.end());
    onsets.erase (std::remove_if (onsets.begin(), onsets.end(), [&] (int t) { return t < 0 || t >= loopTicks; }), onsets.end());
    if (onsets.empty() || onsets.front() != 0) onsets.insert (onsets.begin(), 0);
    for (int base = 0; base < c.total; base += loopTicks)
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            int st = base + onsets[i];
            int en = base + (i + 1 < onsets.size() ? onsets[i + 1] : loopTicks);
            if (st >= c.total) break;
            out.push_back ({ st, std::min (en, c.total) - st });
        }
    return out;
}

// Split every note that spans a groove point. countForBar decides how many points each bar uses.
// Returns, per bar, the ticks that were actually split (in priority order).
std::vector<std::vector<int>> restrike (std::vector<GN>& notes, const Ctx& c,
                                        const std::function<int (int)>& countForBar, int minSide, int velocity)
{
    std::vector<std::vector<int>> made ((size_t) c.bars);
    for (int bar = 0; bar < c.bars; ++bar)
    {
        int k = countForBar (bar);
        if (k <= 0) continue;
        int used = 0;
        for (int p : c.groove[(size_t) c.role (bar)])
        {
            if (used >= k) break;
            const int t = bar * BAR + p;
            std::vector<size_t> hits;
            for (size_t i = 0; i < notes.size(); ++i)
                if (notes[i].start < t && notes[i].end() > t && t - notes[i].start >= minSide && notes[i].end() - t >= minSide)
                    hits.push_back (i);
            if (hits.empty()) continue;
            for (size_t i : hits)
            {
                GN right = notes[i];
                right.start = t;
                right.length = notes[i].end() - t;
                right.kind = KRestrike;
                right.velocity = velocity;
                notes[i].length = t - notes[i].start;
                notes.push_back (right);
            }
            made[(size_t) bar].push_back (t);
            ++used;
        }
    }
    sortNotes (notes);
    return made;
}

int findMonoAt (const std::vector<GN>& notes, int t)
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].start <= t && notes[i].end() > t)
            return (int) i;
    return -1;
}

// Short +12 octave flick on a re-strike (same pitch class), returning to the root.
void octaveFlicks (std::vector<GN>& notes, const Ctx& c, const std::vector<std::vector<int>>& made, double mv, uint64_t salt)
{
    if (mv < 0.55) return;
    for (int bar = 0; bar < c.bars; ++bar)
    {
        int r = c.role (bar);
        if (! (r == 1 || r == 3 || (mv >= 0.85 && r == 2))) continue;
        if (made[(size_t) bar].empty()) continue;
        Rng rng { (uint64_t) c.s.grooveSeed, salt, (uint64_t) bar % 8 };
        if (! rng.chance (0.35 + (mv - 0.55) * 1.6)) continue;
        const int t = made[(size_t) bar].front();
        int i = findMonoAt (notes, t);
        if (i < 0 || notes[(size_t) i].start != t) continue;
        GN n = notes[(size_t) i];
        const int fl = std::min (n.length, 2 * STEP);
        const bool splitBack = n.length - fl >= STEP;
        GN back = n;
        back.start = n.start + fl;
        back.length = n.length - fl;
        back.kind = KRestrike;
        if (splitBack) n.length = fl;
        n.pitch += 12;
        n.kind = KFlick;
        n.velocity = 96;
        notes[(size_t) i] = n;
        if (splitBack) notes.push_back (back);
    }
    sortNotes (notes);
}

// Approach note in the last 1/8 before a chord change: anticipation of the next root, or a scale
// neighbour leading into it. Only splits notes that are already sounding.
void approachNotes (std::vector<GN>& notes, const Ctx& c, double mv, uint64_t salt)
{
    if (mv < 0.4) return;
    auto scale = keyScale (c.song.keyRoot, c.song.mode);
    const size_t nc = c.song.chords.size();
    for (size_t ci = 1; ci <= nc; ++ci)
    {
        // the last boundary is the loop point: the drop repeats into its first chord
        const auto& prev = c.song.chords[ci - 1];
        const auto& next = c.song.chords[ci % nc];
        const int b = ci < nc ? next.start : c.total;
        const int registerLow = c.g.bassLow - 2;
        if (prev.chord.bassPc() == next.chord.bassPc()) continue;
        const int barBefore = (b - 1) / BAR;
        const bool pushed = c.pushTicks.count (b) > 0;
        if (! (pushed || c.role (barBefore) == 3 || mv >= 0.75)) continue;
        Rng rng { (uint64_t) c.s.grooveSeed, salt, (uint64_t) ci };
        if (! pushed && ! rng.chance (0.45 + mv * 0.5)) continue;

        const int t = b - EIGHTH;
        int i = findMonoAt (notes, t);
        if (i < 0) continue;
        if (notes[(size_t) i].end() < b) continue;  // note must run into the change
        if (notes[(size_t) i].start > t) continue;
        if (notes[(size_t) i].start < t)
        {
            if (t - notes[(size_t) i].start < STEP) continue;
            GN right = notes[(size_t) i];
            right.start = t;
            right.length = notes[(size_t) i].end() - t;
            notes[(size_t) i].length = t - notes[(size_t) i].start;
            notes.push_back (right);
            i = (int) notes.size() - 1;
        }
        GN& a = notes[(size_t) i];
        a.length = std::min (a.length, b - a.start);
        const int nextPitch = next.bassPitch;
        if (pushed || rng.chance (0.6))
            a.pitch = nextPitch;
        else
        {
            int nb = stepInSet (scale, nextPitch, -1);
            if (nb < registerLow) nb = stepInSet (scale, nextPitch, +1);
            a.pitch = nb;
        }
        a.kind = KApproach;
        a.velocity = 94;
    }
    sortNotes (notes);
}

// Overlap selected pitch changes by 1/32 so a mono/legato synth slides.
void glides (std::vector<GN>& notes, const Ctx& c, double mv, double lengthParam, double threshold)
{
    if (mv < threshold || lengthParam < 0.95) return;
    sortNotes (notes);
    for (size_t i = 0; i + 1 < notes.size(); ++i)
    {
        GN& a = notes[i];
        const GN& b = notes[i + 1];
        if (a.end() != b.start || a.pitch == b.pitch) continue;
        if (b.length <= 2 * GLIDE_OVERLAP) continue;
        const int barBefore = (b.start - 1) / BAR;
        const bool turnaround = c.role (barBefore) == 3 && b.start % BAR == 0;
        if (turnaround || b.kind == KFlick || mv >= 0.8)
            a.length += GLIDE_OVERLAP;
    }
}

void applyLength (std::vector<GN>& notes, double L, int minLen = T32)
{
    if (L >= 0.999) return;
    for (auto& n : notes)
        n.length = std::max (minLen, (int) std::lround (n.length * L));
}

std::vector<Note> finish (std::vector<GN> notes, const Ctx& c, int octave)
{
    sortNotes (notes);
    std::vector<Note> out;
    out.reserve (notes.size());
    for (auto& g : notes)
    {
        if (g.start >= c.total || g.length <= 0) continue;
        Note n;
        n.pitch = g.pitch + 12 * octave;
        while (n.pitch > 127) n.pitch -= 12;
        while (n.pitch < 0) n.pitch += 12;
        n.start = g.start;
        n.length = std::min (g.length, c.total - g.start);
        n.velocity = std::clamp (g.velocity, 1, 127);
        out.push_back (n);
    }
    return out;
}

// ---- layers -----------------------------------------------------------------------------------------
std::vector<GN> monoFromSegs (const Ctx& c, const std::vector<Seg>& segs, bool sub)
{
    std::vector<GN> out;
    for (auto& s : segs)
    {
        const auto& ev = s.chordOverride >= 0 ? c.song.chords[(size_t) s.chordOverride] : c.chordAt (s.start);
        GN n;
        n.pitch = sub ? ev.subPitch : ev.bassPitch;
        n.start = s.start;
        n.length = s.length;
        n.velocity = (s.start % BAR == 0) ? 108 : (s.start == ev.start ? 104 : 98);
        out.push_back (n);
    }
    sortNotes (out);
    return out;
}

std::vector<GN> buildMidLine (const Ctx& c, int pattern)
{
    std::vector<Seg> segs;
    switch (pattern)
    {
        case BassLongHold:     segs = chordSegs (c); break;
        case BassHoldRestrike: segs = perBar (c, { { 0, 192 }, { 192, 192 } }); break;
        case BassRide332:      segs = perBar (c, { { 0, 144 }, { 144, 144 }, { 288, 96 } }); break;
        case BassRide422:      segs = perBar (c, { { 0, 192 }, { 192, 96 }, { 288, 96 } }); break;
        case BassPump8:        segs = perBar (c, { { 0, 48 }, { 48, 48 }, { 96, 48 }, { 144, 48 }, { 192, 48 }, { 240, 48 }, { 288, 48 }, { 336, 48 } }); break;
        case BassOffbeat8:     segs = perBar (c, { { 48, 48 }, { 144, 48 }, { 240, 48 }, { 336, 48 } }); break;
        case BassTriplet:      segs = perBar (c, { { 0, 64 }, { 64, 32 }, { 96, 64 }, { 160, 32 }, { 192, 96 }, { 288, 96 } }); break;
        case BassFromSeed:
            if (c.seed && c.seed->valid && ! c.seed->bassLine.empty())
            {
                std::vector<int> on;
                for (auto& n : c.seed->bassLine) on.push_back (n.start);
                segs = segsFromOnsets (c, on, c.seed->bars * BAR);
            }
            else
                segs = chordSegs (c);
            break;
        default: segs = chordSegs (c); break;
    }
    segs = splitAtChords (c, segs);
    return monoFromSegs (c, segs, false);
}

void moveMidLine (std::vector<GN>& line, const Ctx& c)
{
    const double mv = c.mv (Layer::BassA);
    auto made = restrike (line, c, [&] (int bar) {
        int k = (int) std::lround (mv * 4.0 * kRoleFactor[c.role (bar)] * c.lift (bar));
        if (c.phraseEnd (bar) && mv >= 0.15) ++k;
        return std::min (k, 6);
    }, STEP, 90);
    octaveFlicks (line, c, made, mv, 11);
    approachNotes (line, c, mv, 12);
}

void splitAB (const Ctx& c, const std::vector<GN>& line, int bPattern, bool aOn,
              std::vector<GN>& outA, std::vector<GN>& outB)
{
    outA.clear();
    outB.clear();
    std::vector<GN> work = line;
    auto splitAt = [&] (int offsetInBar, const std::function<bool (int bar)>& which) {
        std::vector<GN> res;
        for (auto n : work)
        {
            int bar = n.start / BAR;
            int t = bar * BAR + offsetInBar;
            if (which (bar) && n.start < t && n.end() > t)
            {
                GN r = n;
                r.start = t;
                r.length = n.end() - t;
                r.kind = KRestrike;
                r.velocity = 98;
                n.length = t - n.start;
                res.push_back (n);
                res.push_back (r);
            }
            else
                res.push_back (n);
        }
        work = res;
        sortNotes (work);
    };

    switch (bPattern)
    {
        case BOff:
            outA = work;
            break;
        case BDouble:
            outA = work;
            outB = work;
            break;
        case BAnswers:
            splitAt (192, [&] (int bar) { return c.role (bar) == 1 || c.role (bar) == 3; });
            for (auto& n : work)
            {
                int bar = n.start / BAR;
                bool answer = (c.role (bar) == 1 || c.role (bar) == 3) && (n.start % BAR) >= 192;
                (answer ? outB : outA).push_back (n);
            }
            break;
        case BSwap:
        {
            // alternate on the pattern's own hits; movement re-strikes stay on the sound they split from
            // (a bar with a single hit, like a long hold, swaps sound bar by bar instead)
            std::vector<int> hitsInBar ((size_t) c.bars + 1, 0);
            for (auto& n : work)
                if (n.kind == KBase) hitsInBar[(size_t) std::min (c.bars, n.start / BAR)]++;
            int lastBar = -1, k = 0;
            bool toA = true;
            for (auto& n : work)
            {
                if (n.kind == KBase)
                {
                    int bar = n.start / BAR;
                    if (bar != lastBar) { lastBar = bar; k = 0; }
                    else ++k;
                    toA = hitsInBar[(size_t) std::min (c.bars, bar)] <= 1 ? (bar % 2 == 0) : (k % 2) == 0;
                }
                (toA ? outA : outB).push_back (n);
            }
            break;
        }
        case BRestrikes:
        {
            // A keeps the head of each bar / chord (its first pattern hit), B takes every other hit
            int lastBar = -1;
            for (auto& n : work)
            {
                const auto& ev = c.chordAt (n.start);
                const int bar = n.start / BAR;
                bool head = n.kind == KBase && (bar != lastBar || n.start == ev.start);
                if (n.kind == KBase && bar != lastBar) lastBar = bar;
                (head ? outA : outB).push_back (n);
            }
            break;
        }
        case BHalfBar:
            splitAt (192, [] (int) { return true; });
            for (auto& n : work)
                ((n.start % BAR) >= 192 ? outB : outA).push_back (n);
            break;
        default:
            outA = work;
            break;
    }
    if (! aOn)
        outA.clear();
}

std::vector<GN> buildSub (const Ctx& c, int pattern, const std::vector<GN>& midLine)
{
    std::vector<Seg> segs;
    const double mv = c.mv (Layer::Sub);
    std::vector<GN> out;
    switch (pattern)
    {
        case SubOff: return {};
        case SubHold: segs = chordSegs (c); break;
        case SubHoldKick:
        {
            std::vector<int> on = c.song.kicks;
            for (auto& e : c.song.chords) on.push_back (e.start);
            std::sort (on.begin(), on.end());
            on.erase (std::unique (on.begin(), on.end()), on.end());
            for (size_t i = 0; i < on.size(); ++i)
            {
                int st = on[i];
                int en = i + 1 < on.size() ? on[i + 1] : c.total;
                if (st < c.total) segs.push_back ({ st, en - st });
            }
            break;
        }
        case SubFollowBass:
            for (auto n : midLine)
            {
                n.pitch -= (n.kind == KFlick) ? 24 : 12; // same root, one octave under; flicks stay on the root
                if (n.kind == KFlick) n.kind = KRestrike;
                out.push_back (n);
            }
            sortNotes (out);
            return out;
        case SubOffbeat8: segs = perBar (c, { { 48, 48 }, { 144, 48 }, { 240, 48 }, { 336, 48 } }); break;
        case SubPulse4:   segs = perBar (c, { { 0, 96 }, { 96, 96 }, { 192, 96 }, { 288, 96 } }); break;
        default: segs = chordSegs (c); break;
    }
    segs = splitAtChords (c, segs);
    out = monoFromSegs (c, segs, true);

    // movement: the sub stays stable; it only re-strikes in the groove and (808) jumps the octave
    if (pattern == SubHold || pattern == SubPulse4)
    {
        if (mv >= 0.45)
        {
            // re-strike long chords on internal bar lines
            std::vector<GN> res;
            for (auto n : out)
            {
                int t = (n.start / BAR + 1) * BAR;
                while (t < n.end() && n.end() - t >= STEP && t - n.start >= STEP)
                {
                    GN r = n;
                    r.start = t;
                    r.length = n.end() - t;
                    r.kind = KRestrike;
                    r.velocity = 96;
                    n.length = t - n.start;
                    res.push_back (n);
                    n = r;
                    t += BAR;
                }
                res.push_back (n);
            }
            out = res;
            sortNotes (out);
        }
        restrike (out, c, [&] (int bar) {
            if (mv < 0.6) return 0;
            return (int) std::lround ((mv - 0.4) * 1.5 * kRoleFactor[c.role (bar)]);
        }, STEP, 94);
    }
    else if (pattern == SubHoldKick && c.s.genre == Genre::Trap && mv >= 0.35)
    {
        // 808 octave jump on the last kick of turnaround (and, with more movement, answer) bars
        for (int bar = 0; bar < c.bars; ++bar)
        {
            int r = c.role (bar);
            if (! (r == 3 || (mv >= 0.7 && r == 1))) continue;
            int best = -1;
            for (size_t i = 0; i < out.size(); ++i)
                if (out[i].start / BAR == bar && (out[i].start % BAR) >= 192 && out[i].start % BAR != 0)
                    best = (int) i;
            if (best >= 0)
            {
                out[(size_t) best].pitch += 12;
                out[(size_t) best].kind = KFlick;
            }
        }
    }
    else if (pattern == SubOffbeat8)
    {
        restrike (out, c, [&] (int bar) {
            return std::min (4, (int) std::lround (mv * 4.0 * kRoleFactor[c.role (bar)] * c.lift (bar)));
        }, STEP, 92);
    }
    return out;
}

void computePushTicks (Ctx& c, int chordPattern)
{
    c.pushTicks.clear();
    const double mv = c.mv (Layer::Chords);
    if (chordPattern == ChOff || mv < 0.6) return;
    for (size_t i = 1; i < c.song.chords.size(); ++i)
    {
        const int b = c.song.chords[i].start;
        if (b % BAR != 0) continue;
        if (c.role (b / BAR - 1) != 3) continue;
        if (c.song.chords[i].chord.sameAs (c.song.chords[i - 1].chord)) continue;
        Rng rng { (uint64_t) c.s.grooveSeed, 4001, (uint64_t) i };
        if (rng.chance ((mv - 0.5) * 1.6))
            c.pushTicks.insert (b);
    }
}

std::vector<GN> buildChords (const Ctx& c, int pattern)
{
    if (pattern == ChOff) return {};
    std::vector<Seg> segs;
    switch (pattern)
    {
        case ChHold:          segs = chordSegs (c); break;
        case ChStabs332:      segs = perBar (c, { { 0, 144 }, { 144, 144 }, { 288, 96 } }); break;
        case ChPulse8:        segs = perBar (c, { { 0, 48 }, { 48, 48 }, { 96, 48 }, { 144, 48 }, { 192, 48 }, { 240, 48 }, { 288, 48 }, { 336, 48 } }); break;
        case ChOffbeat8:      segs = perBar (c, { { 48, 48 }, { 144, 48 }, { 240, 48 }, { 336, 48 } }); break;
        case ChHalfTimeStabs: segs = perBar (c, { { 0, 72 }, { 144, 48 }, { 240, 48 } }); break;
        case ChFromSeed:
            if (c.seed && c.seed->valid && ! c.seed->chordOnsets.empty())
                segs = segsFromOnsets (c, c.seed->chordOnsets, c.seed->bars * BAR);
            else
                segs = chordSegs (c);
            break;
        default: segs = chordSegs (c); break;
    }
    segs = splitAtChords (c, segs);

    // anticipation (push): the last 1/8 before a pushed change already plays the next chord
    if (! c.pushTicks.empty())
    {
        std::vector<Seg> res;
        for (auto s : segs)
        {
            bool done = false;
            for (int b : c.pushTicks)
            {
                const int t = b - EIGHTH;
                if (s.start < b && s.start + s.length == b)
                {
                    const int next = c.chordIndexAt (b);
                    if (s.start >= t) { s.chordOverride = next; res.push_back (s); }
                    else
                    {
                        res.push_back ({ s.start, t - s.start, s.chordOverride });
                        res.push_back ({ t, b - t, next });
                    }
                    done = true;
                    break;
                }
            }
            if (! done) res.push_back (s);
        }
        segs = res;
        if (pattern == ChHold)
        {
            // a pushed sustained chord is not re-struck on the downbeat: merge
            std::vector<Seg> merged;
            for (auto& s : segs)
            {
                if (! merged.empty() && merged.back().chordOverride >= 0 && c.pushTicks.count (s.start)
                    && merged.back().start + merged.back().length == s.start)
                {
                    merged.back().length += s.length;
                    continue;
                }
                merged.push_back (s);
            }
            segs = merged;
        }
    }

    std::vector<GN> out;
    for (auto& s : segs)
    {
        const auto& ev = s.chordOverride >= 0 ? c.song.chords[(size_t) s.chordOverride] : c.chordAt (s.start);
        std::vector<int> pitches = ev.voicing;
        if (c.g.doubleRootBelow && ! pitches.empty())
        {
            int p = placeInRange (ev.chord.root, pitches.front() - 12);
            if (pitches.front() - p < 3) p -= 12;
            pitches.insert (pitches.begin(), p);
        }
        for (int p : pitches)
        {
            GN n;
            n.pitch = p;
            n.start = s.start;
            n.length = s.length;
            n.velocity = (s.start == ev.start || s.chordOverride >= 0) ? 100 : 92;
            out.push_back (n);
        }
    }
    sortNotes (out);
    const double mv = c.mv (Layer::Chords);
    restrike (out, c, [&] (int bar) {
        return std::min (3, (int) std::lround (mv * 2.0 * kRoleFactor[c.role (bar)] * c.lift (bar)));
    }, EIGHTH, 86);
    return out;
}

// ---- lead -----------------------------------------------------------------------------------------
struct LeadHit { int start, len; bool strong; int move; int seedInterval; };

std::vector<GN> buildLead (const Ctx& c, int pattern)
{
    if (pattern == LeadOff) return {};
    const auto& g = c.g;
    const int lo = g.leadRange.lo, hi = g.leadRange.hi;
    const int center = (lo + hi) / 2;
    std::vector<GN> out;

    auto clampRange = [&] (int p) {
        while (p > hi) p -= 12;
        while (p < lo) p += 12;
        return p;
    };
    auto chordTones = [&] (int t) { return c.chordAt (t).chord.pitchClasses(); };
    auto scaleAt = [&] (int t) { return chordScale (c.chordAt (t).chord, c.song.keyRoot, c.song.mode); };

    // fill weak notes between strong ones: passing tones or neighbours, always in the chord-scale
    auto fillWeak = [&] (std::vector<GN>& notes, const std::vector<bool>& strong) {
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (strong[i]) continue;
            int prev = i > 0 ? notes[i - 1].pitch : notes[i].pitch;
            int next = prev;
            for (size_t j = i + 1; j < notes.size(); ++j)
                if (strong[j]) { next = notes[j].pitch; break; }
            auto sc = scaleAt (notes[i].start);
            int p;
            if (std::abs (next - prev) >= 3)
                p = nearestInSet (sc, (prev + next) / 2, next > prev ? 1 : -1);
            else
            {
                Rng r { (uint64_t) c.s.songSeed, 211, (uint64_t) (notes[i].start % (4 * BAR)) };
                p = stepInSet (sc, prev, r.chance (0.6) ? 1 : -1);
            }
            notes[i].pitch = clampRange (p);
        }
    };

    if (pattern == LeadFromSeed && c.seed && c.seed->valid && ! c.seed->topLine.empty())
    {
        const auto& top = c.seed->topLine;
        const int loopTicks = c.seed->bars * BAR;
        double mean = 0;
        for (auto& n : top) mean += n.pitch;
        mean /= (double) top.size();
        for (int base = 0; base < c.total; base += loopTicks)
        {
            std::vector<GN> phrase;
            int prev = 0;
            for (size_t j = 0; j < top.size(); ++j)
            {
                GN n;
                n.start = base + top[j].start;
                if (n.start >= c.total) break;
                n.length = std::min (top[j].length, c.total - n.start);
                n.velocity = std::clamp (top[j].velocity, 70, 118);
                bool st = (n.start % (BAR / 2) == 0) || n.length >= BEAT;
                int target = j == 0 ? (int) std::lround (center + (top[0].pitch - mean)) : prev + (top[j].pitch - top[j - 1].pitch);
                int dir = j == 0 ? 0 : (top[j].pitch > top[j - 1].pitch ? 1 : -1);
                n.pitch = clampRange (nearestInSet (st ? chordTones (n.start) : scaleAt (n.start), target, dir));
                prev = n.pitch;
                phrase.push_back (n);
            }
            for (auto& n : phrase) out.push_back (n);
        }
    }
    else
    {
        const auto& temps = leadTemplates (pattern == LeadFromSeed ? LeadHook : pattern);
        Rng pick { (uint64_t) c.s.songSeed, 201, (uint64_t) c.s.genre };
        int ti;
        if ((pattern == LeadHook || pattern == LeadFromSeed) && ! g.hookTemplates.empty())
            ti = g.hookTemplates[(size_t) pick.below ((int) g.hookTemplates.size())];
        else
            ti = pick.below ((int) temps.size());
        const auto& tpl = temps[(size_t) std::clamp (ti, 0, (int) temps.size() - 1)];

        // the motif's contour: chord-tone steps between strong notes, arch-shaped
        Rng cr { (uint64_t) c.s.songSeed, 202 };
        std::vector<int> moves;
        for (size_t i = 0; i < tpl.size(); ++i)
        {
            bool firstHalf = i < tpl.size() / 2;
            std::vector<double> w = firstHalf ? std::vector<double> { 0.6, 2.0, 1.6, 3.2, 1.4 }
                                              : std::vector<double> { 1.3, 3.2, 1.6, 2.0, 0.6 };
            moves.push_back (cr.weighted (w) - 2);
        }
        Rng sr { (uint64_t) c.s.songSeed, 203 };
        const int startOffset = std::vector<int> { -3, 0, 4 }[(size_t) sr.below (3)];

        for (int phraseStart = 0; phraseStart < c.total; phraseStart += 4 * BAR)
        {
            std::vector<GN> phrase;
            std::vector<bool> strong;
            const auto firstTones = chordTones (phraseStart);
            int prevStrong = clampRange (nearestInSet (firstTones, center + startOffset));
            for (int half = 0; half < 2; ++half)
            {
                for (size_t i = 0; i < tpl.size(); ++i)
                {
                    GN n;
                    n.start = phraseStart + half * 2 * BAR + tpl[i].step * STEP;
                    n.length = tpl[i].len * STEP;
                    if (n.start >= c.total) break;
                    n.length = std::min (n.length, c.total - n.start);
                    bool st = (tpl[i].step % 8 == 0) || tpl[i].len >= 4;
                    n.velocity = st ? 104 : 94;
                    if (st)
                    {
                        auto tones = chordTones (n.start);
                        int base = nearestInSet (tones, prevStrong);
                        int p = stepInSet (tones, base, (i == 0 && half == 0) ? 0 : moves[i]);
                        if (p > hi || p < lo) p = stepInSet (tones, base, -moves[i]);
                        const bool last = half == 1 && i + 1 == tpl.size();
                        if (last)
                        {
                            // resolve: root or third of the chord, nearest to where we are
                            const auto& ch = c.chordAt (n.start).chord;
                            std::vector<int> rt { ch.root };
                            if (ch.intervals.size() > 1) rt.push_back (pc (ch.root + ch.intervals[1]));
                            p = nearestInSet (rt, prevStrong);
                        }
                        n.pitch = clampRange (p);
                        prevStrong = n.pitch;
                    }
                    phrase.push_back (n);
                    strong.push_back (st);
                }
            }
            // weak notes need their neighbours' pitches, so fill them after
            for (size_t i = 0; i < phrase.size(); ++i)
                if (! strong[i] && i == 0) phrase[i].pitch = prevStrong;
            fillWeak (phrase, strong);
            for (auto& n : phrase) out.push_back (n);
        }
    }
    sortNotes (out);

    // a note held across a chord change must still fit: split it there and move to the nearest chord tone
    {
        std::vector<GN> res;
        for (auto n : out)
        {
            for (size_t ci = 1; ci < c.song.chords.size(); ++ci)
            {
                const int b = c.song.chords[ci].start;
                if (n.start < b && n.end() > b)
                {
                    auto tones = c.song.chords[ci].chord.pitchClasses();
                    if (std::find (tones.begin(), tones.end(), pc (n.pitch)) == tones.end())
                    {
                        GN r = n;
                        r.start = b;
                        r.length = n.end() - b;
                        r.pitch = clampRange (nearestInSet (tones, n.pitch));
                        n.length = b - n.start;
                        res.push_back (n);
                        n = r;
                    }
                }
            }
            res.push_back (n);
        }
        out = res;
        sortNotes (out);
    }

    // movement: re-strike long notes in the groove, and pick-ups that split a note already sounding
    const double mv = c.mv (Layer::Lead);
    auto made = restrike (out, c, [&] (int bar) {
        return std::min (3, (int) std::lround (mv * 1.5 * kRoleFactor[c.role (bar)] * c.lift (bar)));
    }, 2 * STEP, 92);
    (void) made;
    if (mv >= 0.5)
    {
        for (int bar = 0; bar + 1 < c.bars; ++bar)
        {
            if (c.role (bar) != 3) continue;
            const int t = bar * BAR + 15 * STEP;
            int i = findMonoAt (out, t);
            if (i < 0 || out[(size_t) i].start >= t || t - out[(size_t) i].start < STEP) continue;
            int nextFirst = -1;
            for (auto& n : out) if (n.start >= (bar + 1) * BAR) { nextFirst = n.pitch; break; }
            if (nextFirst < 0) continue;
            GN r = out[(size_t) i];
            r.start = t;
            r.length = out[(size_t) i].end() - t;
            out[(size_t) i].length = t - out[(size_t) i].start;
            auto sc = scaleAt (t + STEP);
            r.pitch = clampRange (stepInSet (sc, nextFirst, -1));
            r.kind = KApproach;
            r.velocity = 90;
            out.push_back (r);
            sortNotes (out);
        }
    }
    return out;
}

// ---- arp ------------------------------------------------------------------------------------------
std::vector<GN> buildArp (const Ctx& c, int pattern)
{
    if (pattern == ArpOff) return {};
    const int rate = pattern == ArpUp8 ? EIGHTH : (pattern == ArpUpTrip ? TRIP16 : STEP);
    const double L = c.len (Layer::Arp);
    const double mv = c.mv (Layer::Arp);
    Rng sr { (uint64_t) c.s.songSeed, 301 };
    const int startChoice = sr.below (3);
    int prevStart = -1;
    std::vector<GN> out;

    for (auto& ev : c.song.chords)
    {
        std::vector<int> pcs;
        for (int iv : ev.chord.intervals)
            if (iv < 12 && std::find (pcs.begin(), pcs.end(), pc (ev.chord.root + iv)) == pcs.end())
                pcs.push_back (pc (ev.chord.root + iv));
        if (pcs.size() > 4)
        {
            auto fifth = std::find (pcs.begin(), pcs.end(), pc (ev.chord.root + 7));
            if (fifth != pcs.end()) pcs.erase (fifth);
        }
        int startPitch;
        if (prevStart < 0)
        {
            int rootP = placeInRange (ev.chord.root, c.g.arpLow);
            startPitch = stepInSet (pcs, rootP, startChoice);
            if (startPitch > c.g.arpLow + 12) startPitch = rootP;
        }
        else
            startPitch = nearestInSet (pcs, prevStart);
        prevStart = startPitch;

        std::vector<int> asc { startPitch };
        for (size_t k = 1; k < pcs.size(); ++k) asc.push_back (stepInSet (pcs, asc.back(), 1));
        if (pcs.size() <= 3) asc.push_back (startPitch + 12);

        std::vector<int> cycle;
        switch (pattern)
        {
            case ArpDown16: cycle.assign (asc.rbegin(), asc.rend()); break;
            case ArpUpDown16:
                cycle = asc;
                for (int k = (int) asc.size() - 2; k >= 1; --k) cycle.push_back (asc[(size_t) k]);
                break;
            case ArpUp2Oct:
                cycle = asc;
                if (pcs.size() <= 3) cycle.pop_back();
                {
                    size_t n0 = cycle.size();
                    for (size_t k = 0; k < n0; ++k) cycle.push_back (cycle[k] + 12);
                }
                break;
            default: cycle = asc; break;
        }

        std::vector<std::pair<int, int>> hits; // start, length
        if (pattern == ArpPluck332)
        {
            for (int h = ev.start; h < ev.end(); h += BAR / 2)
                for (auto pr : std::vector<std::pair<int, int>> { { 0, 3 }, { 3, 3 }, { 6, 2 } })
                    if (h + pr.first * STEP < ev.end())
                        hits.push_back ({ h + pr.first * STEP, pr.second * STEP });
        }
        else
            for (int t = ev.start; t < ev.end(); t += rate)
                hits.push_back ({ t, rate });

        int idx = 0;
        for (auto& h : hits)
        {
            GN n;
            n.start = h.first;
            n.pitch = cycle[(size_t) (idx % (int) cycle.size())];
            int slot = std::min (h.second, ev.end() - h.first);
            n.length = std::max (T32 / 2, (int) std::lround (slot * L));
            n.velocity = (n.start % BEAT == 0) ? 98 : 86;
            if (mv > 0 && idx % 3 == 0)
                n.velocity = std::min (122, n.velocity + (int) std::lround (28 * mv));
            out.push_back (n);
            ++idx;
        }
    }
    sortNotes (out);

    // movement: octave jump at the end of answer bars, ratchet at turnarounds
    for (int bar = 0; bar < c.bars; ++bar)
    {
        int last = -1;
        for (size_t i = 0; i < out.size(); ++i)
            if (out[i].start / BAR == bar) last = (int) i;
        if (last < 0) continue;
        if (mv >= 0.5 && c.role (bar) == 1)
        {
            out[(size_t) last].pitch += 12;
            out[(size_t) last].kind = KFlick;
        }
        if (mv >= 0.7 && c.role (bar) == 3 && rate >= STEP)
        {
            GN& n = out[(size_t) last];
            int half = rate / 2;
            GN r = n;
            r.start = n.start + half;
            r.length = std::max (T32 / 2, std::min (n.length, half));
            r.kind = KRestrike;
            n.length = std::min (n.length, half);
            out.push_back (r);
        }
    }
    sortNotes (out);
    return out;
}

// ---- drums ----------------------------------------------------------------------------------------
std::vector<GN> buildDrums (const Ctx& c, int pattern)
{
    if (pattern == DrOff) return {};
    const double mv = c.mv (Layer::Drums);
    const auto& def = drumDef (pattern);
    const bool trap = pattern == DrTrap;
    const bool fourFloor = pattern == DrFourFloor || pattern == DrHardstyle;
    std::vector<GN> out;
    auto add = [&] (int pitch, int tick, int len, int vel) {
        for (auto& n : out)
            if (n.pitch == pitch && n.start == tick) return;
        GN n;
        n.pitch = pitch;
        n.start = tick;
        n.length = len;
        n.velocity = vel;
        out.push_back (n);
    };
    auto removeIn = [&] (int pitch, int from, int to) {
        out.erase (std::remove_if (out.begin(), out.end(), [&] (const GN& n) {
                       return n.pitch == pitch && n.start >= from && n.start < to; }), out.end());
    };

    for (int bar = 0; bar < c.bars; ++bar)
    {
        const DrumBar& db = (bar % 2 == 0) ? def.a : def.b;
        const int B = bar * BAR;
        for (int s : db.kick)  add (drum::Kick, B + s * STEP, STEP, 118);
        for (int s : db.snare) add (drum::Snare, B + s * STEP, STEP, 112);
        for (int s : db.clap)  add (drum::Clap, B + s * STEP, STEP, 104);
        for (int s : db.oh)    add (drum::OpenHat, B + s * STEP, 2 * STEP, 88);
        for (int s : db.ch)
        {
            int vel = trap ? ((s % 2 == 0) ? 96 : 70) : ((s % 4 == 0) ? 92 : ((s % 2 == 0) ? 78 : 62));
            add (drum::ClosedHat, B + s * STEP, STEP, vel);
        }
        if (bar % 8 == 0) add (drum::Crash, B, BAR, 110);

        const int r = c.role (bar);
        // momentum: 16th hats in the second half of long drops
        if (mv >= 0.4 && c.lift (bar) > 1.0 && ! fourFloor)
            for (int s = 1; s < 16; s += 2)
                add (drum::ClosedHat, B + s * STEP, STEP, 58);
        // ghost snares
        if (mv >= 0.35 && r == 1)
            add (drum::Snare, B + (isHalfTimeDrums (pattern) ? 15 : 7) * STEP, STEP, 48);
        if (mv >= 0.6 && r == 3 && isHalfTimeDrums (pattern))
            add (drum::Snare, B + 13 * STEP, STEP, 44);
        // kick variation into the turnaround
        if (mv >= 0.5 && r == 3 && ! fourFloor)
            add (drum::Kick, B + (pattern == DrBackbeat ? 15 : 14) * STEP, STEP, 104);
        // hat roll on the last beat of turnarounds
        if (mv >= 0.25 && r == 3)
        {
            removeIn (drum::ClosedHat, B + 12 * STEP, B + BAR);
            int sub = (trap || mv >= 0.5) ? (trap ? TRIP16 : T32) : STEP;
            int n = BEAT / sub;
            for (int k = 0; k < n; ++k)
                add (drum::ClosedHat, B + 12 * STEP + k * sub, sub, 60 + (40 * k) / std::max (1, n - 1));
        }
        // fill at the end of the 8-bar phrase
        if (mv >= 0.3 && c.phraseEnd (bar))
        {
            const int fillPitch = fourFloor ? drum::Clap : drum::Snare;
            if (mv >= 0.7)
            {
                for (int k = 0; k < 4; ++k) add (fillPitch, B + 12 * STEP + k * STEP, STEP, 70 + k * 8);
                for (int k = 0; k < 4; ++k) add (fillPitch, B + 14 * STEP + k * T32, T32, 96 + k * 6);
                if (pattern == DrHardstyle)
                    for (int k = 0; k < 4; ++k) add (drum::Kick, B + 12 * STEP + k * STEP, STEP, 100 + k * 5);
            }
            else
                for (int k = 0; k < 4; ++k) add (fillPitch, B + 12 * STEP + k * STEP, STEP, 70 + k * 12);
        }
    }
    sortNotes (out);
    return out;
}

std::vector<GN> buildFx (const Ctx& c, int pattern)
{
    if (pattern == FxOff) return {};
    const double mv = c.mv (Layer::FX);
    std::vector<GN> out;
    auto add = [&] (int pitch, int start, int len, int vel) {
        if (start < 0 || start >= c.total) return;
        GN n;
        n.pitch = pitch;
        n.start = start;
        n.length = std::min (len, c.total - start);
        n.velocity = vel;
        out.push_back (n);
    };
    for (int bar = 0; bar < c.bars; ++bar)
    {
        const int B = bar * BAR;
        if (bar % 8 == 0)
        {
            if (pattern == FxSweeps) add (fx::Downlifter, B, 2 * BAR, 100);
            else add (fx::Impact, B, BAR, 110);
        }
        else if (mv >= 0.5 && bar % 4 == 0 && pattern != FxSweeps)
            add (fx::Impact, B, BAR, 82);
        if (pattern == FxSweeps && bar % 4 == 3)
            add (fx::NoiseSweep, B, BAR, 96);
    }
    if (pattern == FxImpactsRisers)
    {
        std::vector<int> boundaries;
        for (int b = 8; b < c.bars; b += 8) boundaries.push_back (b);
        boundaries.push_back (c.bars);
        for (int b : boundaries)
        {
            add (fx::Riser, (b - 2) * BAR, 2 * BAR, 100);
            add (fx::ReverseCrash, b * BAR - BEAT, BEAT, 100);
        }
    }
    sortNotes (out);
    return out;
}
} // namespace

// ---- main entry -----------------------------------------------------------------------------------
Song generate (const Settings& s, const SeedAnalysis* seed)
{
    Song song;
    Ctx c (s, seed, song);
    const auto& g = c.g;

    c.bars = s.bars <= 4 ? 4 : (s.bars <= 8 ? 8 : (s.bars <= 16 ? 16 : 32));
    c.total = c.bars * BAR;
    song.bars = c.bars;
    song.totalTicks = c.total;
    song.bpm = g.bpm;
    song.keyRoot = pc (s.keyRoot);

    for (int l = 0; l < kNumLayers; ++l)
        song.pattern[(size_t) l] = resolvePattern (s, (Layer) l);

    // seed-only patterns fall back to the genre's default when no seed is loaded
    const bool haveSeed = seed && seed->valid;
    for (int l = 0; l < kNumLayers; ++l)
    {
        auto& p = song.pattern[(size_t) l];
        if (p >= 0 && p < (int) patternsFor ((Layer) l).size() && patternsFor ((Layer) l)[(size_t) p].usesSeed && ! haveSeed)
        {
            p = g.defaults[l];
            song.hints.push_back (std::string (layerName ((Layer) l)) + ": no seed loaded, using the genre pattern.");
        }
    }

    // ---- 1. harmony ------------------------------------------------------------------------------
    std::vector<ChordEvent> loop;
    int loopTicks = BAR;
    const bool fromSeed = s.progression == kProgressionSeed && haveSeed;
    if (s.progression == kProgressionSeed && ! haveSeed)
        song.hints.push_back ("Progression: no seed loaded, using the genre pick.");

    if (fromSeed)
    {
        int delta = pc (s.keyRoot - seed->keyRoot);
        if (delta > 6) delta -= 12;
        for (auto e : seed->chords)
        {
            e.chord.root = pc (e.chord.root + delta);
            if (e.chord.bass >= 0) e.chord.bass = pc (e.chord.bass + delta);
            loop.push_back (e);
        }
        loopTicks = seed->bars * BAR;
        song.mode = seed->mode;
        song.progressionLabel = "From seed";
    }
    else
    {
        const auto& cat = progressionCatalogue();
        int k;
        if (s.progression >= kProgressionFirst && s.progression - kProgressionFirst < (int) cat.size())
            k = s.progression - kProgressionFirst;
        else
        {
            Rng r { (uint64_t) s.songSeed, 101, (uint64_t) s.genre };
            std::vector<double> w;
            for (auto& pk : g.picks) w.push_back (pk.second);
            k = g.picks[(size_t) r.weighted (w)].first;
        }
        const auto& def = cat[(size_t) k];
        std::vector<Chord> chords;
        parseProgression (def.numerals, song.keyRoot, def.mode, chords);
        const int cl = chordLengthTicks (s.chordLength);
        for (size_t i = 0; i < chords.size(); ++i)
        {
            ChordEvent e;
            e.start = (int) i * cl;
            e.length = cl;
            e.chord = colourChord (chords[i], g.voicing, song.keyRoot, def.mode);
            loop.push_back (e);
        }
        loopTicks = (int) chords.size() * cl;
        song.mode = def.mode;
        song.progressionLabel = prettyNumerals (def.numerals);
    }
    if (loop.empty())
    {
        ChordEvent e;
        e.chord.root = song.keyRoot;
        loop.push_back (e);
        loopTicks = BAR;
    }
    song.flats = useFlats (song.keyRoot, song.mode);
    song.loopBars = std::max (1, loopTicks / BAR);
    if (loopTicks > c.total)
        song.hints.push_back ("This progression is " + std::to_string (loopTicks / BAR) + " bars long; set BARS to "
                              + std::to_string (loopTicks / BAR) + " or more to hear all of it.");

    // voice leading: two passes so the loop joins smoothly with itself
    {
        std::vector<int> prev;
        int prevSub = -1;
        for (int pass = 0; pass < 2; ++pass)
            for (auto& e : loop)
            {
                // added 9ths sit on top of the triad instead of clustering inside it
                std::vector<int> core;
                std::vector<int> ext;
                for (int iv : e.chord.intervals)
                    (iv >= 12 ? ext : core).push_back (pc (e.chord.root + iv));
                std::vector<int> prevCore = prev;
                if (! ext.empty() && prevCore.size() > 1) prevCore.pop_back();
                e.voicing = voiceChord (core, e.chord.root, g.chordRange.lo, g.chordRange.hi - (ext.empty() ? 0 : 3),
                                        prevCore, 4);
                for (int x : ext)
                    e.voicing.push_back (placeInRange (x, e.voicing.back() + 1));
                prev = e.voicing;

                // sub / bass: nearest octave to the previous root, inside a fixed window
                const int lo = g.subLow + 1, hi = g.subLow + 14;
                int p = prevSub < 0 ? placeInRange (e.chord.bassPc(), g.subLow) : nearestWithPc (e.chord.bassPc(), prevSub);
                while (p < lo) p += 12;
                while (p > hi) p -= 12;
                e.subPitch = p;
                e.bassPitch = p + (g.bassLow - g.subLow);
                prevSub = p;
            }
    }
    for (int base = 0; base < c.total; base += loopTicks)
        for (auto e : loop)
        {
            e.start += base;
            if (e.start >= c.total) break;
            e.length = std::min (e.length, c.total - e.start);
            song.chords.push_back (e);
        }
    {
        std::string names;
        for (auto& e : loop)
        {
            if (! names.empty()) names += "  ";
            names += e.chord.name (song.flats);
        }
        song.chordNames = names;
    }

    // ---- 2. skeleton: groove points + kicks ------------------------------------------------------
    for (int r = 0; r < 4; ++r)
    {
        std::vector<int> order;
        int ti = 0;
        for (auto tier : g.grooveTiers)
        {
            Rng rng { (uint64_t) s.grooveSeed, 501, (uint64_t) s.genre, (uint64_t) r, (uint64_t) ti++ };
            rng.shuffle (tier);
            for (int t : tier) order.push_back (t);
        }
        c.groove[(size_t) r] = order;
    }
    {
        int dp = song.pattern[(size_t) Layer::Drums];
        if (dp == DrOff) dp = g.defaults[(int) Layer::Drums];
        if (dp == DrOff) dp = DrHalfTime;
        const auto& def = drumDef (dp);
        for (int bar = 0; bar < c.bars; ++bar)
            for (int st : ((bar % 2 == 0) ? def.a : def.b).kick)
                song.kicks.push_back (bar * BAR + st * STEP);
    }
    computePushTicks (c, song.pattern[(size_t) Layer::Chords]);

    // ---- 3. layers ---------------------------------------------------------------------------------
    // the mid-bass line is shared by BASS A and BASS B (and SUB "Follow Bass A")
    int aPattern = song.pattern[(size_t) Layer::BassA];
    const bool aOn = aPattern != BassOff;
    int linePattern = aOn ? aPattern : g.defaults[(int) Layer::BassA];
    if (linePattern == BassOff) linePattern = BassLongHold;
    std::vector<GN> midLine = buildMidLine (c, linePattern);
    moveMidLine (midLine, c);

    std::vector<GN> bassA, bassB;
    splitAB (c, midLine, song.pattern[(size_t) Layer::BassB], aOn, bassA, bassB);

    auto finishMono = [&] (std::vector<GN> v, Layer l) {
        applyLength (v, c.len (l));
        glides (v, c, c.mv (l), c.len (l), l == Layer::Sub ? 0.6 : 0.5);
        return finish (v, c, s.layers[(size_t) l].octave);
    };

    song.layers[(size_t) Layer::BassA] = finishMono (bassA, Layer::BassA);
    song.layers[(size_t) Layer::BassB] = song.pattern[(size_t) Layer::BassB] == BOff ? std::vector<Note> {}
                                                                                  : finishMono (bassB, Layer::BassB);
    song.layers[(size_t) Layer::Sub] = finishMono (buildSub (c, song.pattern[(size_t) Layer::Sub], midLine), Layer::Sub);

    {
        auto ch = buildChords (c, song.pattern[(size_t) Layer::Chords]);
        applyLength (ch, c.len (Layer::Chords));
        song.layers[(size_t) Layer::Chords] = finish (ch, c, s.layers[(size_t) Layer::Chords].octave);
    }
    {
        auto ld = buildLead (c, song.pattern[(size_t) Layer::Lead]);
        applyLength (ld, c.len (Layer::Lead));
        glides (ld, c, c.mv (Layer::Lead), c.len (Layer::Lead), 0.75);
        song.layers[(size_t) Layer::Lead] = finish (ld, c, s.layers[(size_t) Layer::Lead].octave);
    }
    song.layers[(size_t) Layer::Arp] = finish (buildArp (c, song.pattern[(size_t) Layer::Arp]), c, s.layers[(size_t) Layer::Arp].octave);
    song.layers[(size_t) Layer::Drums] = finish (buildDrums (c, song.pattern[(size_t) Layer::Drums]), c, 0);
    song.layers[(size_t) Layer::FX] = finish (buildFx (c, song.pattern[(size_t) Layer::FX]), c, 0);
    return song;
}
} // namespace te
