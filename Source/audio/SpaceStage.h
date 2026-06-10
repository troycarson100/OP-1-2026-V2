#pragma once

#include "OnePole.h"
#include "../util/RingBuffer.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

// Space stage: lightweight stereo cross-feedback delay with damping.
// Reads like a small ambience/echo, cheap enough for four tracks.
// Delay lines are sized in prepare(); processing never allocates.
class SpaceStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // Normalized parameters: amount = send, feedback, mix = wet blend.
    void setParams (float amount01, float feedback01, float mix01);

    void process (float* left, float* right, int numSamples);

private:
    RingBuffer delayL_, delayR_;
    OnePole dampL_, dampR_;

    int delaySamplesL_ = 0;
    int delaySamplesR_ = 0;

    SmoothedValue amount_, feedback_, mix_;
};

} // namespace sculpt
