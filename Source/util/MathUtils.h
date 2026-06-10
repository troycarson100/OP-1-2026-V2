#pragma once

#include <cmath>

// Small math helpers used across the portable engine. Header-only, no JUCE.
namespace sculpt
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float clamp01 (float v)        { return clampf (v, 0.0f, 1.0f); }

inline float lerp (float a, float b, float t)  { return a + (b - a) * t; }

inline float dbToGain (float db)      { return std::pow (10.0f, db * 0.05f); }

inline float semitonesToRatio (float semis) { return std::pow (2.0f, semis / 12.0f); }

// Equal-power pan. pan in [-1, 1]. Writes left/right gains.
inline void equalPowerPan (float pan, float& gainL, float& gainR)
{
    const float angle = (clampf (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    gainL = std::cos (angle);
    gainR = std::sin (angle);
}

// Flush denormals / NaNs that could poison feedback paths.
inline float sanitize (float v)
{
    if (! std::isfinite (v) || std::fabs (v) < 1.0e-20f)
        return 0.0f;
    return v;
}

} // namespace sculpt
