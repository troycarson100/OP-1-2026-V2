#include <cmath>
#include "GrainVoice.h"
#include "SampleBuffer.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void GrainVoice::start (const StartParams& params)
{
    position_  = params.startFrame;
    increment_ = params.increment;
    length_    = params.lengthSamples > 8 ? params.lengthSamples : 8;
    gainL_     = params.gainL;
    gainR_     = params.gainR;
    age_       = 0;
    active_    = true;
}

void GrainVoice::render (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    if (! active_ || buffer.getNumFrames() < 2)
        return;

    const int rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;
    const float windowStep = kTwoPi / static_cast<float> (length_);

    for (int i = 0; i < numSamples; ++i)
    {
        if (age_ >= length_)
        {
            active_ = false;
            return;
        }

        // Hann window.
        const float window = 0.5f - 0.5f * std::cos (windowStep * static_cast<float> (age_));

        outL[i] += buffer.getSampleLinear (0, position_) * window * gainL_;
        outR[i] += buffer.getSampleLinear (rightChannel, position_) * window * gainR_;

        position_ += increment_;
        ++age_;
    }
}

} // namespace sculpt
