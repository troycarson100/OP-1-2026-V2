#include <algorithm>
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
    contour_   = params.contour < 0.0f ? 0.0f : (params.contour > 1.0f ? 1.0f : params.contour);
    age_       = 0;
    active_    = true;
    startOffsetRemaining_ = params.startOffsetSamples > 0 ? params.startOffsetSamples : 0;

    originStartFrame_ = params.startFrame;
    originLength_     = length_;
}

float GrainVoice::phase01() const
{
    if (length_ <= 0)
        return 0.0f;
    if (startOffsetRemaining_ > 0)
        return 0.0f;
    return std::clamp (static_cast<float> (age_) / static_cast<float> (length_), 0.0f, 1.0f);
}

void GrainVoice::render (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    if (! active_ || buffer.getNumFrames() < 2)
        return;

    const int   rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;
    const float lenF         = static_cast<float> (length_);
    float       t01          = static_cast<float> (age_) / lenF;
    const float t01Step      = 1.0f / lenF;

    for (int i = 0; i < numSamples; ++i)
    {
        if (startOffsetRemaining_ > 0)
        {
            --startOffsetRemaining_;
            continue;
        }

        if (age_ >= length_)
        {
            active_ = false;
            return;
        }

        // Contour envelope: morph from Hann bell (smooth cloud) to percussive hit.
        // contour_=0 → pure Hann; contour_=1 → 3% linear attack + exponential decay.
        const float hann = 0.5f - 0.5f * std::cos (kTwoPi * t01);
        float window;
        if (contour_ < 0.001f)
        {
            window = hann;
        }
        else
        {
            constexpr float kAtt = 0.03f;
            const float perc = (t01 < kAtt) ? t01 / kAtt
                                             : std::exp (-4.5f * (t01 - kAtt) / (1.0f - kAtt));
            window = hann + contour_ * (perc - hann);
        }

        outL[i] += buffer.getSampleLinear (0, position_) * window * gainL_;
        outR[i] += buffer.getSampleLinear (rightChannel, position_) * window * gainR_;

        position_ += increment_;
        ++age_;
        t01 += t01Step;
    }
}

} // namespace sculpt
