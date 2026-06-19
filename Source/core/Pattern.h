#pragma once

#include <array>
#include "../util/Constants.h"

namespace sculpt
{

// One sequencer step in one track's lane. Portable plain data.
// Phase 4 will extend this with parameter locks (a small fixed sparse array);
// keeping it a struct now means that addition needs no call-site churn.
struct Step
{
    bool trig = false;
};

// A sequencer pattern: per-track lanes of steps + trigs. This is core product
// data (like Scene), stored in portable C++; serialization lives in the wrapper.
// Distinct from Scene: a Scene snapshots parameter values, a Pattern holds the
// step grid (trigs, and later p-locks).
struct Pattern
{
    bool used = false;
    int  length = kNumSteps;   // active step count 1..kNumSteps (shared across tracks for now)

    std::array<std::array<Step, kNumSteps>, kNumTracks> lanes {};

    void clear()
    {
        used   = false;
        length = kNumSteps;
        for (auto& lane : lanes)
            for (auto& s : lane)
                s = Step {};
    }

    bool hasTrig (int track, int step) const
    {
        if (track < 0 || track >= kNumTracks || step < 0 || step >= kNumSteps)
            return false;
        return lanes[static_cast<size_t> (track)][static_cast<size_t> (step)].trig;
    }

    void setTrig (int track, int step, bool on)
    {
        if (track < 0 || track >= kNumTracks || step < 0 || step >= kNumSteps)
            return;
        lanes[static_cast<size_t> (track)][static_cast<size_t> (step)].trig = on;
        if (on)
            used = true;
    }

    void toggleTrig (int track, int step)
    {
        if (track < 0 || track >= kNumTracks || step < 0 || step >= kNumSteps)
            return;
        const bool now = ! lanes[static_cast<size_t> (track)][static_cast<size_t> (step)].trig;
        setTrig (track, step, now);
    }
};

} // namespace sculpt
