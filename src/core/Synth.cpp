#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace te
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

inline double midiHz (double p) { return 440.0 * std::pow (2.0, (p - 69.0) / 12.0); }

inline float polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float sawAt (double ph, double dt) { return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt); }

inline void advance (double& ph, double dt)
{
    ph += dt;
    if (ph >= 1.0) ph -= std::floor (ph);
}
} // namespace

const PreviewSynth::LayerCfg& PreviewSynth::cfg (int layer)
{
    //                         mono  glide  a      d     s     r     cutoff envAmt fdec  res   gain   drive wave
    static const LayerCfg c[] = {
        /* Chords */         { false, 0.0f,  0.02f, 0.5f, 0.75f, 0.28f, 2200, 1400, 0.6f, 0.10f, 0.034f, 1.0f, 3 },
        /* Sub    */         { true,  0.05f, 0.003f,0.1f, 1.0f,  0.04f, 0,    0,    0.1f, 0.0f,  0.50f,  1.2f, 0 },
        /* Bass A */         { true,  0.045f,0.003f,0.3f, 0.9f,  0.05f, 200,  2400, 0.22f,0.30f, 0.15f,  2.6f, 1 },
        /* Bass B */         { true,  0.04f, 0.002f,0.2f, 0.85f, 0.05f, 520,  3600, 0.12f,0.50f, 0.11f,  3.4f, 2 },
        /* Lead   */         { true,  0.03f, 0.005f,0.2f, 0.8f,  0.12f, 3200, 2600, 0.25f,0.15f, 0.070f, 1.0f, 4 },
        /* Arp    */         { false, 0.0f,  0.001f,0.25f,0.0f,  0.12f, 500,  5200, 0.14f,0.22f, 0.065f, 1.0f, 5 },
        /* Drums  */         { false, 0.0f,  0.0f,  0.0f, 0.0f,  0.0f,  0,    0,    0.0f, 0.0f,  0.55f,  1.0f, 0 },
        /* FX     */         { false, 0.0f,  0.0f,  0.0f, 0.0f,  0.0f,  0,    0,    0.0f, 0.0f,  0.22f,  1.0f, 0 },
    };
    return c[std::clamp (layer, 0, kNumLayers - 1)];
}

void PreviewSynth::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    for (auto& v : voices) v = Voice {};
    monoVoice.fill (-1);
    for (auto& s : monoStack) { s.clear(); s.reserve (32); }
}

PreviewSynth::Voice* PreviewSynth::allocate (int layer)
{
    (void) layer;
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.active) return &v;
    for (auto& v : voices)
        if (! v.gate && (! best || v.age < best->age)) best = &v;
    if (best) return best;
    for (auto& v : voices)
        if (! best || v.age < best->age) best = &v;
    return best;
}

void PreviewSynth::noteOn (int layer, int pitch, int velocity)
{
    if (layer < 0 || layer >= kNumLayers) return;
    const auto& c = cfg (layer);
    const float vel = (float) std::clamp (velocity, 1, 127) / 127.0f;

    if (c.mono)
    {
        auto& stack = monoStack[(size_t) layer];
        stack.erase (std::remove (stack.begin(), stack.end(), pitch), stack.end());
        if (stack.size() < 32) stack.push_back (pitch);
        int vi = monoVoice[(size_t) layer];
        if (vi >= 0)
        {
            Voice& v = voices[(size_t) vi];
            if (v.active && v.layer == layer)
            {
                if (v.gate)
                {
                    // legato: slide to the new note without retriggering
                    v.pitch = pitch;
                    v.target = midiHz (pitch);
                    return;
                }
                // released but still sounding: retrigger in place
                v.pitch = pitch;
                v.freq = v.target = midiHz (pitch);
                v.vel = vel;
                v.gate = true;
                v.stage = 0;
                v.t = 0;
                v.fenv = 1;
                v.age = ++counter;
                return;
            }
        }
    }

    Voice* v = allocate (layer);
    if (! v) return;
    const int index = (int) (v - voices.data());
    *v = Voice {};
    v->active = true;
    v->gate = true;
    v->layer = layer;
    v->pitch = pitch;
    v->vel = vel;
    v->freq = v->target = midiHz (pitch);
    v->age = ++counter;
    v->noise = (uint32_t) (counter * 2654435761u) | 1u;
    v->pan = layer == (int) Layer::Arp ? ((counter % 2) ? 0.35f : -0.35f) : 0.0f;
    for (int k = 0; k < 4; ++k) v->phase[k] = (layer == (int) Layer::Chords) ? 0.25 * k : 0.0;
    if (c.mono) monoVoice[(size_t) layer] = index;
}

