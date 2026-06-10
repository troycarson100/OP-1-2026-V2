#pragma once

#include <array>
#include "TapePlayer.h"
#include "GranularEngine.h"
#include "FilterStage.h"
#include "ColorStage.h"
#include "SpaceStage.h"
#include "../util/Constants.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

class SampleBuffer;

// The DSP chain for one track:
//   Material(tape) -> Granular -> Filter -> Color -> Space
// Track owns the material and decides when this runs; TrackEngine only
// processes audio. Scratch buffers are fixed-size members - no allocation.
class TrackEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    TapePlayer&     getTape()     { return tape_; }
    GranularEngine& getGranular() { return granular_; }
    FilterStage&    getFilter()   { return filter_; }
    ColorStage&     getColor()    { return color_; }
    SpaceStage&     getSpace()    { return space_; }

    const TapePlayer&     getTape() const     { return tape_; }
    const GranularEngine& getGranular() const { return granular_; }

    void setMaterialLevel (float level01) { materialLevel_.setTarget (level01); }
    void setGrainMix (float mix01)        { grainMix_.setTarget (mix01); }

    // Overwrites outL/outR with the processed chain. numSamples <= kMaxBlockSize.
    void process (const SampleBuffer& material, float* outL, float* outR, int numSamples);

private:
    TapePlayer     tape_;
    GranularEngine granular_;
    FilterStage    filter_;
    ColorStage     color_;
    SpaceStage     space_;

    SmoothedValue materialLevel_;
    SmoothedValue grainMix_;

    std::array<float, kMaxBlockSize> dryL_ {}, dryR_ {};
    std::array<float, kMaxBlockSize> grainL_ {}, grainR_ {};
};

} // namespace sculpt
