#pragma once

#include <array>
#include "../core/ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

class ParameterState;

// Modulation sources addressable by matrix slots.
enum class ModSource : int
{
    None = 0,
    TrackLFO,        // per-track LFO (uses the slot's track index)
    TrackRandom,     // per-track random source
    InputEnvelope,   // global input envelope follower
    Macro1,
    Macro2,
    Macro3,
    Macro4,
};

// Snapshot of all source values for one block. Built by the Engine,
// consumed by the ModMatrix. Plain data, no allocation.
struct ModSourceValues
{
    std::array<float, kNumTracks> lfo {};      // bipolar
    std::array<float, kNumTracks> random {};   // bipolar
    float inputEnvelope = 0.0f;                // unipolar
    std::array<float, kNumMacros> macros {};   // unipolar
};

// Fixed-size routing matrix: source -> per-track ParameterId destination
// with depth. apply() rewrites the ParameterState mod offsets each block.
class ModMatrix
{
public:
    struct Slot
    {
        bool        active = false;
        ModSource   source = ModSource::None;
        int         track  = -1;            // -1 = all tracks
        ParameterId dest   = ParameterId::Count;
        float       depth  = 0.0f;          // -1..1, scales the source value
    };

    void reset();
    void setDefaultRouting();

    void setSlot (int index, const Slot& slot);
    const Slot& getSlot (int index) const;

    void apply (const ModSourceValues& sources, ParameterState& state) const;

private:
    static float sourceValue (const ModSourceValues& sources, ModSource source, int track);

    std::array<Slot, kModMatrixSlots> slots_ {};
};

} // namespace sculpt
