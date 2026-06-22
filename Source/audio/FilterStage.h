#pragma once

#include "../util/SmoothedValue.h"

namespace sculpt
{

// Enhanced state-variable filter (TPT) with LP/BP/HP mode select.
// mode01: 0 = Low-pass, 0.5 = Band-pass, 1 = High-pass (continuously morphable).
class FilterStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // Normalized parameters; mapping happens via sculpt::map.
    // mode01 selects LP (0) → BP (0.5) → HP (1.0) by morphing SVF outputs.
    void setParams (float cutoff01, float resonance01, float mix01, float mode01 = 0.0f);

    void process (float* left, float* right, int numSamples);

    // Analog 12 dB/oct SVF magnitude response in dB at freqHz, for the morphed LP/BP/HP filter
    // (mode01: 0=LP, 0.5=BP, 1=HP). Static so the engine can plot the curve with no instance.
    static float responseDb (float freqHz, float cutoffHz, float q, float mode01);

private:
    struct ChannelState { float ic1 = 0.0f; float ic2 = 0.0f; };

    float processSample (ChannelState& state, float in, float g, float k, float drive,
                         float& outBP, float& outHP) const;

    double sampleRate_ = 44100.0;
    ChannelState left_, right_;

    SmoothedValue cutoff_, resonance_, mix_, mode_;
};

} // namespace sculpt
