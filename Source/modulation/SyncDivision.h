#pragma once

#include <cmath>
#include <cstdint>

namespace sculpt
{

// Musical divisions for mod rate sync (cycles per quarter-note beat).
// Index is stored in ModPatchSlotParams (uint8_t).
constexpr int kNumSyncDivisions = 16;

// cyclesPerBeat: how many full LFO / random-hold cycles fit in one quarter-note beat.
// e.g. 4 = sixteenth notes (4 cycles per beat); 0.25 = one cycle every 4 beats.
inline double syncDivisionCyclesPerBeat (int index)
{
    constexpr double c[kNumSyncDivisions] = {
        16.0, 12.0, 8.0, 6.0, 4.0, 3.0, 2.0, 1.5,
        1.0, 0.75, 0.5, 0.375, 0.25, 0.1875, 0.125, 0.0625
    };
    const int i = (index < 0) ? 0 : (index >= kNumSyncDivisions ? kNumSyncDivisions - 1 : index);
    return c[static_cast<size_t> (i)];
}

inline const char* syncDivisionLabel (int index)
{
    constexpr const char* names[kNumSyncDivisions] = {
        "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T",
        "1/4", "1/4D", "1/2", "1/2D", "1 bar", "1.5 bar", "2 bar", "4 bar"
    };
    const int i = (index < 0) ? 0 : (index >= kNumSyncDivisions ? kNumSyncDivisions - 1 : index);
    return names[static_cast<size_t> (i)];
}

inline double wrapPhase01 (double p)
{
    p -= std::floor (p);
    return p < 0.0 ? p + 1.0 : p;
}

} // namespace sculpt
