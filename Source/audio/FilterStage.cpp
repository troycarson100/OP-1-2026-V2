#include <cmath>
#include "FilterStage.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void FilterStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    cutoff_.prepare (sampleRate_, 0.02f);
    resonance_.prepare (sampleRate_, 0.02f);
    mix_.prepare (sampleRate_, 0.02f);
    cutoff_.snap (0.8f);
    resonance_.snap (0.2f);
    mix_.snap (1.0f);
    reset();
}

void FilterStage::reset()
{
    left_  = ChannelState {};
    right_ = ChannelState {};
}

void FilterStage::setParams (float cutoff01, float resonance01, float mix01)
{
    cutoff_.setTarget (cutoff01);
    resonance_.setTarget (resonance01);
    mix_.setTarget (mix01);
}

float FilterStage::processSample (ChannelState& s, float in, float g, float k) const
{
    // TPT SVF (Zavalishin). Low-pass output.
    const float v1 = (s.ic1 + g * (in - s.ic2)) / (1.0f + g * (g + k));
    const float v2 = s.ic2 + g * v1;
    s.ic1 = sanitize (2.0f * v1 - s.ic1);
    s.ic2 = sanitize (2.0f * v2 - s.ic2);
    return v2;
}

void FilterStage::process (float* left, float* right, int numSamples)
{
    // Coefficients update once per block (parameters are block-smoothed).
    const float cutoffHz = map::filterCutoffHz (cutoff_.skip (numSamples));
    const float q        = map::filterResonance (resonance_.skip (numSamples));
    const float maxHz    = static_cast<float> (sampleRate_) * 0.45f;
    const float g = std::tan (kPi * clampf (cutoffHz, 20.0f, maxHz) / static_cast<float> (sampleRate_));
    const float k = 1.0f / q;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mix_.next();
        const float fl = processSample (left_,  left[i],  g, k);
        const float fr = processSample (right_, right[i], g, k);
        left[i]  = lerp (left[i],  fl, mix);
        right[i] = lerp (right[i], fr, mix);
    }
}

} // namespace sculpt
