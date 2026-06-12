#pragma once

#include "GrainPool.h"
#include "../util/Random.h"

namespace sculpt
{

class SampleBuffer;

// Granular stage for one track. Spawns grains from a fixed pool at a
// density-driven rate and renders the wet grain cloud.
// All parameters are normalized 0..1; real-unit mapping uses sculpt::map.
class GranularEngine
{
public:
    struct Params
    {
        float position = 0.3f;
        float size     = 0.4f;
        float density  = 0.45f;
        float pitch    = 0.5f;
        float spray    = 0.2f;
        float texture  = 0.2f;
        float spread   = 0.5f;
        float mix      = 0.5f;
    };

    void prepare (double sampleRate);
    void reset();

    void setParams (const Params& params) { params_ = params; }
    void setActive (bool shouldSpawn)     { spawning_ = shouldSpawn; }

    // Adds the grain cloud into outL/outR (wet only - the Track blends mix).
    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

    float getMix() const       { return params_.mix; }
    int   getActiveGrains() const { return pool_.countActive(); }

    void fillGrainDisplay (const SampleBuffer& material, GrainDisplaySlot* out, int maxSlots) const;
    void getGrainFocusWindow01 (float totalFrames, float& outStart01, float& outLen01) const noexcept;

private:
    void spawnGrain (const SampleBuffer& buffer);
    double nextSpawnInterval();

    GrainPool pool_;
    Params    params_;
    Random    rng_ { 0xA5A5A5A5u };

    double sampleRate_       = 44100.0;
    double samplesUntilNext_ = 0.0;
    bool   spawning_         = false;
};

} // namespace sculpt
