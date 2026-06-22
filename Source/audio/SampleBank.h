#pragma once

#include <array>
#include <atomic>
#include "SampleBuffer.h"
#include "../util/Constants.h"

namespace sculpt
{

// Project-wide sample pool (Elektron-style). Up to kMaxBankSamples named samples shared by all
// tracks; each track's Sample knob selects an absolute pool slot, and the slot is p-lockable so a
// step can play any sample. Loaded on the message thread only; the audio thread reads buffer
// pointers (no alloc/lock in the RT path). Slots allocate only as samples are loaded.
class SampleBank
{
public:
    static constexpr int kMaxSamples = kMaxBankSamples;

    // Append PCM to the next free pool slot. right may be null (mono → duplicated). rootBpm is the
    // sample's native tempo for warp/sync (see map::estimateSampleRootBpm). Returns the slot index
    // it landed in, or -1 if the pool is full. Message thread only.
    int addSample (const float* left, const float* right, int numFrames, const char* name, float rootBpm);

    // Place PCM into a specific absolute slot (used by state restore). Message thread only.
    void loadSlot (int slot, const float* left, const float* right, int numFrames, const char* name, float rootBpm);

    // Drop every sample (project reload). Message thread only; audio must be suspended.
    void clear();

    // Returns the buffer for a loaded slot, or nullptr if the slot is empty.
    const SampleBuffer* getBuffer (int slot) const;

    const char* getName (int slot) const;
    bool        isLoaded (int slot) const;

    // Per-sample native tempo (warp/sync). RT-safe read; set on the message thread (knob edit).
    float getRootBpm (int slot) const;
    void  setRootBpm (int slot, float bpm);

    // Number of contiguously loaded samples (the highest loaded slot + 1).
    int count() const { return count_; }

    // Writes an Elektron-style slot ID ("A1", "B3", …) for slot into out (>= 8 bytes).
    static void formatId (int slot, char* out, int outSize);

private:
    struct Slot
    {
        SampleBuffer      buffer;
        char              name[64] {};
        std::atomic<float> rootBpm { 120.0f };  // native tempo for warp/sync (RT read, msg-thread write)
        bool              loaded = false;
    };

    std::array<Slot, kMaxSamples> slots_;
    int count_ = 0;
};

} // namespace sculpt
