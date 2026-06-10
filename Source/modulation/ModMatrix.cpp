#include "ModMatrix.h"
#include "../core/ParameterState.h"

namespace sculpt
{

void ModMatrix::reset()
{
    for (auto& slot : slots_)
        slot = Slot {};
}

void ModMatrix::setDefaultRouting()
{
    reset();

    // Gentle per-track LFO drift on grain position.
    for (int t = 0; t < kNumTracks; ++t)
        slots_[static_cast<size_t> (t)] = { true, ModSource::TrackLFO, t, ParameterId::GrainPosition, 0.05f };

    // Macros as broad performance controls across all tracks.
    slots_[4] = { true, ModSource::Macro1, -1, ParameterId::GrainSize,    0.5f };
    slots_[5] = { true, ModSource::Macro2, -1, ParameterId::GrainDensity, 0.5f };
    slots_[6] = { true, ModSource::Macro3, -1, ParameterId::FilterCutoff, -0.6f };
    slots_[7] = { true, ModSource::Macro4, -1, ParameterId::SpaceMix,     0.6f };
}

void ModMatrix::setSlot (int index, const Slot& slot)
{
    if (index >= 0 && index < kModMatrixSlots)
        slots_[static_cast<size_t> (index)] = slot;
}

const ModMatrix::Slot& ModMatrix::getSlot (int index) const
{
    static const Slot empty {};
    return (index >= 0 && index < kModMatrixSlots) ? slots_[static_cast<size_t> (index)] : empty;
}

float ModMatrix::sourceValue (const ModSourceValues& sources, ModSource source, int track)
{
    const int t = (track >= 0 && track < kNumTracks) ? track : 0;

    switch (source)
    {
        case ModSource::TrackLFO:      return sources.lfo[static_cast<size_t> (t)];
        case ModSource::TrackRandom:   return sources.random[static_cast<size_t> (t)];
        case ModSource::InputEnvelope: return sources.inputEnvelope;
        case ModSource::Macro1:        return sources.macros[0];
        case ModSource::Macro2:        return sources.macros[1];
        case ModSource::Macro3:        return sources.macros[2];
        case ModSource::Macro4:        return sources.macros[3];
        default:                       return 0.0f;
    }
}

void ModMatrix::apply (const ModSourceValues& sources, ParameterState& state) const
{
    state.clearModOffsets();

    for (const auto& slot : slots_)
    {
        if (! slot.active || slot.dest == ParameterId::Count || ! isTrackParameter (slot.dest))
            continue;

        if (slot.track >= 0)
        {
            state.addModOffset (slot.track, slot.dest,
                                sourceValue (sources, slot.source, slot.track) * slot.depth);
        }
        else
        {
            for (int t = 0; t < kNumTracks; ++t)
                state.addModOffset (t, slot.dest,
                                    sourceValue (sources, slot.source, t) * slot.depth);
        }
    }
}

} // namespace sculpt
