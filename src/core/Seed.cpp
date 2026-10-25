// Seed MIDI analysis: key, chords, bass rhythm, top line.
#include "Engine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace te
{
namespace
{
int snapTick (int t)
{
    const int g16 = (int) std::lround (t / (double) STEP) * STEP;
    const int g12 = (int) std::lround (t / (double) TRIP8) * TRIP8;
    return std::abs (t - g16) <= std::abs (t - g12) + 2 ? g16 : g12;
}

double correlate (const double* x, const double* y)
{
    double mx = 0, my = 0;
    for (int i = 0; i < 12; ++i) { mx += x[i]; my += y[i]; }
    mx /= 12; my /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; ++i)
    {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    if (sxx <= 0 || syy <= 0) return 0;
    return sxy / std::sqrt (sxx * syy);
}

struct Template { std::vector<int> iv; double complexity; };
const std::vector<Template>& templates()
{
    static const std::vector<Template> t {
        { { 0, 4, 7 }, 0.0 },  { { 0, 3, 7 }, 0.0 },     { { 0, 3, 6 }, 0.15 },    { { 0, 2, 7 }, 0.12 },
        { { 0, 5, 7 }, 0.12 }, { { 0, 7 }, 0.22 },       { { 0, 4, 7, 11 }, 0.08 }, { { 0, 3, 7, 10 }, 0.08 },
        { { 0, 4, 7, 10 }, 0.1 },
    };
    return t;
}

Chord diatonicTriad (int rootPc, int keyRoot, Mode mode)
{
    auto sc = keyScale (keyRoot, mode);
    auto it = std::find (sc.begin(), sc.end(), pc (rootPc));
    Chord c;
    c.root = pc (rootPc);
    if (it == sc.end())
    {
        c.intervals = { 0, 4, 7 };
        return c;
    }
    int i = (int) (it - sc.begin());
    int third = pc (sc[(size_t) ((i + 2) % 7)] - rootPc);
    int fifth = pc (sc[(size_t) ((i + 4) % 7)] - rootPc);
    c.intervals = { 0, third, fifth };
    return c;
}
} // namespace

SeedAnalysis analyseSeed (const std::vector<Note>& input)
{
    SeedAnalysis a;
    if (input.empty()) return a;

    // ---- normalise + quantise --------------------------------------------------------------------
    int minStart = input.front().start;
    for (auto& n : input) minStart = std::min (minStart, n.start);
    const int offset = (minStart / BAR) * BAR;
    std::vector<Note> notes;
    for (auto n : input)
    {
        int s = snapTick (n.start - offset);
        int e = snapTick (n.end() - offset);
        if (e <= s) e = s + STEP;
        n.start = s;
        n.length = e - s;
        n.velocity = std::clamp (n.velocity, 1, 127);
        notes.push_back (n);
    }
    int maxEnd = 0;
    for (auto& n : notes) maxEnd = std::max (maxEnd, n.end());
    int bars = std::max (1, (maxEnd + BAR - 1) / BAR);
    int loopBars = 1;
    while (loopBars < bars && loopBars < 16) loopBars *= 2;
    const int loopTicks = loopBars * BAR;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const Note& n) { return n.start >= loopTicks; }), notes.end());
    for (auto& n : notes) n.length = std::min (n.length, loopTicks - n.start);
    std::sort (notes.begin(), notes.end(), [] (const Note& x, const Note& y) {
        return x.start != y.start ? x.start < y.start : x.pitch < y.pitch; });
    if (notes.empty()) return a;
    a.notes = notes;
    a.bars = loopBars;

    // onsets and which notes are the lowest / highest at each onset
    std::vector<int> onsets;
    for (auto& n : notes) onsets.push_back (n.start);
    std::sort (onsets.begin(), onsets.end());
    onsets.erase (std::unique (onsets.begin(), onsets.end()), onsets.end());

    auto lowestStartingAt = [&] (int t, bool lowest) -> const Note* {
        const Note* best = nullptr;
        for (auto& n : notes)
            if (n.start == t && (! best || (lowest ? n.pitch < best->pitch : n.pitch > best->pitch)))
                best = &n;
        return best;
    };
    auto coveredBy = [&] (int t, int pitch, bool lowest) {
        for (auto& n : notes)
            if (n.start < t && n.end() > t && (lowest ? n.pitch < pitch : n.pitch > pitch))
                return true;
        return false;
    };

    // ---- key ---------------------------------------------------------------------------------------
    static const double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double w[12] = {};
    for (auto& n : notes) w[pc (n.pitch)] += n.length;
    for (int t : onsets)
        if (auto* lo = lowestStartingAt (t, true))
            if (! coveredBy (t, lo->pitch, true))
                w[pc (lo->pitch)] += 0.5 * lo->length;

    double bestScore = -2;
    int bestRoot = 0;
    Mode bestMode = Mode::Minor;
    double scores[2][12];
    for (int r = 0; r < 12; ++r)
    {
        double x[12];
        for (int i = 0; i < 12; ++i) x[i] = w[pc (i + r)];
        scores[0][r] = correlate (x, minorProfile);
        scores[1][r] = correlate (x, majorProfile);
        for (int m = 0; m < 2; ++m)
            if (scores[m][r] > bestScore)
            {
                bestScore = scores[m][r];
                bestRoot = r;
                bestMode = m == 0 ? Mode::Minor : Mode::Major;
            }
    }
    a.keyRoot = bestRoot;
    a.mode = bestMode;

    // ---- chords per half bar ------------------------------------------------------------------------
    const int segLen = BAR / 2;
    const int nSeg = loopTicks / segLen;
    std::vector<Chord> segChords ((size_t) nSeg);
    std::vector<bool> segEmpty ((size_t) nSeg, true);
    for (int si = 0; si < nSeg; ++si)
    {
        const int s0 = si * segLen, s1 = s0 + segLen;
        double sw[12] = {}, bw[12] = {};
        double W = 0;
        for (auto& n : notes)
        {
            int ov = std::min (n.end(), s1) - std::max (n.start, s0);
            if (ov > 0) { sw[pc (n.pitch)] += ov; W += ov; }
        }
        if (W <= 0) continue;
        segEmpty[(size_t) si] = false;
        double BW = 0;
        for (int t = s0; t < s1; t += STEP)
        {
            int low = 999;
            for (auto& n : notes)
                if (n.start <= t && n.end() > t) low = std::min (low, n.pitch);
            if (low < 999) { bw[pc (low)] += STEP; BW += STEP; }
        }
        int significant = 0;
        for (int i = 0; i < 12; ++i) if (sw[i] >= 0.12 * W) ++significant;
        if (significant <= 1)
        {
            int root = 0;
            double best = -1;
            for (int i = 0; i < 12; ++i)
            {
                double v = sw[i] + (BW > 0 ? bw[i] : 0);
                if (v > best) { best = v; root = i; }
            }
            segChords[(size_t) si] = diatonicTriad (root, a.keyRoot, a.mode);
            continue;
        }
        auto sc = keyScale (a.keyRoot, a.mode);
        double bestS = -1e18;
        Chord bestC;
        for (int r = 0; r < 12; ++r)
            for (auto& tp : templates())
            {
                double in = 0, out = 0;
                int missing = 0;
                bool diatonic = true;
                std::vector<int> tpcs;
                for (int iv : tp.iv) tpcs.push_back (pc (r + iv));
                for (int p : tpcs)
                {
                    in += sw[p];
                    if (sw[p] < 0.05 * W) ++missing;
                    if (std::find (sc.begin(), sc.end(), p) == sc.end()) diatonic = false;
                }
                for (int p = 0; p < 12; ++p)
                    if (std::find (tpcs.begin(), tpcs.end(), p) == tpcs.end()) out += sw[p];
                double score = in - 0.8 * out - tp.complexity * W - 0.5 * W * missing / (double) tpcs.size();
                if (BW > 0) score += 0.4 * W * (bw[r] / BW);
                if (diatonic) score += 0.05 * W;
                if (score > bestS)
                {
                    bestS = score;
                    bestC.root = r;
                    bestC.intervals = tp.iv;
                }
            }
        segChords[(size_t) si] = bestC;
    }
    // empty half bars carry the previous chord (or the next one at the start)
    int firstFull = -1;
    for (int si = 0; si < nSeg; ++si) if (! segEmpty[(size_t) si]) { firstFull = si; break; }
    if (firstFull < 0) return a;
    for (int si = 0; si < nSeg; ++si)
    {
        if (! segEmpty[(size_t) si]) continue;
        segChords[(size_t) si] = si < firstFull ? segChords[(size_t) firstFull] : segChords[(size_t) si - 1];
    }
    for (int si = 0; si < nSeg; ++si)
    {
        if (! a.chords.empty() && a.chords.back().chord.sameAs (segChords[(size_t) si]))
        {
            a.chords.back().length += segLen;
            continue;
        }
        ChordEvent e;
        e.start = si * segLen;
        e.length = segLen;
        e.chord = segChords[(size_t) si];
        a.chords.push_back (e);
    }

    // relative major / minor tie-break: whichever tonic the clip actually starts (or ends) on
    {
        const int rel = a.mode == Mode::Minor ? pc (a.keyRoot + 3) : pc (a.keyRoot + 9);
        const Mode relMode = a.mode == Mode::Minor ? Mode::Major : Mode::Minor;
        const double relScore = scores[relMode == Mode::Minor ? 0 : 1][rel];
        // loops are heard from their first chord: if the clip opens on the relative tonic, with the
        // right quality (minor chord for a minor key), that's home
        const auto& firstChord = a.chords.front().chord;
        const bool qualityFits = (relMode == Mode::Minor) == firstChord.hasMinorThird();
        if (bestScore - relScore < 0.25 && firstChord.root == rel && qualityFits)
        {
            a.keyRoot = rel;
            a.mode = relMode;
        }
    }

    // ---- lines ---------------------------------------------------------------------------------------
    for (int t : onsets)
    {
        const Note* lo = lowestStartingAt (t, true);
        if (lo && ! coveredBy (t, lo->pitch, true))
            a.bassLine.push_back (*lo);
        const Note* hi = lowestStartingAt (t, false);
        if (hi && ! coveredBy (t, hi->pitch, false))
            a.topLine.push_back (*hi);
    }
    for (size_t i = 0; i < a.bassLine.size(); ++i)
    {
        int next = i + 1 < a.bassLine.size() ? a.bassLine[i + 1].start : loopTicks;
        a.bassLine[i].length = next - a.bassLine[i].start; // gapless
    }
    for (size_t i = 0; i + 1 < a.topLine.size(); ++i)
        a.topLine[i].length = std::min (a.topLine[i].length, a.topLine[i + 1].start - a.topLine[i].start);
    for (int t : onsets)
    {
        int count = 0;
        for (auto& n : notes) if (n.start == t) ++count;
        if (count >= 2) a.chordOnsets.push_back (t);
    }
    if (a.chordOnsets.empty())
        for (auto& n : a.bassLine) a.chordOnsets.push_back (n.start);

    // ---- summary -------------------------------------------------------------------------------------
    const bool flats = useFlats (a.keyRoot, a.mode);
    std::string names;
    int shown = 0;
    for (auto& e : a.chords)
    {
        if (shown == 8) { names += "\xE2\x80\x93..."; break; }
        if (! names.empty()) names += "\xE2\x80\x93";
        names += e.chord.name (flats);
        ++shown;
    }
    a.summary = keyName (a.keyRoot, a.mode) + "  \xC2\xB7  " + names + "  \xC2\xB7  " + std::to_string (a.bars)
              + (a.bars == 1 ? " bar" : " bars");
    a.valid = true;
    return a;
}
} // namespace te
