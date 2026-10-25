// TEAROUT ENGINE command-line tool: dump, analyse, write example MIDI / WAV
#include "../src/core/Engine.h"
#include "../src/core/MidiFile.h"
#include "../src/core/Synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <fstream>
#include <iostream>
#include <string>

using namespace te;

static std::vector<uint8_t> readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    return std::vector<uint8_t> ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
}

static void writeFile (const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream f (path, std::ios::binary);
    f.write ((const char*) data.data(), (std::streamsize) data.size());
}

static void dumpLayer (const Song& song, Layer l)
{
    const auto& notes = song.layers[(size_t) l];
    std::printf ("  %-7s (%s) %zu notes\n", layerName (l), patternsFor (l)[(size_t) song.pattern[(size_t) l]].name, notes.size());
    if (notes.empty()) return;
    int shownBars = std::min (song.bars, 8);
    for (int bar = 0; bar < shownBars; ++bar)
    {
        std::string line;
        for (auto& n : notes)
            if (n.start / BAR == bar)
            {
                char buf[64];
                std::snprintf (buf, sizeof buf, "%s@%g/%g ", pitchName (n.pitch, song.flats).c_str(),
                               (n.start % BAR) / (double) STEP, n.length / (double) STEP);
                line += buf;
            }
        std::printf ("    bar %d: %s\n", bar + 1, line.c_str());
    }
}

static Settings parseSettings (int argc, char** argv, int first)
{
    Settings s;
    for (int i = first; i + 1 < argc; i += 2)
    {
        std::string k = argv[i], v = argv[i + 1];
        int x = std::atoi (v.c_str());
        if (k == "genre") s.genre = (Genre) std::clamp (x, 0, kNumGenres - 1);
        else if (k == "key") s.keyRoot = x;
        else if (k == "prog") s.progression = x;
        else if (k == "bars") s.bars = x;
        else if (k == "chordlen") s.chordLength = x;
        else if (k == "seed") s.songSeed = (uint32_t) x;
        else if (k == "groove") s.grooveSeed = (uint32_t) x;
        else if (k == "mv") for (auto& l : s.layers) l.movement = (float) std::atof (v.c_str());
        else if (k.rfind ("oct", 0) == 0) s.layers[(size_t) std::atoi (k.c_str() + 3)].octave = x;
        else if (k.rfind ("pat", 0) == 0) s.layers[(size_t) std::atoi (k.c_str() + 3)].pattern = x;
        else if (k.rfind ("mv", 0) == 0) s.layers[(size_t) std::atoi (k.c_str() + 2)].movement = (float) std::atof (v.c_str());
    }
    return s;
}

