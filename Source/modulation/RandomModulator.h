#pragma once

#include "Modulator.h"
#include "../util/Random.h"

namespace sculpt
{

// Sample-and-hold random source with slewing. Bipolar output [-1, 1].
class RandomModulator : public Modulator
{
public:
    void prepare (double sampleRate) override;
    void reset() override;
    void update (int numSamples) override;
    float getValue() const override { return value_; }

    void setRateHz (float hz)    { rateHz_ = hz > 0.01f ? hz : 0.01f; }
    void setSlew (float amount01){ slew_ = amount01; }
    void setSeed (uint32_t seed) { rng_.seed (seed); }

    // Beat-synced sample-hold: new target when floor(beat*cyclesPerBeat) crosses.
    void updateSync (double beatStart, double beatEnd, double cyclesPerBeat, float slew01);

private:
    double  sampleRate_      = 44100.0;
    double  samplesUntilNext_ = 0.0;
    float   rateHz_          = 2.0f;
    float   slew_            = 0.5f;
    float   target_          = 0.0f;
    float   value_           = 0.0f;
    Random  rng_ { 0xBEEF1234u };
};

} // namespace sculpt
