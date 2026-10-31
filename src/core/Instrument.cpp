#include "Instrument.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <string>

namespace te
{
#include "SimplerTemplate.inc"

InstrumentSpec instrumentDefaults (Layer layer, bool userSample)
{
    // voices/glide/release follow PreviewSynth::cfg: mono layers glide only between legato notes (exponential,
    // tau = cfg glide); Simpler's glide time is the full slide, so it is about 2x tau.
    InstrumentSpec s;
    switch (layer)
    {
        case Layer::Sub:   s.voicesIndex = 0; s.glide = true; s.glideMs = 100; s.releaseMs = 40; break;
        case Layer::BassA: s.voicesIndex = 0; s.glide = true; s.glideMs = 90;  s.releaseMs = 50; break;
        case Layer::BassB: s.voicesIndex = 0; s.glide = true; s.glideMs = 80;  s.releaseMs = 50; break;
        case Layer::Lead:  s.voicesIndex = 0; s.glide = true; s.glideMs = 60;  s.releaseMs = 120; break;
        case Layer::Chords:s.voicesIndex = 7; s.releaseMs = 280; break;
        case Layer::Arp:   s.voicesIndex = 5; s.releaseMs = 120; break;
        default: break;
    }
    if (userSample)
    {
        // the preview plays loaded sounds with a 2 ms fade-in and a ~40 ms (time constant) release
        s.attackMs = 2.0;
        s.releaseMs = 110.0;
    }
    else
        s.attackMs = 0.5; // the rendered voice already holds the synth's attack
    return s;
}

InstrumentAudio instrumentAudio (const SampleData& d, bool applySampleGain)
{
    InstrumentAudio a;
    const float g = applySampleGain ? d.gain : 1.0f;
    a.left = d.left;
    a.right = d.right.size() == d.left.size() ? d.right : d.left;
    float peak = 0.0f;
    for (size_t i = 0; i < a.left.size(); ++i)
        peak = std::max ({ peak, std::fabs (a.left[i] * g), std::fabs (a.right[i] * g) });
    if (peak < 1e-6f) return a;
    const double target = 0.891; // -1 dBFS
    const double scale = target / peak;
    for (size_t i = 0; i < a.left.size(); ++i)
    {
        a.left[i] = (float) (a.left[i] * g * scale);
        a.right[i] = (float) (a.right[i] * g * scale);
    }
    a.volumeDb = std::clamp (-6.0 - 20.0 * std::log10 (scale), -36.0, 36.0);
    return a;
}

int instrumentRoot (Layer layer)
{
    switch (layer)
    {
        case Layer::Sub:    return 36;
        case Layer::BassA:
        case Layer::BassB:  return 48;
        case Layer::Chords: return 60;
        default:            return 72;
    }
}

static std::string xmlEscape (const std::string& in)
{
    std::string out;
    for (char c : in)
    {
        switch (c)
        {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

static std::string num (double v)
{
    char buf[64];
    std::snprintf (buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string makeSimplerPresetXml (const InstrumentSpec& s)
{
    std::string xml ((const char*) kSimplerTemplate, kSimplerTemplateSize);
    const int last = std::max (0, s.frames - 1);
    const bool looped = s.loopEnd > s.loopStart && s.loopStart >= 0;
    const std::pair<const char*, std::string> values[] = {
        { "@@USERNAME@@", xmlEscape (s.userName) },
        { "@@NAME@@", xmlEscape (s.sampleName) },
        { "@@ROOT@@", std::to_string (std::clamp (s.rootNote, 0, 127)) },
        { "@@END@@", std::to_string (last) },
        { "@@LSTART@@", std::to_string (looped ? s.loopStart : 0) },
        { "@@LEND@@", std::to_string (looped ? std::min (s.loopEnd, last) : last) },
        { "@@LMODE@@", looped ? "1" : "0" },
        { "@@PATH@@", xmlEscape (s.wavPath) },
        { "@@SIZE@@", std::to_string (s.fileSize) },
        { "@@FRAMES@@", std::to_string (s.frames) },
        { "@@RATE@@", std::to_string ((int) std::lround (s.sampleRate)) },
        { "@@VOICES@@", std::to_string (s.voicesIndex) },
        { "@@RETRIGGER@@", s.glide ? "false" : "true" },
        { "@@PMODE@@", s.glide ? "1" : "0" },
        { "@@PTIME@@", num (s.glideMs) },
        { "@@ATTACK@@", num (s.attackMs) },
        { "@@RELEASE@@", num (s.releaseMs) },
        { "@@VOLUME@@", num (s.volumeDb) },
    };
    for (auto& kv : values)
    {
        const std::string key = kv.first;
        for (size_t p = xml.find (key); p != std::string::npos; p = xml.find (key, p + kv.second.size()))
            xml.replace (p, key.size(), kv.second);
    }
    return xml;
}

std::shared_ptr<SampleData> renderLayerVoice (Layer layer, double sr)
{
    PreviewSynth synth;
    synth.prepare (sr);
    const int root = instrumentRoot (layer);
    const int held = (int) (sr * 2.6), tail = (int) (sr * 0.6);
    std::vector<float> L ((size_t) (held + tail)), R ((size_t) (held + tail));
    synth.noteOn ((int) layer, root, 112);
    const int block = 256;
    for (int pos = 0; pos < held + tail; pos += block)
    {
        if (pos >= held && pos - block < held) synth.noteOff ((int) layer, root);
        const int n = std::min (block, held + tail - pos);
        synth.render (L.data() + pos, R.data() + pos, n);
    }
    // trim the silent end
    size_t end = L.size();
    while (end > 1 && std::fabs (L[end - 1]) < 1e-4f && std::fabs (R[end - 1]) < 1e-4f) --end;
    L.resize (end);
    R.resize (end);
    auto data = makeSampleData (std::move (L), std::move (R), sr, root);
    if (layer == Layer::Arp && data) data->loopStart = data->loopEnd = -1; // a pluck: one-shot, no sustain loop
    return data;
}
} // namespace te
