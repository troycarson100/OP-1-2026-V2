#pragma once

#include "Modulator.h"

namespace sculpt
{

// Tracks the level of an audio signal. Feed it audio with processAudio()
// during the block; getValue() returns a unipolar level in [0, 1].
class EnvelopeFollower : public Modulator
{
public:
    void prepare (double sampleRate) override;
    void reset() override;
    void update (int numSamples) override { (void) numSamples; }
    float getValue() const override { return envelope_; }

    void setTimesMs (float attackMs, float releaseMs);

    // Call once per block with the signal to follow (any channel count >= 1).
    void processAudio (const float* const* channels, int numChannels, int numSamples);

private:
    void updateCoefficients();

    double sampleRate_ = 44100.0;
    float  attackMs_   = 5.0f;
    float  releaseMs_  = 120.0f;
    float  attackCoeff_  = 0.0f;
    float  releaseCoeff_ = 0.0f;
    float  envelope_   = 0.0f;
};

} // namespace sculpt
