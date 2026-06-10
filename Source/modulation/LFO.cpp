#include <cmath>
#include "LFO.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void LFO::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void LFO::reset()
{
    phase_ = 0.0;
    value_ = 0.0f;
}

void LFO::setPhase (float phase01)
{
    phase_ = static_cast<double> (clamp01 (phase01));
}

void LFO::update (int numSamples)
{
    phase_ += static_cast<double> (rateHz_) * static_cast<double> (numSamples) / sampleRate_;
    phase_ -= static_cast<double> (static_cast<long long> (phase_));

    if (shape_ == Shape::Sine)
    {
        value_ = std::sin (static_cast<float> (phase_) * kTwoPi);
    }
    else
    {
        const float p = static_cast<float> (phase_);
        value_ = p < 0.5f ? (p * 4.0f - 1.0f) : (3.0f - p * 4.0f);
    }
}

} // namespace sculpt
