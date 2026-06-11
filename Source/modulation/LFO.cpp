#include <cmath>
#include "LFO.h"
#include "../util/MathUtils.h"

namespace sculpt
{

float LFO::valueForShape (Shape s, double phase01)
{
    phase01 -= std::floor (phase01);
    if (phase01 < 0.0)
        phase01 += 1.0;

    switch (s)
    {
        case Shape::Sine:
            return std::sin (static_cast<float> (phase01) * kTwoPi);
        case Shape::Triangle:
        {
            const float p = static_cast<float> (phase01);
            return p < 0.5f ? (p * 4.0f - 1.0f) : (3.0f - p * 4.0f);
        }
        case Shape::SawUp:
            return static_cast<float> (phase01 * 2.0 - 1.0);
        case Shape::Square:
            return phase01 < 0.5 ? -1.0f : 1.0f;
        case Shape::RampDown:
            return static_cast<float> (1.0 - phase01 * 2.0);
        case Shape::Count:
        default:
            return 0.0f;
    }
}

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

float LFO::getWrappedPhase01() const
{
    double p = phase_ - std::floor (phase_);
    if (p < 0.0)
        p += 1.0;
    return static_cast<float> (p);
}

void LFO::update (int numSamples)
{
    phase_ += static_cast<double> (rateHz_) * static_cast<double> (numSamples) / sampleRate_;
    phase_ -= static_cast<double> (static_cast<long long> (phase_));
    value_ = valueForShape (shape_, phase_);
}

} // namespace sculpt
