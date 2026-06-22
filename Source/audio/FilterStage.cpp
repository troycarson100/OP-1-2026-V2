#include <cmath>
#include "FilterStage.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

namespace
{
    // Cheap bounded tanh-style soft clipper (Padé approx, saturates to +/-1 beyond |x|~3).
    // Transparent for small signals, so the filter stays clean until pushed.
    inline float softClip (float x)
    {
        const float ax = x < 0.0f ? -x : x;
        if (ax > 3.0f)
            return x < 0.0f ? -1.0f : 1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

void FilterStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    cutoff_.prepare    (sampleRate_, 0.02f);
    resonance_.prepare (sampleRate_, 0.02f);
    mix_.prepare       (sampleRate_, 0.02f);
    mode_.prepare      (sampleRate_, 0.05f);
    cutoff_.snap    (0.8f);
    resonance_.snap (0.2f);
    mix_.snap       (1.0f);
    mode_.snap      (0.0f);
    reset();
}

void FilterStage::reset()
{
    left_  = ChannelState {};
    right_ = ChannelState {};
}

void FilterStage::setParams (float cutoff01, float resonance01, float mix01, float mode01)
{
    cutoff_.setTarget    (cutoff01);
    resonance_.setTarget (resonance01);
    mix_.setTarget       (mix01);
    mode_.setTarget      (mode01);
}

float FilterStage::processSample (ChannelState& s, float in, float g, float k, float drive,
                                   float& outBP, float& outHP) const
{
    // TPT SVF (Zavalishin) with an analog-style nonlinearity. Input is pre-driven; the resonant
    // integrator state is soft-clipped, which adds harmonic warmth and bounds (tames) the
    // resonance peak so high Q can scream and self-oscillate without blowing up.
    const float inD = in * drive;
    const float v1 = (s.ic1 + g * (inD - s.ic2)) / (1.0f + g * (g + k));
    const float v2 = s.ic2 + g * v1;
    s.ic1 = sanitize (softClip (2.0f * v1 - s.ic1));
    s.ic2 = sanitize (2.0f * v2 - s.ic2);
    outBP = v1;
    outHP = inD - k * v1 - v2;
    return v2;   // LP
}

float FilterStage::responseDb (float freqHz, float cutoffHz, float q, float mode01)
{
    const float w   = freqHz / (cutoffHz > 1.0e-3f ? cutoffHz : 1.0e-3f);  // normalized frequency
    const float w2  = w * w;
    const float den = std::sqrt ((1.0f - w2) * (1.0f - w2) + (w / q) * (w / q)) + 1.0e-9f;
    const float lp  = 1.0f / den;
    const float bp  = (w / q) / den;
    const float hp  = w2 / den;

    const float m   = clamp01 (mode01);
    const float mag = (m < 0.5f) ? lerp (lp, bp, m * 2.0f)
                                 : lerp (bp, hp, (m - 0.5f) * 2.0f);
    return 20.0f * std::log10 (mag + 1.0e-9f);
}

void FilterStage::process (float* left, float* right, int numSamples)
{
    const float cutoffHz = map::filterCutoffHz (cutoff_.skip (numSamples));
    const float res01    = clamp01 (resonance_.skip (numSamples));
    const float q        = map::filterResonance (res01);
    const float maxHz    = static_cast<float> (sampleRate_) * 0.45f;
    const float g = std::tan (kPi * clampf (cutoffHz, 20.0f, maxHz) / static_cast<float> (sampleRate_));
    const float k = 1.0f / q;
    // Drive into the saturator grows with resonance: clean at low res, juicy/overdriven near the
    // top. Compensate the output so the perceived level stays sane as drive climbs.
    const float drive = 1.0f + res01 * res01 * 3.0f;
    const float comp  = 1.0f / std::sqrt (drive);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix  = mix_.next();
        // mode: 0 = LP, 0.5 = BP, 1 = HP (continuous morph through SVF outputs).
        const float mode = clamp01 (mode_.next());

        float bpL, hpL, bpR, hpR;
        const float lpL = processSample (left_,  left[i],  g, k, drive, bpL, hpL);
        const float lpR = processSample (right_, right[i], g, k, drive, bpR, hpR);

        float outL, outR;
        if (mode < 0.5f)
        {
            const float t = mode * 2.0f;   // 0..1 across LP→BP
            outL = lerp (lpL, bpL, t);
            outR = lerp (lpR, bpR, t);
        }
        else
        {
            const float t = (mode - 0.5f) * 2.0f;  // 0..1 across BP→HP
            outL = lerp (bpL, hpL, t);
            outR = lerp (bpR, hpR, t);
        }

        left[i]  = lerp (left[i],  outL * comp, mix);
        right[i] = lerp (right[i], outR * comp, mix);
    }
}

} // namespace sculpt
