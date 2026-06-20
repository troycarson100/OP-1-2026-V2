#pragma once

#include <array>
#include "TapePlayer.h"
#include "GranularEngine.h"
#include "FilterStage.h"
#include "SpectralFilterStage.h"
#include "ColorStage.h"
#include "MixBusStage.h"
#include "../util/Constants.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

class SampleBuffer;
struct GranularBlockTiming;

// The DSP chain for one track:
//   Material(tape) -> Granular -> Filter -> Color -> MixBus (EQ + comp)
// Reverb/delay are no longer per-track: each track feeds the instrument-wide SpaceSendBus via its
// Reverb/Delay Send (handled in Engine). Track owns the material and decides when this runs;
// TrackEngine only processes audio. Scratch buffers are fixed-size members - no allocation.
class TrackEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    TapePlayer&          getTape()           { return tape_; }
    GranularEngine&      getGranular()       { return granular_; }
    FilterStage&         getFilter()         { return filter_; }
    SpectralFilterStage& getSpectralFilter() { return spectralFilter_; }
    ColorStage&          getColor()          { return color_; }
    MixBusStage&         getMixBus()         { return mixBus_; }
    const MixBusStage&   getMixBus() const   { return mixBus_; }

    const TapePlayer&          getTape()           const { return tape_; }
    const GranularEngine&      getGranular()       const { return granular_; }
    const SpectralFilterStage& getSpectralFilter() const { return spectralFilter_; }

    // Selects which filter runs in process(). false = LPF/BP/HP, true = Spectral.
    void setSpectralMode (bool spectral) { spectralMode_ = spectral; }

    void setMaterialLevel (float level01) { materialLevel_.setTarget (level01); }
    void setGrainMix (float mix01)        { grainMix_.setTarget (mix01); }
    void snapGrainMix (float mix01)       { grainMix_.snap (mix01); }

    void setGranularBlockTiming (const GranularBlockTiming& t);

    // Overwrites outL/outR with the processed chain. numSamples <= kMaxBlockSize.
    void process (const SampleBuffer& material, float* outL, float* outR, int numSamples);

private:
    TapePlayer           tape_;
    GranularEngine       granular_;
    FilterStage          filter_;
    SpectralFilterStage  spectralFilter_;
    ColorStage           color_;
    MixBusStage          mixBus_;

    bool spectralMode_ = false;

    SmoothedValue materialLevel_;
    SmoothedValue grainMix_;

    std::array<float, kMaxBlockSize> dryL_ {}, dryR_ {};
    std::array<float, kMaxBlockSize> grainL_ {}, grainR_ {};
};

} // namespace sculpt
