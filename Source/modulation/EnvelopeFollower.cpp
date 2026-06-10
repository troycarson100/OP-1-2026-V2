#include <cmath>
#include "EnvelopeFollower.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void EnvelopeFollower::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
    reset();
}

void EnvelopeFollower::reset()
{
    envelope_ = 0.0f;
}

void EnvelopeFollower::setTimesMs (float attackMs, float releaseMs)
{
    attackMs_  = attackMs  > 0.1f ? attackMs  : 0.1f;
    releaseMs_ = releaseMs > 1.0f ? releaseMs : 1.0f;
    updateCoefficients();
}

void EnvelopeFollower::updateCoefficients()
{
    attackCoeff_  = std::exp (-1.0f / (static_cast<float> (sampleRate_) * attackMs_  * 0.001f));
    releaseCoeff_ = std::exp (-1.0f / (static_cast<float> (sampleRate_) * releaseMs_ * 0.001f));
}

void EnvelopeFollower::processAudio (const float* const* channels, int numChannels, int numSamples)
{
    if (channels == nullptr || numChannels < 1)
        return;

    float env = envelope_;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += std::fabs (channels[ch][i]);
        mono /= static_cast<float> (numChannels);

        const float coeff = mono > env ? attackCoeff_ : releaseCoeff_;
        env = mono + coeff * (env - mono);
    }

    envelope_ = clamp01 (sanitize (env));
}

} // namespace sculpt
