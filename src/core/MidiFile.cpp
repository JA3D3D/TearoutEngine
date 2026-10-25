#include "MidiFile.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace te
{
namespace
{
void put32 (std::vector<uint8_t>& v, uint32_t x)
{
    v.push_back ((uint8_t) (x >> 24)); v.push_back ((uint8_t) (x >> 16));
    v.push_back ((uint8_t) (x >> 8));  v.push_back ((uint8_t) x);
}
void put16 (std::vector<uint8_t>& v, uint16_t x)
{
    v.push_back ((uint8_t) (x >> 8)); v.push_back ((uint8_t) x);
}
void putVar (std::vector<uint8_t>& v, uint32_t x)
{
    uint8_t buf[5];
    int n = 0;
    buf[n++] = (uint8_t) (x & 0x7F);
    while ((x >>= 7) != 0) buf[n++] = (uint8_t) ((x & 0x7F) | 0x80);
    while (n > 0) v.push_back (buf[--n]);
}
void putChunk (std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& body)
{
    out.insert (out.end(), id, id + 4);
    put32 (out, (uint32_t) body.size());
    out.insert (out.end(), body.begin(), body.end());
}
void metaText (std::vector<uint8_t>& t, uint8_t type, const std::string& s)
{
    putVar (t, 0);
    t.push_back (0xFF); t.push_back (type);
    putVar (t, (uint32_t) s.size());
    t.insert (t.end(), s.begin(), s.end());
}
} // namespace

std::vector<uint8_t> writeMidi (const std::vector<MidiTrackOut>& tracks, double bpm)
{
    std::vector<uint8_t> out;
    std::vector<uint8_t> hdr;
    put16 (hdr, 1);
    put16 (hdr, (uint16_t) (tracks.size() + 1));
    put16 (hdr, (uint16_t) PPQ);
    putChunk (out, "MThd", hdr);

    // conductor track
    {
        std::vector<uint8_t> t;
        metaText (t, 0x03, "TEAROUT ENGINE");
        const uint32_t usPerQ = (uint32_t) std::lround (60000000.0 / std::max (20.0, bpm));
        putVar (t, 0);
        t.insert (t.end(), { 0xFF, 0x51, 0x03, (uint8_t) (usPerQ >> 16), (uint8_t) (usPerQ >> 8), (uint8_t) usPerQ });
        putVar (t, 0);
        t.insert (t.end(), { 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08 });
        putVar (t, 0);
        t.insert (t.end(), { 0xFF, 0x2F, 0x00 });
        putChunk (out, "MTrk", t);
    }

    for (auto& tr : tracks)
    {
        struct Ev { int tick; int order; uint8_t a, b, c; };
        std::vector<Ev> evs;
        const uint8_t ch = (uint8_t) (tr.channel & 0x0F);
        for (auto& n : tr.notes)
        {
            if (n.length <= 0) continue;
            const uint8_t p = (uint8_t) std::clamp (n.pitch, 0, 127);
            const uint8_t v = (uint8_t) std::clamp (n.velocity, 1, 127);
            evs.push_back ({ n.start, 1, (uint8_t) (0x90 | ch), p, v });
            evs.push_back ({ n.end(), 0, (uint8_t) (0x80 | ch), p, 0x40 });
        }
        // note-offs before note-ons at the same tick, so touching notes never cancel each other
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& x, const Ev& y) {
            return x.tick != y.tick ? x.tick < y.tick : x.order < y.order; });
        std::vector<uint8_t> t;
        metaText (t, 0x03, tr.name);
        int last = 0;
        for (auto& e : evs)
        {
            putVar (t, (uint32_t) (e.tick - last));
            last = e.tick;
            t.push_back (e.a); t.push_back (e.b); t.push_back (e.c);
        }
        putVar (t, 0);
        t.insert (t.end(), { 0xFF, 0x2F, 0x00 });
        putChunk (out, "MTrk", t);
    }
    return out;
}

