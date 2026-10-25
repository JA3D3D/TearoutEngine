// Deterministic random numbers: same seed + same question = same answer, on every platform.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace te
{
inline uint64_t mix64 (uint64_t z) noexcept
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Hash any number of integers into one 64-bit value (order matters).
inline uint64_t hashOf (std::initializer_list<uint64_t> parts) noexcept
{
    uint64_t h = 0x243F6A8885A308D3ull;
    for (auto p : parts)
        h = mix64 (h ^ mix64 (p));
    return h;
}

class Rng
{
public:
    explicit Rng (uint64_t seed) noexcept : state (mix64 (seed ^ 0xD1B54A32D192ED03ull)) {}
    Rng (std::initializer_list<uint64_t> parts) noexcept : state (hashOf (parts)) {}

    uint64_t next() noexcept
    {
        state += 0x9E3779B97F4A7C15ull;
        return mix64 (state);
    }
    // [0, 1)
    double unit() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    // [0, n)
    int below (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (uint64_t) n); }
    bool chance (double p) noexcept { return unit() < p; }

    // Pick an index using non-negative weights.
    int weighted (const std::vector<double>& w) noexcept
    {
        double total = 0;
        for (auto x : w) total += x > 0 ? x : 0;
        if (total <= 0) return 0;
        double r = unit() * total;
        for (size_t i = 0; i < w.size(); ++i)
        {
            double x = w[i] > 0 ? w[i] : 0;
            if (r < x) return (int) i;
            r -= x;
        }
        return (int) w.size() - 1;
    }

    template <typename T>
    void shuffle (std::vector<T>& v) noexcept
    {
        for (int i = (int) v.size() - 1; i > 0; --i)
        {
            int j = below (i + 1);
            std::swap (v[(size_t) i], v[(size_t) j]);
        }
    }

private:
    uint64_t state;
};
} // namespace te
