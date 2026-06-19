#pragma once

namespace sculpt
{

// Maps a sample into slices (Digitakt-style). For now slices are equal divisions of the whole
// buffer; the start/end accessors are the single place playback reads slice bounds, so editable
// (non-uniform) slice markers can be added later without touching call sites. Portable, no JUCE.
constexpr int kMaxSlices = 64;

class SliceMap
{
public:
    void setCount (int n) { count_ = n < 1 ? 1 : (n > kMaxSlices ? kMaxSlices : n); }
    int  count() const    { return count_; }

    // Normalized [0,1] start/end of slice i across the buffer.
    float start01 (int i) const { return static_cast<float> (clampIndex (i)) / static_cast<float> (count_); }
    float end01   (int i) const { return static_cast<float> (clampIndex (i) + 1) / static_cast<float> (count_); }

private:
    int clampIndex (int i) const { return i < 0 ? 0 : (i >= count_ ? count_ - 1 : i); }

    int count_ = 16;
};

} // namespace sculpt
