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

    // Per-step parameter locks (sequencer): an active override replaces the base value for a track
    // parameter (modulation still adds on top). The sequencer sets these at a step's trig and clears
    // them at the next trig, so a locked value applies for that step. Audio-thread only.
    void  setStepOverride (int track, ParameterId id, float value);
    void  clearStepOverride (int track, ParameterId id);
    void  clearStepOverrides (int track);
    bool  hasStepOverride (int track, ParameterId id) const;

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

    // Per-step lock override layer (see setStepOverride).
    std::array<std::array<float, kNumParameters>, kNumTracks> stepOverride_ {};
    std::array<std::array<bool,  kNumParameters>, kNumTracks> stepOverrideActive_ {};
};

} // namespace sculpt
