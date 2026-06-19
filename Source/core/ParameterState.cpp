#include "ParameterState.h"

namespace sculpt
{

ParameterState::ParameterState()
{
    resetToDefaults();
}

void ParameterState::resetToDefaults()
{
    for (int i = 0; i < kNumParameters; ++i)
    {
        const auto id = static_cast<ParameterId> (i);
        global_[static_cast<size_t> (i)] = parameterDefault (id);

        for (int t = 0; t < kNumTracks; ++t)
        {
            track_[static_cast<size_t> (t)][static_cast<size_t> (i)]     = parameterDefault (id);
            modOffset_[static_cast<size_t> (t)][static_cast<size_t> (i)] = 0.0f;
        }

        globalModOffset_[static_cast<size_t> (i)] = 0.0f;
    }
}

void ParameterState::setGlobal (ParameterId id, float normalized)
{
    global_[static_cast<size_t> (id)] = clamp01 (normalized);
}

float ParameterState::getGlobal (ParameterId id) const
{
    return global_[static_cast<size_t> (id)];
}

void ParameterState::setTrack (int track, ParameterId id, float normalized)
{
    if (track < 0 || track >= kNumTracks)
        return;
    track_[static_cast<size_t> (track)][static_cast<size_t> (id)] = clamp01 (normalized);
}

float ParameterState::getTrack (int track, ParameterId id) const
{
    if (track < 0 || track >= kNumTracks)
        return 0.0f;
    return track_[static_cast<size_t> (track)][static_cast<size_t> (id)];
}

void ParameterState::clearModOffsets()
{
    for (auto& trackOffsets : modOffset_)
        trackOffsets.fill (0.0f);
    globalModOffset_.fill (0.0f);
}

void ParameterState::addModOffsetGlobal (ParameterId id, float offset)
{
    globalModOffset_[static_cast<size_t> (id)] += offset;
}

void ParameterState::addModOffset (int track, ParameterId id, float offset)
{
    if (track < 0 || track >= kNumTracks)
        return;
    modOffset_[static_cast<size_t> (track)][static_cast<size_t> (id)] += offset;
}

void ParameterState::setStepOverride (int track, ParameterId id, float value)
{
    if (track < 0 || track >= kNumTracks)
        return;
    const auto t = static_cast<size_t> (track);
    const auto i = static_cast<size_t> (id);
    stepOverride_[t][i]       = clamp01 (value);
    stepOverrideActive_[t][i] = true;
}

void ParameterState::clearStepOverride (int track, ParameterId id)
{
    if (track < 0 || track >= kNumTracks)
        return;
    stepOverrideActive_[static_cast<size_t> (track)][static_cast<size_t> (id)] = false;
}

void ParameterState::clearStepOverrides (int track)
{
    if (track < 0 || track >= kNumTracks)
        return;
    stepOverrideActive_[static_cast<size_t> (track)].fill (false);
}

bool ParameterState::hasStepOverride (int track, ParameterId id) const
{
    if (track < 0 || track >= kNumTracks)
        return false;
    return stepOverrideActive_[static_cast<size_t> (track)][static_cast<size_t> (id)];
}

float ParameterState::effective (int track, ParameterId id) const
{
    if (track < 0 || track >= kNumTracks)
        return 0.0f;
    const auto t = static_cast<size_t> (track);
    const auto i = static_cast<size_t> (id);
    // Step lock (if active) replaces the base value; modulation still adds on top.
    const float base = stepOverrideActive_[t][i] ? stepOverride_[t][i] : track_[t][i];
    return clamp01 (base + modOffset_[t][i]);
}

float ParameterState::effectiveGlobal (ParameterId id) const
{
    const auto i = static_cast<size_t> (id);
    return clamp01 (global_[i] + globalModOffset_[i]);
}

float ParameterState::getTrackModOffset (int track, ParameterId id) const
{
    if (track < 0 || track >= kNumTracks)
        return 0.0f;
    return modOffset_[static_cast<size_t> (track)][static_cast<size_t> (id)];
}

float ParameterState::getGlobalModOffset (ParameterId id) const
{
    return globalModOffset_[static_cast<size_t> (id)];
}

} // namespace sculpt
