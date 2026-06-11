#include "RandomModulator.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void RandomModulator::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void RandomModulator::reset()
{
    samplesUntilNext_ = 0.0;
    target_ = 0.0f;
    value_  = 0.0f;
}

void RandomModulator::update (int numSamples)
{
    samplesUntilNext_ -= static_cast<double> (numSamples);

    if (samplesUntilNext_ <= 0.0)
    {
        target_ = rng_.nextBipolar();
        samplesUntilNext_ += sampleRate_ / static_cast<double> (rateHz_);
        if (samplesUntilNext_ <= 0.0)
            samplesUntilNext_ = sampleRate_ / static_cast<double> (rateHz_);
    }

    // Higher slew -> smoother glide toward the held target.
    const float blend = lerp (0.9f, 0.05f, clamp01 (slew_));
    value_ = lerp (value_, target_, blend);
}

void RandomModulator::updateSync (double beatStart, double beatEnd, double cyclesPerBeat, float slew01)
{
    const double b0 = beatStart * cyclesPerBeat;
    const double b1 = beatEnd * cyclesPerBeat;
    const auto   i0 = static_cast<long long> (std::floor (b0));
    const auto   i1 = static_cast<long long> (std::floor (b1));
    if (i1 != i0)
        target_ = rng_.nextBipolar();

    const float blend = lerp (0.9f, 0.05f, clamp01 (slew01));
    value_ = lerp (value_, target_, blend);
}

} // namespace sculpt