static std::vector<MidiTrackOut> songTracks (const Song& song, const std::string& prefix)
{
    std::vector<MidiTrackOut> tracks;
    for (int l = 0; l < kNumLayers; ++l)
        if (! song.layers[(size_t) l].empty())
            tracks.push_back ({ prefix + " " + layerName ((Layer) l), song.layers[(size_t) l], 0 });
    return tracks;
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::vector<uint8_t> d;
    auto p32 = [&] (uint32_t x) { for (int i = 0; i < 4; ++i) d.push_back ((uint8_t) (x >> (8 * i))); };
    auto p16 = [&] (uint16_t x) { d.push_back ((uint8_t) x); d.push_back ((uint8_t) (x >> 8)); };
    const uint32_t n = (uint32_t) L.size();
    d.insert (d.end(), { 'R', 'I', 'F', 'F' });
    p32 (36 + n * 4);
    d.insert (d.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    p32 (16); p16 (1); p16 (2); p32 ((uint32_t) sr); p32 ((uint32_t) sr * 4); p16 (4); p16 (16);
    d.insert (d.end(), { 'd', 'a', 't', 'a' });
    p32 (n * 4);
    for (uint32_t i = 0; i < n; ++i)
    {
        p16 ((uint16_t) (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f));
        p16 ((uint16_t) (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f));
    }
    writeFile (path, d);
}

static bool g_heavy = false;
struct LayerSample { std::shared_ptr<SampleData> data; float gain = 1.0f; };
static std::map<int, std::shared_ptr<SampleData>> g_samples;
static std::map<int, float> g_gains;

// PCM 16/24/32 or float WAV -> SampleData, with an automatic crossfaded sustain loop.
static std::shared_ptr<SampleData> loadWav (const std::string& path, int root)
{
    auto d = readFile (path);
    if (d.size() < 44 || std::string (d.begin(), d.begin() + 4) != "RIFF") return nullptr;
    size_t pos = 12;
    int channels = 0, bits = 0, format = 0;
    uint32_t rate = 0;
    auto u16 = [&] (size_t p) { return (uint32_t) d[p] | ((uint32_t) d[p + 1] << 8); };
    auto u32 = [&] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };
    auto out = std::make_shared<SampleData>();
    while (pos + 8 <= d.size())
    {
        std::string id (d.begin() + (long) pos, d.begin() + (long) pos + 4);
        uint32_t len = u32 (pos + 4);
        size_t body = pos + 8;
        if (id == "fmt ")
        {
            format = (int) u16 (body);
            channels = (int) u16 (body + 2);
            rate = u32 (body + 4);
            bits = (int) u16 (body + 14);
            if (format == 0xFFFE && len >= 26) format = (int) u16 (body + 24);
        }
        else if (id == "data" && channels > 0)
        {
            const int bps = bits / 8;
            const size_t frames = std::min<size_t> (len, d.size() - body) / (size_t) (bps * channels);
            for (size_t f = 0; f < frames; ++f)
            {
                float ch[2] = { 0, 0 };
                for (int c = 0; c < channels && c < 2; ++c)
                {
                    size_t p = body + (f * (size_t) channels + (size_t) c) * (size_t) bps;
                    float v = 0;
                    if (format == 3 && bits == 32) { uint32_t u = u32 (p); std::memcpy (&v, &u, 4); }
                    else if (bits == 16) v = (int16_t) u16 (p) / 32768.0f;
                    else if (bits == 24) { int32_t x = (int32_t) ((u32 (p) & 0xFFFFFF) << 8) >> 8; v = x / 8388608.0f; }
                    else if (bits == 32) v = (int32_t) u32 (p) / 2147483648.0f;
                    ch[c] = v;
                }
                out->left.push_back (ch[0]);
                out->right.push_back (channels > 1 ? ch[1] : ch[0]);
            }
        }
        pos = body + len + (len & 1);
    }
    if (out->left.size() < 64) return nullptr;
    out->sampleRate = rate;
    out->rootNote = root;
    // sustain loop over the steady part of the sound (RMS >= 70 % of max), crossfaded
    const size_t n = out->left.size(), win = (size_t) (rate * 0.05);
    std::vector<float> env;
    for (size_t i = 0; i + win <= n; i += win)
    {
        double e = 0;
        for (size_t k = i; k < i + win; ++k) e += out->left[k] * out->left[k];
        env.push_back ((float) std::sqrt (e / (double) win));
    }
    float peak = 0;
    for (float e : env) peak = std::max (peak, e);
    size_t a = env.size(), b = 0;
    for (size_t i = 0; i < env.size(); ++i)
        if (env[i] >= 0.7f * peak) { a = std::min (a, i); b = i; }
    if (b > a + 6)
    {
        size_t ls = (a + (b - a) / 4) * win, le = b * win;
        const size_t xf = std::min<size_t> ((size_t) (rate * 0.04), ls);
        for (size_t i = 0; i < xf; ++i)
        {
            float t = (float) i / (float) xf;
            out->left[le - xf + i] = out->left[le - xf + i] * (1 - t) + out->left[ls - xf + i] * t;
            out->right[le - xf + i] = out->right[le - xf + i] * (1 - t) + out->right[ls - xf + i] * t;
        }
        out->loopStart = (int) ls;
        out->loopEnd = (int) le;
    }
    std::printf ("sample %s: %zu frames @ %u Hz, root %d, loop %d..%d\n", path.c_str(), n, rate, root, out->loopStart, out->loopEnd);
    return out;
}