void PreviewSynth::noteOff (int layer, int pitch)
{
    if (layer < 0 || layer >= kNumLayers) return;
    const auto& c = cfg (layer);
    if (c.mono)
    {
        auto& stack = monoStack[(size_t) layer];
        stack.erase (std::remove (stack.begin(), stack.end(), pitch), stack.end());
        int vi = monoVoice[(size_t) layer];
        if (vi < 0) return;
        Voice& v = voices[(size_t) vi];
        if (! v.active || v.layer != layer || v.pitch != pitch) return;
        if (! stack.empty())
        {
            v.pitch = stack.back();
            v.target = midiHz (v.pitch);
            return;
        }
        v.gate = false;
        v.stage = 2;
        return;
    }
    for (auto& v : voices)
        if (v.active && v.gate && v.layer == layer && v.pitch == pitch)
        {
            v.gate = false;
            v.stage = 2;
        }
}

void PreviewSynth::allNotesOff (bool hard)
{
    for (auto& v : voices)
    {
        if (hard) v.active = false;
        v.gate = false;
        v.stage = 2;
    }
    for (auto& s : monoStack) s.clear();
}

float PreviewSynth::noise (Voice& v)
{
    v.noise ^= v.noise << 13;
    v.noise ^= v.noise >> 17;
    v.noise ^= v.noise << 5;
    return (float) ((int32_t) v.noise) * (1.0f / 2147483648.0f);
}

void PreviewSynth::svf (float x, float g, float k, float& ic1, float& ic2, float& lp, float& bp)
{
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    lp = v2;
    bp = v1;
}

