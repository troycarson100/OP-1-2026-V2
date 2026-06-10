#pragma once

#include <array>
#include "GrainVoice.h"
#include "../util/Constants.h"

namespace sculpt
{

class SampleBuffer;

// Fixed-size pool of grain voices for one track.
// All voices exist up front - nothing is allocated during processing.
class GrainPool
{
public:
    void reset();

    // Returns nullptr if every voice is busy (the grain is simply skipped).
    GrainVoice* findFreeVoice();

    void renderAll (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

    int countActive() const;

private:
    std::array<GrainVoice, kGrainsPerTrack> voices_;
};

} // namespace sculpt
