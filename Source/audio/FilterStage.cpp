#include <cmath>
#include "FilterStage.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

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

float FilterStage::processSample (ChannelState& s, float in, float g, float k,
                                   float& outBP, float& outHP) const
{
    // TPT SVF (Zavalishin). Exposes all three outputs.
    const float v1 = (s.ic1 + g * (in - s.ic2)) / (1.0f + g * (g + k));
    const float v2 = s.ic2 + g * v1;
    s.ic1 = sanitize (2.0f * v1 - s.ic1);
    s.ic2 = sanitize (2.0f * v2 - s.ic2);
    outBP = v1;
    outHP = in - k * v1 - v2;
    return v2;   // LP
}

void FilterStage::process (float* left, float* right, int numSamples)
{
    const float cutoffHz = map::filterCutoffHz (cutoff_.skip (numSamples));
    const float q        = map::filterResonance (resonance_.skip (numSamples));
    const float maxHz    = static_cast<float> (sampleRate_) * 0.45f;
    const float g = std::tan (kPi * clampf (cutoffHz, 20.0f, maxHz) / static_cast<float> (sampleRate_));
    const float k = 1.0f / q;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix  = mix_.next();
        // mode: 0 = LP, 0.5 = BP, 1 = HP (continuous morph through SVF outputs).
        const float mode = clamp01 (mode_.next());

        float bpL, hpL, bpR, hpR;
        const float lpL = processSample (left_,  left[i],  g, k, bpL, hpL);
        const float lpR = processSample (right_, right[i], g, k, bpR, hpR);

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

        left[i]  = lerp (left[i],  outL, mix);
        right[i] = lerp (right[i], outR, mix);
    }
}

} // namespace sculpt
