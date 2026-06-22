#include "SampleBank.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace sculpt
{

void SampleBank::loadSlot (int slot, const float* left, const float* right, int numFrames, const char* name, float rootBpm)
{
    if (slot < 0 || slot >= kMaxSamples || numFrames < 1 || left == nullptr)
        return;

    auto& s = slots_[static_cast<size_t> (slot)];
    s.buffer.resize (2, numFrames);
    float* L = s.buffer.getChannelData (0);
    float* R = s.buffer.getChannelData (1);
    for (int i = 0; i < numFrames; ++i)
    {
        L[i] = left[i];
        R[i] = right ? right[i] : left[i];
    }
    std::strncpy (s.name, name ? name : "", 63);
    s.name[63] = '\0';
    s.rootBpm.store (rootBpm, std::memory_order_relaxed);
    s.loaded    = true;

    if (slot + 1 > count_)
        count_ = slot + 1;
}

void SampleBank::clear()
{
    for (auto& s : slots_)
    {
        s.buffer.resize (0, 0);
        s.name[0] = '\0';
        s.rootBpm.store (120.0f, std::memory_order_relaxed);
        s.loaded = false;
    }
    count_ = 0;
}

int SampleBank::addSample (const float* left, const float* right, int numFrames, const char* name, float rootBpm)
{
    if (count_ >= kMaxSamples)
        return -1;
    const int slot = count_;
    loadSlot (slot, left, right, numFrames, name, rootBpm);
    return slots_[static_cast<size_t> (slot)].loaded ? slot : -1;
}

float SampleBank::getRootBpm (int slot) const
{
    if (slot < 0 || slot >= kMaxSamples)
        return 120.0f;
    return slots_[static_cast<size_t> (slot)].rootBpm.load (std::memory_order_relaxed);
}

void SampleBank::setRootBpm (int slot, float bpm)
{
    if (slot < 0 || slot >= kMaxSamples)
        return;
    slots_[static_cast<size_t> (slot)].rootBpm.store (bpm, std::memory_order_relaxed);
}

const SampleBuffer* SampleBank::getBuffer (int slot) const
{
    if (slot < 0 || slot >= kMaxSamples)
        return nullptr;
    const auto& s = slots_[static_cast<size_t> (slot)];
    return s.loaded ? &s.buffer : nullptr;
}

const char* SampleBank::getName (int slot) const
{
    if (slot < 0 || slot >= kMaxSamples)
        return "";
    return slots_[static_cast<size_t> (slot)].name;
}

bool SampleBank::isLoaded (int slot) const
{
    if (slot < 0 || slot >= kMaxSamples)
        return false;
    return slots_[static_cast<size_t> (slot)].loaded;
}

void SampleBank::formatId (int slot, char* out, int outSize)
{
    if (out == nullptr || outSize < 1)
        return;
    if (slot < 0)
    {
        std::snprintf (out, static_cast<size_t> (outSize), "--");
        return;
    }
    const char letter = static_cast<char> ('A' + (slot / 16));
    const int  number = (slot % 16) + 1;
    std::snprintf (out, static_cast<size_t> (outSize), "%c%d", letter, number);
}

} // namespace sculpt
