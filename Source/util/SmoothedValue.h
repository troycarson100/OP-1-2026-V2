#pragma once

#include <cmath>

namespace sculpt
{

// One-pole parameter smoother. Real-time safe, no allocation.
// Call prepare() once, then setTarget()/next() freely on the audio thread.
class SmoothedValue
{
public:
    void prepare (double sampleRate, float smoothingSeconds)
    {
        const double samples = sampleRate * static_cast<double> (smoothingSeconds);
        coeff_ = samples > 1.0 ? static_cast<float> (std::exp (-1.0 / samples)) : 0.0f;
        current_ = target_;
    }

    void snap (float value)          { current_ = target_ = value; }
    void setTarget (float value)     { target_ = value; }

    float next()
    {
        current_ = target_ + coeff_ * (current_ - target_);
        return current_;
    }

    // Advance the smoother as if numSamples ticks elapsed, returning the end value.
    float skip (int numSamples)
    {
        current_ = target_ + std::pow (coeff_, static_cast<float> (numSamples)) * (current_ - target_);
        return current_;
    }

    float getCurrent() const         { return current_; }
    float getTarget() const          { return target_; }

private:
    float current_ = 0.0f;
    float target_  = 0.0f;
    float coeff_   = 0.0f;
};

} // namespace sculpt
