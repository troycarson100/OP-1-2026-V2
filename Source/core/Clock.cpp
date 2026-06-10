#include "Clock.h"

namespace sculpt
{

void Clock::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void Clock::reset()
{
    samplePosition_ = 0;
    beatPosition_   = 0.0;
}

void Clock::setBpm (double bpm)
{
    if (bpm > 1.0 && bpm < 999.0)
        bpm_ = bpm;
}

void Clock::advance (int numSamples)
{
    samplePosition_ += static_cast<uint64_t> (numSamples);
    beatPosition_   += static_cast<double> (numSamples) * bpm_ / (60.0 * sampleRate_);
}

double Clock::getBeatPhase() const
{
    return beatPosition_ - static_cast<double> (static_cast<long long> (beatPosition_));
}

} // namespace sculpt
