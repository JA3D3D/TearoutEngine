#include "Sample.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace te
{
PitchGuess detectPitch (const std::vector<float>& mono, double sampleRate)
{
    PitchGuess g;
    if (mono.size() < 2048 || sampleRate <= 0) return g;

    // decimate to ~11 kHz (box filter), skip the attack, analyse up to 0.7 s of the body
    const int factor = std::max (1, (int) std::floor (sampleRate / 11025.0));
    const double sr = sampleRate / factor;
    const size_t start = std::min (mono.size() / 6, (size_t) (sampleRate * 0.12));
    std::vector<float> x;
    for (size_t i = start; i + (size_t) factor <= mono.size() && x.size() < (size_t) (sr * 0.7); i += (size_t) factor)
    {
        float s = 0;
        for (int k = 0; k < factor; ++k) s += mono[i + (size_t) k];
        x.push_back (s / (float) factor);
    }
    const int tauMin = std::max (2, (int) (sr / 1000.0));
    const int tauMax = (int) (sr / 20.0);
    const int W = (int) x.size() - tauMax - 2;
    if (W < 256) return g;

    std::vector<double> d ((size_t) tauMax + 2, 0.0), cmnd ((size_t) tauMax + 2, 1.0);
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        double sum = 0;
        for (int j = 0; j < W; ++j)
        {
            const double diff = (double) x[(size_t) j] - (double) x[(size_t) (j + tau)];
            sum += diff * diff;
        }
        d[(size_t) tau] = sum;
    }
    double running = 0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        running += d[(size_t) tau];
        cmnd[(size_t) tau] = running > 0 ? d[(size_t) tau] * tau / running : 1.0;
    }
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
        if (cmnd[(size_t) tau] < 0.12)
        {
            while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        double m = 1e9;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (cmnd[(size_t) tau] < m) { m = cmnd[(size_t) tau]; best = tau; }
    }
    if (best <= 0) return g;

    double t = best;
    if (best > 1 && best < tauMax)
    {
        const double a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const double den = a - 2 * b + c;
        if (std::fabs (den) > 1e-12) t = best + 0.5 * (a - c) / den;
    }
    g.hz = sr / t;
    g.confidence = std::clamp (1.0 - cmnd[(size_t) best], 0.0, 1.0);

    // refine: the frequency within +-8 % whose first 8 harmonics carry the most energy (windowed DFT).
    // Growly, modulated basses can fool the time-domain estimate by a semitone; the harmonic comb can't.
    {
        const size_t N = std::min (x.size(), (size_t) sr); // up to 1 s
        std::vector<double> w (N);
        for (size_t i = 0; i < N; ++i) w[i] = x[i] * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (N - 1)));
        auto power = [&] (double f) {
            const double k = 2.0 * 3.14159265358979 * f / sr;
            const double coeff = 2.0 * std::cos (k);
            double s1 = 0, s2 = 0;
            for (size_t i = 0; i < N; ++i)
            {
                const double s0 = w[i] + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - coeff * s1 * s2;
        };
        double bestF = g.hz, bestScore = -1;
        for (double f = g.hz * 0.92; f <= g.hz * 1.08; f *= 1.0015)
        {
            double score = 0;
            for (int h = 1; h <= 8 && h * f < sr * 0.45; ++h) score += std::sqrt (power (h * f));
            if (score > bestScore) { bestScore = score; bestF = f; }
        }
        g.hz = bestF;
    }
    const double m = 69.0 + 12.0 * std::log2 (g.hz / 440.0);
    g.midi = (int) std::lround (m);
    if (g.midi < 0 || g.midi > 127) g.midi = -1;
    return g;
}

int noteFromFileName (const std::string& path, int& octave)
{
    octave = -99;
    std::string name = path;
    const auto slash = name.find_last_of ("/\\");
    if (slash != std::string::npos) name = name.substr (slash + 1);
    const auto dot = name.find_last_of ('.');
    if (dot != std::string::npos) name = name.substr (0, dot);

    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    int found = -1;
    for (size_t i = 0; i < name.size(); ++i)
    {
        const char c = name[i];
        if (c < 'A' || c > 'G') continue;
        if (i > 0 && std::isalnum ((unsigned char) name[i - 1])) continue;
        size_t j = i + 1;
        int pcv = base[c - 'A'];
        if (j < name.size() && (name[j] == '#' || name[j] == 'b'))
        {
            // "Bb" is a flat; "Bass" is not (the b would be followed by a letter)
            const bool isFlat = name[j] == 'b';
            const bool nextIsLetter = j + 1 < name.size() && std::isalpha ((unsigned char) name[j + 1]);
            if (! isFlat || ! nextIsLetter || (j + 1 < name.size() && name[j + 1] == 'm'))
            {
                pcv += isFlat ? -1 : 1;
                ++j;
            }
        }
        int oct = -99;
        size_t k = j;
        bool neg = false;
        if (k < name.size() && name[k] == '-' && k + 1 < name.size() && std::isdigit ((unsigned char) name[k + 1])) { neg = true; ++k; }
        if (k < name.size() && std::isdigit ((unsigned char) name[k]))
        {
            oct = name[k] - '0';
            if (neg) oct = -oct;
            j = k + 1;
        }
        // allow a chord-quality suffix: Am, C#min, Fmaj, Dminor
        for (const char* suf : { "minor", "major", "min", "maj", "m" })
        {
            const std::string sfx (suf);
            if (name.compare (j, sfx.size(), sfx) == 0) { j += sfx.size(); break; }
        }
        if (j < name.size() && std::isalnum ((unsigned char) name[j])) continue;
        found = ((pcv % 12) + 12) % 12;
        octave = oct; // the last match wins: note names usually sit at the end
    }
    if (found < 0) octave = -99;
    return found;
}