void PreviewSynth::renderVoice (Voice& v, float* L, float* R, int n)
{
    const auto& c = cfg (v.layer);
    const double invSr = 1.0 / sr;
    const float panL = std::sqrt (0.5f * (1.0f - v.pan)), panR = std::sqrt (0.5f * (1.0f + v.pan));
    const float velAmp = 0.35f + 0.65f * v.vel;

    for (int i = 0; i < n; ++i)
    {
        float out = 0.0f;
        const double t = v.t;

        if (v.layer == (int) Layer::Drums)
        {
            float lp, bp;
            switch (v.pitch)
            {
                case drum::Kick:
                {
                    double f = 46.0 + 120.0 * std::exp (-t * 32.0);
                    advance (v.phase[0], f * invSr);
                    out = (float) std::sin (2 * kPi * v.phase[0]) * (float) std::exp (-t * 5.5)
                        + noise (v) * (float) std::exp (-t * 320.0) * 0.25f;
                    out = std::tanh ((heavy ? 3.2f : 1.6f) * out);
                    if (t > 0.6) v.active = false;
                    break;
                }
                case drum::Snare:
                {
                    advance (v.phase[0], 190.0 * invSr);
                    float nz = noise (v);
                    svf (nz, 0.55f, 1.2f, v.ic1, v.ic2, lp, bp);
                    out = (float) std::sin (2 * kPi * v.phase[0]) * (float) std::exp (-t * 22.0) * 0.55f
                        + (nz - lp) * (float) std::exp (-t * 13.0) * 0.75f;
                    if (t > 0.45) v.active = false;
                    break;
                }
                case drum::Clap:
                {
                    float nz = noise (v);
                    svf (nz, 0.35f, 0.9f, v.ic1, v.ic2, lp, bp);
                    float e = 0;
                    for (double b : { 0.0, 0.011, 0.022 })
                        if (t >= b) e += (float) std::exp (-(t - b) * 180.0);
                    e += (float) std::exp (-t * 11.0) * 0.55f;
                    out = bp * e * 1.6f;
                    if (t > 0.5) v.active = false;
                    break;
                }
                case drum::ClosedHat:
                case drum::OpenHat:
                case drum::Crash:
                {
                    float nz = noise (v);
                    svf (nz, 0.9f, 1.0f, v.ic1, v.ic2, lp, bp);
                    double dec = v.pitch == drum::ClosedHat ? 48.0 : (v.pitch == drum::OpenHat ? 7.5 : 1.7);
                    float g = v.pitch == drum::Crash ? 0.35f : 0.42f;
                    out = (nz - lp) * (float) std::exp (-t * dec) * g;
                    if (t > (v.pitch == drum::Crash ? 2.5 : (v.pitch == drum::OpenHat ? 0.8 : 0.2))) v.active = false;
                    if (! v.gate && v.pitch == drum::OpenHat) v.env *= 0.999f;
                    break;
                }
                default:
                {
                    double f = (v.pitch == drum::Tom ? 110.0 : 900.0) * (1.0 + 0.5 * std::exp (-t * 30.0));
                    advance (v.phase[0], f * invSr);
                    out = (float) std::sin (2 * kPi * v.phase[0]) * (float) std::exp (-t * (v.pitch == drum::Tom ? 9.0 : 60.0)) * 0.5f;
                    if (t > 0.5) v.active = false;
                    break;
                }
            }
            out *= velAmp * c.gain;
        }
        else if (v.layer == (int) Layer::FX)
        {
            float nz = noise (v);
            float lp, bp;
            switch (v.pitch)
            {
                case fx::Impact:
                {
                    advance (v.phase[0], (40.0 + 30.0 * std::exp (-t * 6.0)) * invSr);
                    svf (nz, 0.08f, 1.4f, v.ic1, v.ic2, lp, bp);
                    out = (float) std::sin (2 * kPi * v.phase[0]) * (float) std::exp (-t * 1.3) * 1.2f
                        + lp * (float) std::exp (-t * 2.5) * 0.9f;
                    if (t > 3.0) v.active = false;
                    break;
                }
                case fx::Riser:
                case fx::NoiseSweep:
                case fx::Downlifter:
                {
                    double rise = v.pitch == fx::NoiseSweep ? 1.6 : 3.2;
                    double x = std::min (1.0, t / rise);
                    double f = v.pitch == fx::Downlifter ? 7000.0 * std::pow (0.03, x) : 250.0 * std::pow (32.0, x);
                    float g = (float) std::tan (kPi * std::min (f, sr * 0.45) * invSr);
                    svf (nz, g, 0.5f, v.ic1, v.ic2, lp, bp);
                    float amp = v.pitch == fx::Downlifter ? (float) std::exp (-t * 0.9) : (float) (0.2 + 0.8 * x);
                    out = bp * amp * 0.9f;
                    if (v.pitch == fx::Downlifter && t > 4.0) v.active = false;
                    break;
                }
                case fx::ReverseCrash:
                {
                    svf (nz, 0.9f, 1.0f, v.ic1, v.ic2, lp, bp);
                    double x = std::min (1.0, t / 0.45);
                    out = (nz - lp) * (float) (x * x) * 0.6f;
                    break;
                }
                default:
                    out = nz * (float) std::exp (-t * 40.0) * 0.3f;
                    if (t > 0.3) v.active = false;
                    break;
            }
            // FX follow the note length: fast fade after note-off
            if (! v.gate)
            {
                v.env = v.stage == 2 ? v.env : 1.0f;
                v.stage = 2;
            }
            if (v.stage == 2)
            {
                v.env *= (float) std::exp (-invSr / 0.08);
                out *= v.env;
                if (v.env < 1e-4f) v.active = false;
            }
            out *= velAmp * c.gain;
        }
        else
        {
            // ---- pitched layers ------------------------------------------------------------------
            if (c.mono && c.glide > 0 && v.freq != v.target)
            {
                const double k = 1.0 - std::exp (-invSr / c.glide);
                v.freq += (v.target - v.freq) * k;
                if (std::fabs (v.freq - v.target) < 0.01) v.freq = v.target;
            }
            const double dt = v.freq * invSr;
            float osc = 0.0f;
            switch (c.wave)
            {
                case 0:
                    advance (v.phase[0], dt);
                    osc = (float) std::sin (2 * kPi * v.phase[0]);
                    break;
                case 1: // saw + square an octave down
                {
                    advance (v.phase[0], dt);
                    advance (v.phase[1], dt * 0.5);
                    double ph2 = v.phase[1] + 0.5; if (ph2 >= 1) ph2 -= 1;
                    float sq = sawAt (v.phase[1], dt * 0.5) - sawAt (ph2, dt * 0.5);
                    osc = 0.65f * sawAt (v.phase[0], dt) + 0.45f * sq;
                    break;
                }
                case 2: // pulse-width square
                {
                    advance (v.phase[0], dt);
                    double pw = 0.5 + 0.35 * std::sin (2 * kPi * t * 3.0);
                    double ph2 = v.phase[0] + pw; if (ph2 >= 1) ph2 -= 1;
                    osc = sawAt (v.phase[0], dt) - sawAt (ph2, dt);
                    break;
                }
                case 3: // supersaw: 3 detuned saws, spread
                {
                    static const double det[3] = { 0.9942, 1.0, 1.0059 };
                    float s3[3];
                    for (int k = 0; k < 3; ++k)
                    {
                        advance (v.phase[k], dt * det[k]);
                        s3[k] = sawAt (v.phase[k], dt * det[k]);
                    }
                    osc = s3[1];
                    v.lastOut = 0.8f * (s3[0] - s3[2]);
                    break;
                }
                case 4: // lead: saw + square
                {
                    advance (v.phase[0], dt);
                    double ph2 = v.phase[0] + 0.5; if (ph2 >= 1) ph2 -= 1;
                    osc = 0.6f * sawAt (v.phase[0], dt) + 0.4f * (sawAt (v.phase[0], dt) - sawAt (ph2, dt));
                    break;
                }
                default: // pluck saw
                    advance (v.phase[0], dt);
                    osc = sawAt (v.phase[0], dt);
                    break;
            }

            // amp envelope
            switch (v.stage)
            {
                case 0:
                    v.env += (float) (invSr / std::max (0.0005f, c.a));
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
                    break;
                case 1:
                {
                    float k = (float) (1.0 - std::exp (-invSr / std::max (0.01f, c.d)));
                    v.env += (c.s - v.env) * k;
                    break;
                }
                default:
                {
                    float k = (float) std::exp (-invSr / std::max (0.005f, c.r));
                    v.env *= k;
                    if (v.env < 1e-4f) v.active = false;
                    break;
                }
            }
            v.fenv *= (float) std::exp (-invSr / std::max (0.01f, c.fdecay));

            float sig = osc;
            float side = v.lastOut;
            if (c.cutoff > 0)
            {
                double fc = std::min (sr * 0.45, (double) c.cutoff + c.envAmt * v.fenv + v.freq * 1.5);
                float g = (float) std::tan (kPi * fc * invSr);
                float k = 2.0f - 1.9f * c.res;
                float lp, bp;
                svf (sig, g, k, v.ic1, v.ic2, lp, bp);
                sig = lp;
                if (c.wave == 3)
                {
                    float lp2, bp2;
                    svf (side, g, k, v.jc1, v.jc2, lp2, bp2);
                    side = lp2;
                }
            }
            {
                float drive = c.drive;
                if (heavy && (v.layer == (int) Layer::BassA || v.layer == (int) Layer::BassB)) drive *= 2.6f;
                if (heavy && (v.layer == (int) Layer::Chords || v.layer == (int) Layer::Lead)) drive = 3.0f;
                if (drive > 1.0f) sig = std::tanh (sig * drive) / std::tanh (drive);
            }
            float amp = v.env * velAmp * c.gain;
            if (c.wave == 3)
            {
                L[i] += (sig + side) * amp * 0.7f;
                R[i] += (sig - side) * amp * 0.7f;
                v.t += invSr;
                if (! v.active) break;
                continue;
            }
            out = sig * amp;
        }

        L[i] += out * panL * 1.41421356f;
        R[i] += out * panR * 1.41421356f;
        v.t += invSr;
        if (! v.active) break;
    }
}

