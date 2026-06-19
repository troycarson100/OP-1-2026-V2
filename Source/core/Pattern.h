#pragma once

#include <array>
#include <cstdint>
#include "../util/Constants.h"

namespace sculpt
{

// One per-step parameter lock: a track ParameterId (stored as its int index) held at `value`
// (normalized 0..1) for that step's trig. Sparse so a pattern stays small.
struct ParamLock
{
    int16_t paramId = -1;   // ParameterId index, -1 = empty slot
    float   value   = 0.0f;
};

// One sequencer step in one track's lane. Portable plain data: a trig plus up to
// kMaxLocksPerStep parameter locks (Digitakt-style).
struct Step
{
    bool    trig     = false;
    uint8_t numLocks = 0;
    std::array<ParamLock, kMaxLocksPerStep> locks {};

    // Find the lock slot for paramId, or -1.
    int findLock (int paramId) const
    {
        for (int i = 0; i < numLocks; ++i)
            if (locks[static_cast<size_t> (i)].paramId == static_cast<int16_t> (paramId))
                return i;
        return -1;
    }

    bool hasLock (int paramId) const { return findLock (paramId) >= 0; }

    bool getLock (int paramId, float& out) const
    {
        const int i = findLock (paramId);
        if (i < 0)
            return false;
        out = locks[static_cast<size_t> (i)].value;
        return true;
    }

    // Insert/update a lock. No-op if full and the param isn't already locked.
    void setLock (int paramId, float value)
    {
        const int i = findLock (paramId);
        if (i >= 0)
        {
            locks[static_cast<size_t> (i)].value = value;
            return;
        }
        if (numLocks >= kMaxLocksPerStep)
            return;
        locks[static_cast<size_t> (numLocks)] = { static_cast<int16_t> (paramId), value };
        ++numLocks;
    }

    void clearLock (int paramId)
    {
        const int i = findLock (paramId);
        if (i < 0)
            return;
        // Compact: move the last lock into the gap.
        locks[static_cast<size_t> (i)] = locks[static_cast<size_t> (numLocks - 1)];
        locks[static_cast<size_t> (numLocks - 1)] = ParamLock {};
        --numLocks;
    }

    void clearLocks() { numLocks = 0; locks = {}; }
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

    // ---- Parameter locks ----
    bool validCell (int track, int step) const
    {
        return track >= 0 && track < kNumTracks && step >= 0 && step < kNumSteps;
    }

    Step* step (int track, int stepIdx) { return validCell (track, stepIdx)
        ? &lanes[static_cast<size_t> (track)][static_cast<size_t> (stepIdx)] : nullptr; }
    const Step* step (int track, int stepIdx) const { return validCell (track, stepIdx)
        ? &lanes[static_cast<size_t> (track)][static_cast<size_t> (stepIdx)] : nullptr; }

    void setLock (int track, int stepIdx, int paramId, float value)
    {
        if (auto* s = step (track, stepIdx)) { s->setLock (paramId, value); used = true; }
    }
    void clearLock (int track, int stepIdx, int paramId)
    {
        if (auto* s = step (track, stepIdx)) s->clearLock (paramId);
    }
    bool hasLock (int track, int stepIdx, int paramId) const
    {
        const auto* s = step (track, stepIdx); return s != nullptr && s->hasLock (paramId);
    }
    bool getLock (int track, int stepIdx, int paramId, float& out) const
    {
        const auto* s = step (track, stepIdx); return s != nullptr && s->getLock (paramId, out);
    }
    int lockCount (int track, int stepIdx) const
    {
        const auto* s = step (track, stepIdx); return s != nullptr ? s->numLocks : 0;
    }
};

} // namespace sculpt
