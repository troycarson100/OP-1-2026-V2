#pragma once

#include "GrainPatternTypes.h"
#include <algorithm>
#include <array>

namespace sculpt
{
namespace grain_patterns
{

constexpr GrainPatternStep S (float pos, int8_t ps, float sz, float g, uint8_t r)
{
    return { pos, ps, sz, g, r };
}

constexpr GrainPattern makeFlat()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<size_t> (i)] = S (0, 0, 1, 0, 1);
    return p;
}

constexpr GrainPattern makeRiser()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const float   pos = 0.4f * static_cast<float> (i) / 15.0f;
        const int8_t ps = (i % 4 == 2 ? 7 : (i % 4 == 3 ? 12 : 0));
        const float   sz = 1.0f - 0.12f * static_cast<float> (i) / 15.0f;
        p.steps[static_cast<size_t> (i)] = S (pos, ps, sz, 0, 1);
    }
    return p;
}

constexpr GrainPattern makeFaller()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const float pos = 0.4f * static_cast<float> (15 - i) / 15.0f;
        const int8_t ps = (i % 4 == 2 ? -5 : (i % 4 == 3 ? -12 : 0));
        const float sz  = 1.0f - 0.1f * static_cast<float> (i) / 15.0f;
        p.steps[static_cast<size_t> (i)] = S (pos, ps, sz, 0, 1);
    }
    return p;
}

constexpr GrainPattern makeOctaver()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const bool hi = (i & 1) != 0;
        p.steps[static_cast<size_t> (i)] = S (0, static_cast<int8_t> (hi ? 12 : 0), 1.0f, hi ? 2.0f : 0.0f, 1);
    }
    return p;
}

constexpr GrainPattern makeFifths()
{
    // 16-step pitch motif 0,+7,0,+7,+12,+7,0,-5 repeating-ish
    constexpr int8_t seq[16] = { 0, 7, 0, 7, 12, 7, 0, -5, 0, 7, 12, 5, 0, 7, 0, -7 };
    GrainPattern     p {};
    for (int i = 0; i < 16; ++i)
    {
        const float g = (i % 3 == 0 ? 0.5f : 0.0f);
        p.steps[static_cast<size_t> (i)] = S (0, seq[static_cast<size_t> (i)], 1.0f, g, 1);
    }
    return p;
}

constexpr GrainPattern makeChopper()
{
    constexpr float pos4[4] = { -0.375f, -0.125f, 0.125f, 0.375f };
    GrainPattern    p {};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<size_t> (i)] = S (pos4[static_cast<size_t> (i % 4)], 0, 0.5f, 0, 1);
    return p;
}

constexpr GrainPattern makeStutter()
{
    constexpr uint8_t rat[16] = { 1, 1, 2, 1, 1, 1, 4, 1, 1, 2, 1, 1, 1, 1, 4, 2 };
    GrainPattern        p {};
    for (int i = 0; i < 16; ++i)
    {
        const uint8_t r = rat[static_cast<size_t> (i)];
        const float   sz = r > 1 ? 0.4f : 1.0f;
        p.steps[static_cast<size_t> (i)] = S (0, 0, sz, 0, r);
    }
    return p;
}

constexpr GrainPattern makeSparseBloom()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const bool acc = (i == 0 || i == 5 || i == 10);
        p.steps[static_cast<size_t> (i)] = S (0, 0, acc ? 1.85f : 1.0f, acc ? 2.0f : -6.0f, 1);
    }
    return p;
}

constexpr GrainPattern makeDownshift()
{
    constexpr int8_t ps[16] = { 0, 0, -2, -2, -4, -5, -7, -7, -9, -10, -12, -12, -14, -15, -17, -19 };
    GrainPattern       p {};
    for (int i = 0; i < 16; ++i)
    {
        const float sz = 1.0f + 0.35f * static_cast<float> (i) / 15.0f;
        p.steps[static_cast<size_t> (i)] = S (0, ps[static_cast<size_t> (i)], sz, 0, 1);
    }
    return p;
}

constexpr GrainPattern makeSkipStep()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<size_t> (i)] = S (0.0625f * static_cast<float> (i), 0, 1.0f, 0, 1);
    return p;
}

constexpr GrainPattern makeCallResponse()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        if (i < 8)
            p.steps[static_cast<size_t> (i)] = S (-0.25f, 0, 1.0f, 0, 1);
        else
            p.steps[static_cast<size_t> (i)] = S (0.25f, 12, 1.0f, (i == 8 ? -2.5f : 0.0f), 1);
    }
    return p;
}