void PreviewSynth::render (float* L, float* R, int n)
{
    for (auto& v : voices)
        if (v.active)
            renderVoice (v, L, R, n);
    for (int i = 0; i < n; ++i)
    {
        const float m = heavy ? 1.5f : 0.9f;
        L[i] = std::tanh (L[i] * m) * 0.9f;
        R[i] = std::tanh (R[i] * m) * 0.9f;
    }
}

// ---- song player ----------------------------------------------------------------------------------
std::shared_ptr<const PlayData> makePlayData (const Song& song)
{
    auto d = std::make_shared<PlayData>();
    d->totalTicks = std::max (1, song.totalTicks);
    for (int l = 0; l < kNumLayers; ++l)
        for (auto& n : song.layers[(size_t) l])
        {
            d->events.push_back ({ n.start, (uint8_t) l, (uint8_t) n.pitch, (uint8_t) n.velocity, true });
            d->events.push_back ({ std::min (n.end(), d->totalTicks), (uint8_t) l, (uint8_t) n.pitch, 0, false });
        }
    std::stable_sort (d->events.begin(), d->events.end(), [] (const PlayEvent& a, const PlayEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return (int) a.on < (int) b.on; // offs first
    });
    return d;
}

void SongPlayer::releaseAll (PreviewSynth& synth, std::vector<MidiOutEvent>* midiOut, uint32_t midiMask, int offset)
{
    for (int l = 0; l < kNumLayers; ++l)
        for (int p = 0; p < 128; ++p)
            if (held[(size_t) l][(size_t) p])
            {
                synth.noteOff (l, p);
                if (midiOut && ((midiMask >> l) & 1u)) midiOut->push_back ({ offset, l, p, 0, false });
                held[(size_t) l][(size_t) p] = false;
            }
}

