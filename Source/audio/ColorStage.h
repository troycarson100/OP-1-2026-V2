#pragma once

#include "OnePole.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

// Color stage: tanh drive with tone tilt. Adds saturation/character.
class ColorStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // Normalized parameters. tone < 0.5 darkens, > 0.5 brightens.
    void setParams (float drive01, float tone01, float mix01);

    void process (float* left, float* right, int numSamples);

private:
    float processSample (OnePole& toneFilter, float in, float driveGain, float tone, float mix);

    OnePole toneL_, toneR_;
    SmoothedValue drive_, tone_, mix_;
};

} // namespace sculpt
