#pragma once

#include <cstdint>

namespace sculpt
{

// Minimal ADSR 0..1 envelope as a modulation source. Triggered from UI via Engine.
class AdsrModulator
{
public:
    void prepare (double sampleRate);
    void reset();
    void trigger();

    void setParams (float attackSec, float decaySec, float sustain01, float releaseSec);

    void process (int numSamples);
    float getValue() const { return value_; }

private:
    enum class Phase : uint8_t { Idle, Attack, Decay, Release };

    double sampleRate_ = 44100.0;
    Phase  phase_      = Phase::Idle;
    double timeInPhase_ = 0.0;
    float  value_      = 0.0f;

    float attackSec_  = 0.02f;
    float decaySec_   = 0.08f;
    float sustain01_  = 0.65f;
    float releaseSec_ = 0.12f;
};

} // namespace sculpt
