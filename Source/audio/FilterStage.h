#pragma once

#include "../util/SmoothedValue.h"

namespace sculpt
{

// Filter stage: stereo state-variable filter (TPT), low-pass for now.
// HP/BP outputs are available in the topology when needed later.
class FilterStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // Normalized parameters; mapping happens via sculpt::map.
    void setParams (float cutoff01, float resonance01, float mix01);

    void process (float* left, float* right, int numSamples);

private:
    struct ChannelState { float ic1 = 0.0f; float ic2 = 0.0f; };

    float processSample (ChannelState& state, float in, float g, float k) const;

    double sampleRate_ = 44100.0;
    ChannelState left_, right_;

    SmoothedValue cutoff_, resonance_, mix_;
};

} // namespace sculpt