static void renderSong (const Song& song, const std::string& path, double bpm, int loops, uint32_t layerMask = 0xFFFFFFFFu)
{
    const int sr = 44100;
    SongPlayer player;
    PreviewSynth synth;
    synth.prepare (sr);
    synth.setHeavy (g_heavy);
    for (auto& kv : g_samples) synth.setLayerSample (kv.first, kv.second);
    for (auto& kv : g_gains) synth.setLayerGain (kv.first, kv.second);
    player.setSong (std::make_shared<const Song> (song));
    const double ticksPerSample = bpm / 60.0 * PPQ / sr;
    const int total = (int) std::ceil (song.totalTicks * loops / ticksPerSample) + sr;
    std::vector<float> L ((size_t) total), R ((size_t) total);
    const int block = 256;
    double tick = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        int n = std::min (block, total - pos);
        bool playing = tick < song.totalTicks * loops;
        player.process (synth, L.data() + pos, R.data() + pos, n, tick, ticksPerSample, playing, layerMask, nullptr);
        tick += n * ticksPerSample;
    }
    writeWav (path, L, R, sr);
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: te_cli dump [genre N key N prog N bars N seed N groove N mv X patL N mvL X]\n"
                     "       te_cli analyse file.mid\n"
                     "       te_cli examples outdir [wav]\n"
                     "       te_cli render outdir/name.wav [settings...]\n");
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "dump")
    {
        Settings s = parseSettings (argc, argv, 2);
        Song song = generate (s);
        std::printf ("%s | %s %s | %s | %d bars, loop %d\n", genreProfile (s.genre).name, keyName (song.keyRoot, song.mode).c_str(),
                     song.progressionLabel.c_str(), song.chordNames.c_str(), song.bars, song.loopBars);
        for (auto& h : song.hints) std::printf ("  hint: %s\n", h.c_str());
        for (int l = 0; l < kNumLayers; ++l) dumpLayer (song, (Layer) l);
        return 0;
    }
    if (cmd == "analyse" && argc >= 3)
    {
        std::vector<Note> notes;
        double bpm;
        std::string err;
        if (! readMidi (readFile (argv[2]), notes, bpm, err)) { std::printf ("error: %s\n", err.c_str()); return 1; }
        auto a = analyseSeed (notes);
        std::printf ("%s\n", a.summary.c_str());
        return 0;
    }
    if (cmd == "examples" && argc >= 3)
    {
        std::string dir = argv[2];
        bool wav = argc >= 4 && std::string (argv[3]) == "wav";
        for (int g = 0; g < kNumGenres; ++g)
        {
            Settings s;
            s.genre = (Genre) g;
            s.keyRoot = 5; // F
            s.bars = 8;
            s.songSeed = 7;
            s.grooveSeed = 3;
            // show some range: hardstyle on the trance loop, common on the pop axis (I-V-vi-IV)
            if ((Genre) g == Genre::Hardstyle) s.progression = kProgressionFirst + 2;
            if ((Genre) g == Genre::Common) s.progression = kProgressionFirst + 20;
            Song song = generate (s);
            std::string name = std::string (genreProfile ((Genre) g).shortName);
            std::string file = dir + "/" + std::to_string (g + 1) + "_" + name + "_" + keyName (song.keyRoot, song.mode) + ".mid";
            for (auto& ch : file) if (ch == ' ') ch = '_';
            writeFile (file, writeMidi (songTracks (song, name), song.bpm));
            if (wav)
            {
                std::string wf = file.substr (0, file.size() - 4) + ".wav";
                renderSong (song, wf, song.bpm, 1);
            }
            std::printf ("%s  (%s)\n", file.c_str(), song.chordNames.c_str());
        }
        return 0;
    }
    if (cmd == "make" && argc >= 3)
    {
        // te_cli make out/base [settings...] -> base.mid (all layers) + base.wav, loops twice
        Settings s = parseSettings (argc, argv, 3);
        std::map<int, std::string> samplePaths;
        std::map<int, int> roots;
        for (int i = 3; i + 1 < argc; i += 2)
        {
            std::string k = argv[i];
            if (k == "heavy") g_heavy = std::atoi (argv[i + 1]) != 0;
            else if (k.rfind ("sample", 0) == 0) samplePaths[std::atoi (k.c_str() + 6)] = argv[i + 1];
            else if (k.rfind ("root", 0) == 0) roots[std::atoi (k.c_str() + 4)] = std::atoi (argv[i + 1]);
            else if (k.rfind ("gain", 0) == 0) g_gains[std::atoi (k.c_str() + 4)] = (float) std::atof (argv[i + 1]);
        }
        for (auto& kv : samplePaths)
            if (auto smp = loadWav (kv.second, roots.count (kv.first) ? roots[kv.first] : 60)) g_samples[kv.first] = smp;
        Song song = generate (s);
        std::string base = argv[2];
        writeFile (base + ".mid", writeMidi (songTracks (song, "TE"), song.bpm));
        renderSong (song, base + ".wav", song.bpm, 2);
        std::printf ("%s | %s %s | %s | %d bars\n", genreProfile (s.genre).name, keyName (song.keyRoot, song.mode).c_str(),
                     song.progressionLabel.c_str(), song.chordNames.c_str(), song.bars);
        for (int l = 0; l < kNumLayers; ++l)
            std::printf ("  %-7s %s (%zu notes)\n", layerName ((Layer) l), patternsFor ((Layer) l)[(size_t) song.pattern[(size_t) l]].name,
                         song.layers[(size_t) l].size());
        return 0;
    }
    if (cmd == "render" && argc >= 3)
    {
        Settings s = parseSettings (argc, argv, 3);
        Song song = generate (s);
        renderSong (song, argv[2], song.bpm, 1);
        return 0;
    }
    std::printf ("unknown command\n");
    return 1;
}
