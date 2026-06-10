#include "SpaceStage.h"
#include "../core/ParameterIds.h"
#include "../util/Constants.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void SpaceStage::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int capacity = static_cast<int> (sr * static_cast<double> (kMaxSpaceSeconds)) + 1;

    delayL_.resize (capacity);
    delayR_.resize (capacity);

    // Slightly detuned L/R times give a diffuse, reverb-ish tail.
    delaySamplesL_ = static_cast<int> (sr * 0.311);
    delaySamplesR_ = static_cast<int> (sr * 0.402);

    dampL_.prepare (sr);
    dampR_.prepare (sr);
    dampL_.setCutoffHz (3500.0f);
    dampR_.setCutoffHz (3500.0f);

    amount_.prepare (sr, 0.03f);
    feedback_.prepare (sr, 0.03f);
    mix_.prepare (sr, 0.03f);
    amount_.snap (0.25f);
    feedback_.snap (0.35f);
    mix_.snap (0.25f);

    reset();
}

void SpaceStage::reset()
{
    delayL_.clear();
    delayR_.clear();
    dampL_.reset();
    dampR_.reset();
}

void SpaceStage::setParams (float amount01, float feedback01, float mix01)
{
    amount_.setTarget (amount01);
    feedback_.setTarget (feedback01);
    mix_.setTarget (mix01);
}

void SpaceStage::process (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float amount   = amount_.next();
        const float feedback = map::spaceFeedback (feedback_.next());
        const float mix      = mix_.next();

        const float wetL = delayL_.read (delaySamplesL_);
        const float wetR = delayR_.read (delaySamplesR_);

        // Cross-feedback with damping keeps the tail soft and stable.
        delayL_.push (sanitize (left[i]  * amount + dampL_.processLowpass (wetR) * feedback));
        delayR_.push (sanitize (right[i] * amount + dampR_.processLowpass (wetL) * feedback));

        left[i]  = lerp (left[i],  left[i]  + wetL, mix);
        right[i] = lerp (right[i], right[i] + wetR, mix);
    }
}

} // namespace sculpt
