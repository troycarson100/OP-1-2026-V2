#pragma once

#include <vector>

namespace sculpt
{

// Portable delay primitives for the plate reverb. No JUCE. Allocation only in
// prepare(); process()/read()/write() are real-time safe.

// Fractional-read delay line. process() returns the input delayed by the current
// delay length; readAt() taps the buffer at an arbitrary distance behind the head
// (used for the multi-tap reverb output) without changing state.
class DelayLine
{
public:
    void prepare (int maxLenSamples);
    void reset();

    void setDelaySamples (float d);

    float process (float x);                 // y = x delayed by delay_
    float readAt (float samplesBehind) const; // interpolated tap (read-only)
    void  write (float x);                    // raw push (advances head)

    int capacity() const { return size_; }

private:
    float readFrac (float samplesBehind) const;

    std::vector<float> buf_;
    int   size_  = 1;
    int   write_ = 0;
    float delay_ = 1.0f;
};

// Schroeder all-pass: AP(z) = (g + z^-k) / (1 + g z^-k), with optional fractional
// delay modulation for the reverb tank diffusers. g may be negative.
class AllpassDelay
{
public:
    void prepare (int maxLenSamples);
    void reset();

    void setDelaySamples (float d);
    void setCoeff (float g) { g_ = g; }

    float process (float x)               { return processMod (x, 0.0f); }
    float processMod (float x, float modSamples);
    float readAt (float samplesBehind) const; // tap the internal delay memory

private:
    float readFrac (float samplesBehind) const;

    std::vector<float> buf_;
    int   size_  = 1;
    int   write_ = 0;
    float delay_ = 1.0f;
    float g_     = 0.5f;
};

} // namespace sculpt
