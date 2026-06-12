#pragma once

#include <cstdint>

#include "GrainPool.h"
#include "../util/Random.h"

namespace sculpt
{

class SampleBuffer;

// Per-chunk host timing for granular (beat grid). Portable POD.
struct GranularBlockTiming
{
    double beatAtBlockStart = 0.0;   // Clock beat at first sample of this chunk
    double samplesPerBeat   = 0.0;   // sampleRate * 60 / bpm
    double bpm              = 120.0; // effective BPM for overlap math
    bool   hostPlaying      = true;  // reserved for future transport gating
};

// Granular stage for one track. Spawns grains from a fixed pool at a
// density-driven rate (free) or on a musical grid (synced).
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

        bool  syncedMode     = false; // GrainSync > 0.5
        int   steps          = 8;
        int   pulses         = 4;
        int   rotate         = 0;
        int   pitchQuantIndex = 0; // from GrainPitchQuant
        float loopStart01    = 0.0f;
        float loopEnd01      = 1.0f;
    };

    void prepare (double sampleRate);
    void reset();

    void setParams (const Params& params) { params_ = params; }
    void setBlockTiming (const GranularBlockTiming& t) { timing_ = t; }
    void setActive (bool shouldSpawn) { spawning_ = shouldSpawn; }

    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

    float getMix() const       { return params_.mix; }
    int   getActiveGrains() const { return pool_.countActive(); }

    void fillGrainDisplay (const SampleBuffer& material, GrainDisplaySlot* out, int maxSlots) const;
    void getGrainFocusWindow01 (float totalFrames, float& outStart01, float& outLen01) const noexcept;

    // Pattern snapshot for ScreenModel (written during process; read on same thread after process).
    bool     getPatternSyncOn() const noexcept { return patternSyncOn_; }
    int      getPatternDivisionIndex() const noexcept { return patternDivisionIndex_; }
    int      getPatternSteps() const noexcept { return patternSteps_; }
    int      getPatternPulses() const noexcept { return patternPulses_; }
    int      getPatternRotate() const noexcept { return patternRotate_; }
    int      getPatternCurrentStep() const noexcept { return patternCurrentStep_; }
    uint16_t getPatternMask() const noexcept { return patternMask_; }

private:
    void spawnGrainFree (const SampleBuffer& buffer);
    void spawnGrainAt (const SampleBuffer& buffer, int startOffsetInBlock, bool syncedOverlap,
                        float accentMul, int stepIndexForAccent);
    double nextSpawnIntervalFree();

    void processSyncedBoundaries (const SampleBuffer& buffer, int numSamples);
    void updateEuclideanMask();

    GrainPool pool_;
    Params             params_;
    GranularBlockTiming timing_;
    Random             rng_ { 0xA5A5A5A5u };

    double sampleRate_            = 44100.0;
    double samplesUntilNext_      = 0.0;
    bool   spawning_              = false;
    double lastFiredBoundaryBeat_ = -1.0e30;
    double lastDivBeats_          = -1.0;
    bool   prevSyncedScheduler_   = false;

    uint16_t cachedEuclidMask_ = 0;
    int      cachedEuclidSteps_ = -1;
    int      cachedEuclidPulses_ = -1;
    int      cachedEuclidRotate_ = -1;

    // Display copy (stable for updateScreenModel)
    bool     patternSyncOn_         = false;
    int      patternDivisionIndex_  = 0;
    int      patternSteps_          = 8;
    int      patternPulses_         = 4;
    int      patternRotate_         = 0;
    int      patternCurrentStep_    = 0;
    uint16_t patternMask_           = 0;
};

} // namespace sculpt