bool readMidi (const std::vector<uint8_t>& d, std::vector<Note>& notes, double& bpm, std::string& error, bool skipDrums)
{
    notes.clear();
    bpm = 0;
    size_t pos = 0;
    auto need = [&] (size_t n) { return pos + n <= d.size(); };
    auto rd32 = [&] () { uint32_t x = ((uint32_t) d[pos] << 24) | ((uint32_t) d[pos + 1] << 16) | ((uint32_t) d[pos + 2] << 8) | d[pos + 3]; pos += 4; return x; };
    auto rd16 = [&] () { uint16_t x = (uint16_t) ((d[pos] << 8) | d[pos + 1]); pos += 2; return x; };

    if (! need (14) || std::string (d.begin(), d.begin() + 4) != "MThd") { error = "Not a MIDI file."; return false; }
    pos = 4;
    const uint32_t hlen = rd32();
    if (hlen < 6 || ! need (hlen)) { error = "Broken MIDI header."; return false; }
    rd16(); // format
    const int ntracks = rd16();
    const uint16_t division = rd16();
    pos = 8 + hlen;
    if (division & 0x8000) { error = "SMPTE-timed MIDI files aren't supported."; return false; }
    if (division == 0) { error = "Broken MIDI header."; return false; }

    for (int tr = 0; tr < ntracks && need (8); ++tr)
    {
        const std::string id (d.begin() + (long) pos, d.begin() + (long) pos + 4);
        pos += 4;
        const uint32_t len = rd32();
        if (! need (len)) { error = "Truncated MIDI track."; return false; }
        const size_t end = pos + len;
        if (id != "MTrk") { pos = end; continue; }
        uint64_t tick = 0;
        uint8_t status = 0;
        std::map<int, std::vector<std::pair<uint64_t, int>>> open; // (ch<<8|pitch) -> stack of (start, vel)
        auto readVar = [&] () -> uint32_t {
            uint32_t v = 0;
            for (int k = 0; k < 4 && pos < end; ++k)
            {
                uint8_t b = d[pos++];
                v = (v << 7) | (b & 0x7F);
                if (! (b & 0x80)) break;
            }
            return v;
        };
        while (pos < end)
        {
            tick += readVar();
            if (pos >= end) break;
            uint8_t b = d[pos];
            if (b & 0x80) { status = b; ++pos; }
            if (status == 0xFF)
            {
                if (pos >= end) break;
                uint8_t type = d[pos++];
                uint32_t l = readVar();
                if (type == 0x51 && l == 3 && pos + 3 <= end && bpm == 0)
                {
                    uint32_t us = ((uint32_t) d[pos] << 16) | ((uint32_t) d[pos + 1] << 8) | d[pos + 2];
                    if (us > 0) bpm = 60000000.0 / us;
                }
                pos += l;
                status = 0;
                continue;
            }
            if (status == 0xF0 || status == 0xF7)
            {
                uint32_t l = readVar();
                pos += l;
                status = 0;
                continue;
            }
            const uint8_t hi = status & 0xF0;
            const int ch = status & 0x0F;
            if (hi == 0x80 || hi == 0x90)
            {
                if (pos + 2 > end) break;
                int pitch = d[pos] & 0x7F, vel = d[pos + 1] & 0x7F;
                pos += 2;
                int key = (ch << 8) | pitch;
                if (hi == 0x90 && vel > 0)
                    open[key].push_back ({ tick, vel });
                else
                {
                    auto& st = open[key];
                    if (! st.empty())
                    {
                        auto s = st.front();
                        st.erase (st.begin());
                        if (! (skipDrums && ch == 9))
                        {
                            Note n;
                            n.pitch = pitch;
                            n.start = (int) std::llround ((double) s.first * PPQ / division);
                            n.length = std::max (1, (int) std::llround ((double) (tick - s.first) * PPQ / division));
                            n.velocity = s.second;
                            notes.push_back (n);
                        }
                    }
                }
            }
            else if (hi == 0xA0 || hi == 0xB0 || hi == 0xE0) pos += 2;
            else if (hi == 0xC0 || hi == 0xD0) pos += 1;
            else { ++pos; } // unknown: skip a byte
        }
        pos = end;
    }
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    if (bpm <= 0) bpm = 120.0;
    if (notes.empty()) { error = "No notes found in this MIDI file."; return false; }
    return true;
}
} // namespace te
