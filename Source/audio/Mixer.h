#pragma once

#include <array>
#include "../util/Constants.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

// Final mixer: per-track level/pan plus smoothed global output gain.
// Sums the four track buses into the stereo output.
class Mixer
{
public:
    void prepare (double sampleRate);

    // Normalized values; mapping to gain/pan happens here only via sculpt::map.
    void setTrackLevel (int track, float level01);
    void setTrackPan (int track, float pan01);
    void setOutputGain (float gain01);

    // trackL/trackR are arrays of kNumTracks channel pointers.
    // Overwrites outL/outR.
    void process (const float* const* trackL, const float* const* trackR,
                  float* outL, float* outR, int numSamples);

private:
    std::array<SmoothedValue, kNumTracks> levels_;
    std::array<SmoothedValue, kNumTracks> pans_;
    SmoothedValue outputGain_;
};

} // namespace sculpt
