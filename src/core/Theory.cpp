#include "Theory.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <set>
#include <sstream>

namespace te
{
// ---- Chord -----------------------------------------------------------------------------------
std::vector<int> Chord::pitchClasses() const
{
    std::vector<int> out;
    for (int iv : intervals)
    {
        int p = pc (root + iv);
        if (std::find (out.begin(), out.end(), p) == out.end())
            out.push_back (p);
    }
    return out;
}

bool Chord::contains (int pitchClass) const
{
    int p = pc (pitchClass);
    for (int iv : intervals)
        if (pc (root + iv) == p)
            return true;
    return false;
}

bool Chord::hasMinorThird() const
{
    bool m3 = false, M3 = false;
    for (int iv : intervals)
    {
        if (pc (iv) == 3) m3 = true;
        if (pc (iv) == 4) M3 = true;
    }
    return m3 && ! M3;
}

bool Chord::isPlainTriad() const
{
    return intervals == std::vector<int> { 0, 3, 7 } || intervals == std::vector<int> { 0, 4, 7 }
        || intervals == std::vector<int> { 0, 3, 6 };
}

std::string Chord::suffix() const
{
    struct Q { std::vector<int> iv; const char* s; };
    static const Q table[] = {
        { { 0, 4, 7 }, "" },          { { 0, 3, 7 }, "m" },          { { 0, 3, 6 }, "dim" },
        { { 0, 4, 8 }, "aug" },       { { 0, 2, 7 }, "sus2" },       { { 0, 5, 7 }, "sus4" },
        { { 0, 7 }, "5" },            { { 0, 7, 12 }, "5" },         { { 0, 4, 7, 11 }, "maj7" },
        { { 0, 3, 7, 10 }, "m7" },    { { 0, 4, 7, 10 }, "7" },      { { 0, 3, 6, 10 }, "m7b5" },
        { { 0, 3, 6, 9 }, "dim7" },   { { 0, 4, 7, 14 }, "add9" },   { { 0, 3, 7, 14 }, "madd9" },
        { { 0, 3, 7, 10, 14 }, "m9" },{ { 0, 4, 7, 11, 14 }, "maj9" },{ { 0, 4, 7, 10, 14 }, "9" },
        { { 0, 2, 7, 10 }, "7sus2" }, { { 0, 5, 7, 10 }, "7sus4" },
    };
    for (auto& q : table)
        if (q.iv == intervals)
            return q.s;
    return hasMinorThird() ? "m" : "";
}

std::string Chord::name (bool flats) const
{
    std::string n = noteName (root, flats) + suffix();
    if (bass >= 0 && bass != root)
        n += "/" + noteName (bass, flats);
    return n;
}

bool Chord::sameAs (const Chord& o) const
{
    return root == o.root && intervals == o.intervals && bassPc() == o.bassPc();
}

// ---- scales & names ----------------------------------------------------------------------------
const std::vector<int>& scaleSteps (Mode mode)
{
    static const std::vector<int> minor { 0, 2, 3, 5, 7, 8, 10 };
    static const std::vector<int> major { 0, 2, 4, 5, 7, 9, 11 };
    return mode == Mode::Minor ? minor : major;
}

std::vector<int> keyScale (int keyRoot, Mode mode)
{
    std::vector<int> out;
    for (int s : scaleSteps (mode))
        out.push_back (pc (keyRoot + s));
    return out;
}

std::vector<int> chordScale (const Chord& chord, int keyRoot, Mode mode)
{
    std::set<int> scale;
    for (int p : keyScale (keyRoot, mode))
        scale.insert (p);
    auto tones = chord.pitchClasses();
    auto isTone = [&] (int p) { return std::find (tones.begin(), tones.end(), p) != tones.end(); };
    for (int t : tones)
    {
        if (scale.count (t)) continue;
        scale.insert (t);
        for (int nb : { pc (t + 1), pc (t - 1) })
            if (! isTone (nb))
                scale.erase (nb);
    }
    // avoid notes: a scale tone a semitone above a chord tone (b9 over the root, 4 over a major 3rd...)
    for (int t : tones)
        if (! isTone (pc (t + 1)))
            scale.erase (pc (t + 1));
    std::vector<int> out (scale.begin(), scale.end());
    return out;
}

bool useFlats (int keyRoot, Mode mode)
{
    int r = pc (keyRoot);
    if (mode == Mode::Major)
        return r == 5 || r == 10 || r == 3 || r == 8 || r == 1 || r == 6;
    // minor keys whose relative major uses flats: D G C F Bb Eb (and Ab/Db minor rarely)
    return r == 2 || r == 7 || r == 0 || r == 5 || r == 10 || r == 3 || r == 8;
}

std::string noteName (int pitchClass, bool flats)
{
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    int p = pc (pitchClass);
    return flats ? flatNames[p] : sharpNames[p];
}

std::string keyName (int keyRoot, Mode mode)
{
    return noteName (keyRoot, useFlats (keyRoot, mode)) + (mode == Mode::Minor ? " minor" : " major");
}

std::string pitchName (int midiPitch, bool flats)
{
    int octave = (int) std::floor (midiPitch / 12.0) - 2; // Ableton: C3 = 60
    return noteName (pc (midiPitch), flats) + std::to_string (octave);
}

// ---- roman numerals ----------------------------------------------------------------------------
static int romanValue (const std::string& upper)
{
    static const char* r[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    for (int i = 0; i < 7; ++i)
        if (upper == r[i])
            return i;
    return -1;
}

bool parseNumeral (const std::string& tokenIn, int keyRoot, Mode mode, Chord& out)
{
    std::string token = tokenIn;
    size_t i = 0;
    int accidental = 0;
    bool hasAccidental = false;
    while (i < token.size() && (token[i] == 'b' || token[i] == '#'))
    {
        accidental += token[i] == 'b' ? -1 : 1;
        hasAccidental = true;
        ++i;
    }
    size_t numStart = i;
    while (i < token.size() && (token[i] == 'I' || token[i] == 'V' || token[i] == 'i' || token[i] == 'v'))
        ++i;
    if (i == numStart)
        return false;

    std::string numeral = token.substr (numStart, i - numStart);
    bool upper = std::isupper ((unsigned char) numeral[0]) != 0;
    std::string up = numeral;
    for (auto& c : up) c = (char) std::toupper ((unsigned char) c);
    int degree = romanValue (up);
    if (degree < 0)
        return false;

    std::string rest = token.substr (i);
    static const std::vector<int> majorSteps { 0, 2, 4, 5, 7, 9, 11 };
    int semis = hasAccidental ? majorSteps[(size_t) degree] + accidental : scaleSteps (mode)[(size_t) degree];

    Chord c;
    c.root = pc (keyRoot + semis);
    c.intervals = upper ? std::vector<int> { 0, 4, 7 } : std::vector<int> { 0, 3, 7 };

    if (rest.empty()) {}
    else if (rest == "o" || rest == "dim" || rest == "\xC2\xB0") c.intervals = { 0, 3, 6 };
    else if (rest == "sus2") c.intervals = { 0, 2, 7 };
    else if (rest == "sus4") c.intervals = { 0, 5, 7 };
    else if (rest == "5") c.intervals = { 0, 7 };
    else if (rest == "7") c.intervals = upper ? std::vector<int> { 0, 4, 7, 10 } : std::vector<int> { 0, 3, 7, 10 };
    else if (rest == "maj7") c.intervals = upper ? std::vector<int> { 0, 4, 7, 11 } : std::vector<int> { 0, 3, 7, 11 };
    else if (rest == "add9") c.intervals.push_back (14);
    else if (rest == "9") c.intervals = upper ? std::vector<int> { 0, 4, 7, 10, 14 } : std::vector<int> { 0, 3, 7, 10, 14 };
    else if (rest == "o7" || rest == "dim7") c.intervals = { 0, 3, 6, 9 };
    else return false;

    out = c;
    return true;
}

bool parseProgression (const std::string& numerals, int keyRoot, Mode mode, std::vector<Chord>& out)
{
    out.clear();
    std::istringstream ss (numerals);
    std::string tok;
    while (ss >> tok)
    {
        Chord c;
        if (! parseNumeral (tok, keyRoot, mode, c))
            return false;
        out.push_back (c);
    }
    return ! out.empty();
}

Chord colourChord (const Chord& c, Voicing v, int keyRoot, Mode mode)
{
    if (! c.isPlainTriad() || c.intervals == std::vector<int> { 0, 3, 6 })
        return c;
    Chord r = c;
    auto scale = keyScale (keyRoot, mode);
    auto inScale = [&] (int p) { return std::find (scale.begin(), scale.end(), pc (p)) != scale.end(); };
    switch (v)
    {
        case Voicing::Lush:
            if (inScale (c.root + 2)) r.intervals.push_back (14);
            break;
        case Voicing::Seventh:
            if (inScale (c.root + 10)) r.intervals.push_back (10);
            else if (inScale (c.root + 11)) r.intervals.push_back (11);
            break;
        case Voicing::Triad:
        case Voicing::OctaveTriad:
            break;
    }
    return r;
}

// ---- pitch helpers -----------------------------------------------------------------------------
int placeInRange (int pitchClass, int lo) noexcept
{
    int p = lo + pc (pitchClass - lo);
    return p;
}

int nearestWithPc (int pitchClass, int target) noexcept
{
    int below = target - pc (target - pitchClass);
    int above = below + 12;
    if (below == target) return target;
    return (target - below) <= (above - target) ? below : above;
}

int nearestInSet (const std::vector<int>& pcs, int target, int bias) noexcept
{
    int best = target, bestD = INT_MAX;
    for (int d = 0; d <= 12; ++d)
    {
        for (int sgn : { bias >= 0 ? 1 : -1, bias >= 0 ? -1 : 1 })
        {
            int p = target + sgn * d;
            if (std::find (pcs.begin(), pcs.end(), pc (p)) != pcs.end() && d < bestD)
            {
                best = p;
                bestD = d;
            }
        }
        if (bestD != INT_MAX) break;
    }
    return best;
}

int stepInSet (const std::vector<int>& pcs, int from, int steps) noexcept
{
    int p = from;
    int dir = steps >= 0 ? 1 : -1;
    int n = steps >= 0 ? steps : -steps;
    for (int k = 0; k < n; ++k)
    {
        for (int d = 1; d <= 12; ++d)
        {
            int q = p + dir * d;
            if (std::find (pcs.begin(), pcs.end(), pc (q)) != pcs.end())
            {
                p = q;
                break;
            }
        }
    }
    return p;
}

std::vector<int> voiceChord (const std::vector<int>& pcsIn, int rootPc, int lo, int hi,
                             const std::vector<int>& prev, int maxNotes)
{
    // order pitch classes from the root upwards
    std::vector<int> pcs;
    for (int p : pcsIn)
        if (std::find (pcs.begin(), pcs.end(), pc (p)) == pcs.end())
            pcs.push_back (pc (p));
    std::sort (pcs.begin(), pcs.end(), [rootPc] (int a, int b) { return pc (a - rootPc) < pc (b - rootPc); });
    // too many tones: drop the fifth first, then the highest extension
    while ((int) pcs.size() > maxNotes && pcs.size() > 2)
    {
        auto fifth = std::find (pcs.begin(), pcs.end(), pc (rootPc + 7));
        if (fifth != pcs.end() && pcs.size() > 3) pcs.erase (fifth);
        else pcs.pop_back();
    }

    const int n = (int) pcs.size();
    const double center = (lo + hi) * 0.5;
    std::vector<int> best;
    double bestCost = 1e18;

    for (int inv = 0; inv < n; ++inv)
    {
        for (int base = lo; base <= hi; ++base)
        {
            if (pc (base) != pcs[(size_t) inv]) continue;
            std::vector<int> v { base };
            for (int k = 1; k < n; ++k)
            {
                int want = pcs[(size_t) ((inv + k) % n)];
                int p = v.back() + 1;
                while (pc (p) != want) ++p;
                v.push_back (p);
            }
            if (v.back() > hi) continue;

            double mean = 0;
            for (int p : v) mean += p;
            mean /= n;
            double cost = 0;
            if (prev.empty())
            {
                cost = std::fabs (mean - (center - 2)) + (inv == 0 ? 0.0 : 2.5);
            }
            else
            {
                for (int p : v)
                {
                    int d = INT_MAX;
                    for (int q : prev) d = std::min (d, std::abs (p - q));
                    cost += d;
                }
                for (int q : prev)
                {
                    int d = INT_MAX;
                    for (int p : v) d = std::min (d, std::abs (p - q));
                    cost += 0.5 * d;
                }
                cost += 0.35 * std::fabs (mean - center);
            }
            if (cost < bestCost)
            {
                bestCost = cost;
                best = v;
            }
        }
    }
    if (best.empty())
    {
        // range too narrow: stack from lo
        int p = placeInRange (pcs[0], lo);
        best.push_back (p);
        for (int k = 1; k < n; ++k)
        {
            int q = best.back() + 1;
            while (pc (q) != pcs[(size_t) k]) ++q;
            best.push_back (q);
        }
    }
    return best;
}
} // namespace te