void SongPlayer::process (PreviewSynth& synth, float* L, float* R, int n, double startTick, double tps, bool playing,
                          uint32_t layerMask, std::vector<MidiOutEvent>* midiOut, uint32_t midiMask)
{
    process (synth, data.get(), L, R, n, startTick, tps, playing, layerMask, midiOut, midiMask);
}

void SongPlayer::process (PreviewSynth& synth, const PlayData* d, float* L, float* R, int n, double startTick, double tps,
                          bool playing, uint32_t layerMask, std::vector<MidiOutEvent>* midiOut, uint32_t midiMask)
{
    std::fill (L, L + n, 0.0f);
    std::fill (R, R + n, 0.0f);
    if (! playing || ! d || d->totalTicks <= 0 || tps <= 0)
    {
        if (wasPlaying) releaseAll (synth, midiOut, midiMask, 0);
        wasPlaying = false;
        lastData = d;
        expected = -1;
        synth.render (L, R, n);
        return;
    }

    const double total = d->totalTicks;
    double pos = std::fmod (startTick, total);
    if (pos < 0) pos += total;
    const bool jumped = ! wasPlaying || d != lastData || expected < 0 || std::fabs (pos - expected) > 1.5;
    if (jumped && wasPlaying) releaseAll (synth, midiOut, midiMask, 0);
    wasPlaying = true;
    lastData = d;
    lastPos = pos;

    if (scratch.capacity() < 4096) scratch.reserve (4096);
    scratch.clear();
    const auto& ev = d->events;

    // collect events in [a, b) with their sample offsets
    auto collect = [&] (double a, double b, double sampleBase, bool includeEnd) {
        auto it = std::lower_bound (ev.begin(), ev.end(), (int) std::ceil (a),
                                    [] (const PlayEvent& e, int t) { return e.tick < t; });
        // when the block starts exactly on a note (jump/start), make sure it is played
        for (; it != ev.end(); ++it)
        {
            const double t = it->tick;
            if (t > b || (t == b && ! includeEnd)) break;
            if (t < a) continue;
            int off = (int) std::floor ((t - a) / tps + sampleBase);
            off = std::clamp (off, 0, n - 1);
            scratch.push_back ({ off, it->layer, it->pitch, it->velocity, it->on });
        }
    };
    const double endTick = pos + n * tps;
    if (endTick <= total)
        collect (pos, endTick, 0.0, false);
    else
    {
        collect (pos, total, 0.0, true);
        const double samplesToWrap = (total - pos) / tps;
        collect (0.0, endTick - total, samplesToWrap, false);
    }
    expected = std::fmod (endTick, total);

    int done = 0;
    for (auto& e : scratch)
    {
        if (e.sampleOffset > done)
        {
            synth.render (L + done, R + done, e.sampleOffset - done);
            done = e.sampleOffset;
        }
        auto& h = held[(size_t) e.layer];
        if (e.on)
        {
            if (! ((layerMask >> e.layer) & 1u)) continue;
            if (h[(size_t) e.pitch])
            {
                synth.noteOff (e.layer, e.pitch);
                if (midiOut && ((midiMask >> e.layer) & 1u)) midiOut->push_back ({ e.sampleOffset, e.layer, e.pitch, 0, false });
            }
            synth.noteOn (e.layer, e.pitch, e.velocity);
            h[(size_t) e.pitch] = true;
            if (midiOut && ((midiMask >> e.layer) & 1u)) midiOut->push_back (e);
        }
        else
        {
            if (! h[(size_t) e.pitch]) continue;
            synth.noteOff (e.layer, e.pitch);
            h[(size_t) e.pitch] = false;
            if (midiOut && ((midiMask >> e.layer) & 1u)) midiOut->push_back (e);
        }
    }
    if (done < n) synth.render (L + done, R + done, n - done);
}
} // namespace te
