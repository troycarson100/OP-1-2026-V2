#include <algorithm>
#include <cmath>
#include "AdsrModulator.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void AdsrModulator::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void AdsrModulator::reset()
{
    phase_       = Phase::Idle;
    timeInPhase_ = 0.0;
    value_       = 0.0f;
}

void AdsrModulator::setParams (float attackSec, float decaySec, float sustain01, float releaseSec)
{
    attackSec_  = std::max (0.001f, attackSec);
    decaySec_   = std::max (0.001f, decaySec);
    sustain01_  = clamp01 (sustain01);
    releaseSec_ = std::max (0.001f, releaseSec);
}

void AdsrModulator::trigger()
{
    phase_       = Phase::Attack;
    timeInPhase_ = 0.0;
}

void AdsrModulator::process (int numSamples)
{
    if (phase_ == Phase::Idle)
    {
        value_ = 0.0f;
        return;
    }

    const double dt = static_cast<double> (numSamples) / sampleRate_;

    auto segment = [&] (double durationSec, float y0, float y1, Phase nextPhase)
    {
        if (durationSec <= 1.0e-9)
        {
            value_       = y1;
            phase_       = nextPhase;
            timeInPhase_ = 0.0;
            return;
        }
        timeInPhase_ += dt;
        const float t = static_cast<float> (std::min (1.0, timeInPhase_ / durationSec));
        value_ = y0 + (y1 - y0) * t;
        if (timeInPhase_ >= durationSec)
        {
            phase_       = nextPhase;
            timeInPhase_ = 0.0;
        }
    };

    switch (phase_)
    {
        case Phase::Attack:
            segment (static_cast<double> (attackSec_), 0.0f, 1.0f, Phase::Decay);
            break;
        case Phase::Decay:
            segment (static_cast<double> (decaySec_), 1.0f, sustain01_, Phase::Release);
            break;
        case Phase::Release:
            segment (static_cast<double> (releaseSec_), sustain01_, 0.0f, Phase::Idle);
            if (phase_ == Phase::Idle)
                value_ = 0.0f;
            break;
        default:
            break;
    }
}

} // namespace sculpt