RootChoice chooseRoot (const std::vector<float>& mono, double sampleRate, const std::string& fileName)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto noteText = [] (int m) { return std::string (names[((m % 12) + 12) % 12]) + std::to_string (m / 12 - 2); };
    RootChoice r;
    const auto g = detectPitch (mono, sampleRate);
    int nameOct = -99;
    const int namePc = noteFromFileName (fileName, nameOct);
    if (g.midi >= 0 && g.confidence >= 0.6)
    {
        r.midi = g.midi;
        r.detected = true;
        char buf[64];
        std::snprintf (buf, sizeof buf, "%.1f Hz", g.hz);
        r.reason = "detected " + noteText (g.midi) + " (" + buf + ")";
        if (namePc >= 0 && namePc != g.midi % 12)
            r.reason += ", file name says " + std::string (names[namePc]);
        return r;
    }
    if (namePc >= 0)
    {
        if (nameOct != -99) r.midi = (nameOct + 2) * 12 + namePc;
        else if (g.midi >= 0) r.midi = nearestWithPc (namePc, g.midi);
        else r.midi = 36 + namePc;
        r.reason = "from the file name (" + noteText (r.midi) + ")";
        return r;
    }
    if (g.midi >= 0)
    {
        r.midi = g.midi;
        r.detected = true;
        r.reason = "best guess " + noteText (g.midi);
        return r;
    }
    r.midi = 60;
    r.reason = "no pitch found, using C3";
    return r;
}

std::shared_ptr<SampleData> makeSampleData (std::vector<float> left, std::vector<float> right, double sampleRate, int rootNote)
{
    if (right.size() != left.size()) right = left;
    const size_t maxFrames = (size_t) (sampleRate * 30.0);
    if (left.size() > maxFrames) { left.resize (maxFrames); right.resize (maxFrames); }
    auto out = std::make_shared<SampleData>();
    out->left = std::move (left);
    out->right = std::move (right);
    out->sampleRate = sampleRate;
    out->rootNote = rootNote;
    const size_t n = out->left.size();
    if (n < 64) return out;

    // envelope in 50 ms windows
    const size_t win = std::max<size_t> (64, (size_t) (sampleRate * 0.05));
    std::vector<float> env;
    for (size_t i = 0; i + win <= n; i += win)
    {
        double e = 0;
        for (size_t k = i; k < i + win; ++k)
        {
            const double m = 0.5 * (out->left[k] + out->right[k]);
            e += m * m;
        }
        env.push_back ((float) std::sqrt (e / (double) win));
    }
    float peak = 0;
    for (float e : env) peak = std::max (peak, e);
    if (peak <= 0) return out;
    size_t a = env.size(), b = 0;
    for (size_t i = 0; i < env.size(); ++i)
        if (env[i] >= 0.7f * peak) { a = std::min (a, i); b = i; }

    // level: steady part at a comfortable preview level
    double sum = 0;
    size_t cnt = 0;
    for (size_t i = a; i <= b && i < env.size(); ++i) { sum += env[i]; ++cnt; }
    const double rms = cnt ? sum / (double) cnt : peak;
    out->gain = (float) std::clamp (0.3 / std::max (1e-4, rms), 0.05, 6.0);

    // sustain loop over the steady part (needs at least ~0.3 s), crossfaded
    if (b > a + 6)
    {
        const size_t ls = (a + (b - a) / 4) * win, le = b * win;
        const size_t xf = std::min<size_t> ((size_t) (sampleRate * 0.04), std::min (ls, (le - ls) / 2));
        for (size_t i = 0; i < xf; ++i)
        {
            const float t = (float) i / (float) xf;
            out->left[le - xf + i] = out->left[le - xf + i] * (1 - t) + out->left[ls - xf + i] * t;
            out->right[le - xf + i] = out->right[le - xf + i] * (1 - t) + out->right[ls - xf + i] * t;
        }
        out->loopStart = (int) ls;
        out->loopEnd = (int) le;
    }
    return out;
}
} // namespace te
