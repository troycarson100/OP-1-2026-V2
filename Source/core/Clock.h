#pragma once

#include <cstdint>

namespace sculpt
{

// Sample-accurate musical clock. Host tempo may be fed in, but all timing
// math lives here so it ports directly to hardware.
class Clock
{
public:
    void prepare (double sampleRate);
    void reset();

    void setBpm (double bpm);
    double getBpm() const            { return bpm_; }

    // Advance by a block. Call once per processed chunk.
    void advance (int numSamples);

    uint64_t getSamplePosition() const { return samplePosition_; }
    double   getBeatPosition() const   { return beatPosition_; }

    // Phase within the current beat, 0..1.
    double   getBeatPhase() const;

private:
    double   sampleRate_     = 44100.0;
    double   bpm_            = 120.0;
    uint64_t samplePosition_ = 0;
    double   beatPosition_   = 0.0;
};

} // namespace sculpt
