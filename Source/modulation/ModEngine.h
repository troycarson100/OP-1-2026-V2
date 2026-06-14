#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "AdsrModulator.h"
#include "LFO.h"
#include "ModTypes.h"
#include "RandomModulator.h"
#include "../core/ParameterState.h"
#include "../util/Constants.h"

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

    // inputEnvMain01 / inputEnvSide01: 0..1 envelope followers (main input vs host sidechain).
    // trackInputEnv01: per-track bus envelope from previous audio block (see Engine).
    void apply (ParameterState& params, float inputEnvMain01, float inputEnvSide01,
                const std::array<float, kNumTracks>& trackInputEnv01, int numSamples,
                double beatAtBlockStart, double bpm, double sampleRate);

    // Fills oscilloscope snapshot for hardware LCD (read-only; no extra DSP state).
    // overlayPeaks01: optional peak waveform for InputEnvelope (same bin count as carrier); nullptr skips.
    void writeModLcdSnapshot (int track, int slot, float inputEnvMain01, float inputEnvSide01,
                              const std::array<float, kNumTracks>& trackInputEnv01,
                              const std::array<float, kMaterialWaveformBins>* overlayPeaks01,
                              double beatAtEnd, ModLcdSnapshot& out) const;

private:
    float sourceValue (int track, int slot, float inputEnvMain01, float inputEnvSide01,
                       const std::array<float, kNumTracks>& trackInputEnv01, int numSamples,
                       double beatAtBlockStart, double beatAtBlockEnd);

    ModPatch live_;

    std::array<std::array<LFO, kModSlotsPerTrack>, kNumTracks>             lfos_ {};
    std::array<std::array<RandomModulator, kModSlotsPerTrack>, kNumTracks> rnds_ {};
    std::array<std::array<AdsrModulator, kModSlotsPerTrack>, kNumTracks>   adsrs_ {};

    std::atomic<uint32_t> pendingAdsrTrig_ { 0 };
};

} // namespace sculpt
