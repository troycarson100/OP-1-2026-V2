#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "AdsrModulator.h"
#include "LFO.h"
#include "ModTypes.h"
#include "RandomModulator.h"
#include "../core/ParameterState.h"

namespace sculpt
{

struct ModLcdSnapshot;

// Per-track modulation buses (S-4-style): four slots, each with a modulator
// kind + params, and per–device-page mapping depths. Applies to ParameterState
// after clearModOffsets() each block. Portable, no JUCE.
class ModEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    void setLivePatch (const ModPatch& patch) { live_ = patch; }
    const ModPatch& getLivePatch() const { return live_; }

    // Bit i = track t slot s where i = t * kModSlotsPerTrack + s
    void triggerAdsr (int track, int slot);

    // inputEnv01: 0..1 envelope follower. Updates sources then sums mappings.
    void apply (ParameterState& params, float inputEnv01, int numSamples,
                double beatAtBlockStart, double bpm, double sampleRate);

    // Fills oscilloscope snapshot for hardware LCD (read-only; no extra DSP state).
    void writeModLcdSnapshot (int track, int slot, float inputEnv01, double beatAtEnd,
                            ModLcdSnapshot& out) const;

private:
    float sourceValue (int track, int slot, float inputEnv01, int numSamples,
                       double beatAtBlockStart, double beatAtBlockEnd,
                       float amountOffset, float rateOffset);

    ModPatch live_;

    // Previous block's source value per slot, used so mod-slot-targeting modulation (a slot
    // modulating another slot's Amount/Rate) is well-defined regardless of slot order / feedback.
    std::array<std::array<float, kModSlotsPerTrack>, kNumTracks> prevSrc_ {};

    std::array<std::array<LFO, kModSlotsPerTrack>, kNumTracks>             lfos_ {};
    std::array<std::array<RandomModulator, kModSlotsPerTrack>, kNumTracks> rnds_ {};
    std::array<std::array<AdsrModulator, kModSlotsPerTrack>, kNumTracks>   adsrs_ {};

    std::atomic<uint32_t> pendingAdsrTrig_ { 0 };
};

} // namespace sculpt
