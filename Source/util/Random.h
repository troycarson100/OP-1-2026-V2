#pragma once

#include <cstdint>

namespace sculpt
{

// Small xorshift PRNG. Deterministic, allocation-free, real-time safe.
class Random
{
public:
    explicit Random (uint32_t seed = 0x12345678u) : state_ (seed != 0 ? seed : 1u) {}

    void seed (uint32_t s) { state_ = (s != 0 ? s : 1u); }

    uint32_t nextUInt()
    {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    // [0, 1)
    float nextFloat()
    {
        return static_cast<float> (nextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    // [-1, 1)
    float nextBipolar()
    {
        return nextFloat() * 2.0f - 1.0f;
    }

private:
    uint32_t state_;
};

} // namespace sculpt