constexpr GrainPattern makeTripletGhost()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const bool hit = (i % 3) == 0;
        const uint8_t r = (i == 12 ? 3 : 1);
        p.steps[static_cast<size_t> (i)] = S (0, 0, 1.0f, hit ? 0.0f : -5.0f, r);
    }
    return p;
}

constexpr GrainPattern makeBounce()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const int j = i % 4;
        const float sz = 1.85f - (1.45f * static_cast<float> (j) / 3.0f);
        const int8_t ps = static_cast<int8_t> (10 - 3 * j);
        p.steps[static_cast<size_t> (i)] = S (0, ps, sz, 0, 1);
    }
    return p;
}

constexpr GrainPattern makeDrunk()
{
    // Fixed pseudo-random walk (deterministic).
    constexpr float pos[16] = { 0.04f,  -0.09f, 0.07f,  0.18f,  -0.15f, 0.025f, -0.06f, 0.11f,
                                -0.17f, 0.075f, -0.035f, 0.14f,  -0.11f, 0.05f,  -0.16f, 0.10f };
    constexpr int8_t ps[16] = { 0, 2, -1, 0, -2, 1, 0, 2, -1, -2, 1, 0, 2, -1, 0, -2 };
    GrainPattern     p {};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<size_t> (i)] = S (pos[static_cast<size_t> (i)], ps[static_cast<size_t> (i)], 1.0f, 0, 1);
    return p;
}

constexpr GrainPattern makeMaxChaos()
{
    constexpr float pos[16] = { 0.28f,  -0.32f, 0.16f,  -0.2f,  0.34f,  -0.27f, 0.14f,  -0.3f,
                                0.22f,  -0.18f, 0.25f,  -0.16f, 0.32f,  -0.28f, 0.12f,  -0.33f };
    constexpr int8_t sem[16] = { 7, -7, 5, -5, 12, -10, 4, -4, 9, -8, 10, -12, 3, -3, 7, -7 };
    constexpr uint8_t rt[16] = { 2, 1, 3, 1, 3, 1, 2, 2, 1, 3, 1, 3, 1, 2, 1, 2 };
    GrainPattern      p {};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<size_t> (i)] = S (pos[static_cast<size_t> (i)], sem[static_cast<size_t> (i)], 0.82f,
                                            (i & 1) ? 1.2f : -2.0f, rt[static_cast<size_t> (i)]);
    return p;
}

// std::sin is not constexpr in all stdlibs for C++17 — use series for pendulum at compile time:
constexpr float sinApprox (float x)
{
    // x in [-pi, pi]
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + x2 * x2 / 120.0f - x2 * x2 * x2 / 5040.0f);
}

constexpr GrainPattern makePendulumSafe()
{
    GrainPattern p {};
    for (int i = 0; i < 16; ++i)
    {
        const float ph  = 6.2831853f * static_cast<float> (i) / 16.0f;
        const float pos = 0.3f * sinApprox (ph);
        p.steps[static_cast<size_t> (i)] = S (pos, 0, 1.0f, 0, 1);
    }
    return p;
}

} // namespace grain_patterns

inline constexpr std::array<GrainPattern, kNumGrainPatterns> kGrainPatternBank {
    grain_patterns::makeFlat(),
    grain_patterns::makeRiser(),
    grain_patterns::makeFaller(),
    grain_patterns::makeOctaver(),
    grain_patterns::makeFifths(),
    grain_patterns::makePendulumSafe(),
    grain_patterns::makeChopper(),
    grain_patterns::makeStutter(),
    grain_patterns::makeSparseBloom(),
    grain_patterns::makeDownshift(),
    grain_patterns::makeSkipStep(),
    grain_patterns::makeCallResponse(),
    grain_patterns::makeTripletGhost(),
    grain_patterns::makeBounce(),
    grain_patterns::makeDrunk(),
    grain_patterns::makeMaxChaos(),
};

// UI / ScreenModel only (never read from audio thread for DSP strings).
inline constexpr const char* kGrainPatternUiNames[kNumGrainPatterns] = {
    "Flat",         "Riser",        "Faller",       "Octaver",    "Fifths",     "Pendulum", "Chopper", "Stutter",
    "Sparse Bloom", "Downshift",    "Skip Step",    "Call/Resp.", "Triplet Gh.", "Bounce",  "Drunk",   "Max Chaos",
};

inline const GrainPatternStep& grainPatternStep (int patternIndex, int step16)
{
    const int pi = std::clamp (patternIndex, 0, kNumGrainPatterns - 1);
    const int s  = ((step16 % 16) + 16) % 16;
    return kGrainPatternBank[static_cast<size_t> (pi)].steps[static_cast<size_t> (s)];
}

} // namespace sculpt
