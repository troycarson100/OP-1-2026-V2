#pragma once

#include <array>
#include "ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

// Owns all normalized parameter values for the instrument.
// Base values come from the host/UI/hardware. Modulation offsets are
// rewritten every block by ModEngine. effective() = clamp01(base + offset).
// Plain arrays, no allocation, no JUCE.
class ParameterState
{
public:
    ParameterState();

    void resetToDefaults();

    // Global parameters (isTrackParameter(id) == false).
    void  setGlobal (ParameterId id, float normalized);
    float getGlobal (ParameterId id) const;

    // Per-track parameters (isTrackParameter(id) == true).
    void  setTrack (int track, ParameterId id, float normalized);
    float getTrack (int track, ParameterId id) const;

    // Modulation offsets, applied additively before clamping.
    void  clearModOffsets();
    void  addModOffset (int track, ParameterId id, float offset);
    void  addModOffsetGlobal (ParameterId id, float offset);

    // Base + modulation, clamped to 0..1.
    float effective (int track, ParameterId id) const;
    float effectiveGlobal (ParameterId id) const;

    // Current block's summed modulation delta (before clamp in effective()).
    float getTrackModOffset (int track, ParameterId id) const;
    float getGlobalModOffset (ParameterId id) const;

private:
    std::array<float, kNumParameters> global_ {};
    std::array<std::array<float, kNumParameters>, kNumTracks> track_ {};
    std::array<std::array<float, kNumParameters>, kNumTracks> modOffset_ {};
    std::array<float, kNumParameters> globalModOffset_ {};
};

} // namespace sculpt
