#pragma once

#include <cstdint>

namespace sculpt
{

// One step in a 16-step grain choreography (offsets on top of live knob values).
struct GrainPatternStep
{
    float   posOffset = 0.0f;   // -0.5 .. +0.5 normalized, added to position then wrapped
    int8_t  pitchSemis = 0;     // -24 .. +24, added before pitch ratio / quantize
    float   sizeScale  = 1.0f;  // 0.25 .. 2.0, multiplies mapped grain length
    float   gainDb     = 0.0f;  // per-step accent dB (scaled by GrainPatternAmount)
    uint8_t ratchet    = 1;     // 1..4 sub-hits within one division period
};

struct GrainPattern
{
    GrainPatternStep steps[16] {};
};

constexpr int kNumGrainPatterns     = 16;
constexpr int kGrainPatternNumSteps = 16;

} // namespace sculpt
