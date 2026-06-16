#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include "GrainPool.h"
#include "../util/GrainPatternTypes.h"
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

        bool  syncedMode      = false; // GrainSync > 0.5
        int   steps           = 8;
        int   pulses          = 4;
        int   rotate          = 0;
        int   pitchQuantIndex = 0; // from GrainPitchQuant
        float loopStart01     = 0.0f;
        float loopEnd01       = 1.0f;

        float grainPattern        = 0.0f; // normalized → map::grainPatternIndex
        float grainPatternAmount  = 0.0f; // 0 = bypass choreography

        float contour  = 0.0f; // 0 = Hann, 1 = percussive (S-4 CONTOUR)
        float randRev  = 0.0f; // per-grain reverse probability 0..1 (S-4 RAND REV)
        float randAmp  = 0.0f; // per-grain amplitude variation 0..1 (S-4 RAND AMP)
    };

    void prepare (double sampleRate);
    void reset();

    void setParams (const Params& params) { params_ = params; }
    void setBlockTiming (const GranularBlockTiming& t) { timing_ = t; }
    void setActive (bool shouldSpawn);

    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

    float getMix() const          { return params_.mix; }
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

    int                         getGrainPatternActiveIndex() const noexcept { return activePatternIndex_; }
    float                       getGrainPatternAmount() const noexcept { return params_.grainPatternAmount; }
    int                         getGrainPatternSequenceStep16() const noexcept { return patternSequenceStep16_; }
    const std::array<float, 16>& getGrainPatternStepLed() const noexcept { return patternStepLed_; }

    /** Test-only: fixed PRNG seed for deterministic renders (default engine seed otherwise). */
    void setRngSeedForTesting (uint32_t seed) noexcept { rng_.seed (seed); }

private:
    void maybeLatchPattern (int64_t patternGridIndex);

    void spawnOneGrain (const SampleBuffer& buffer, int startOffsetInBlock, bool syncedOverlap,
                        float accentMul, int euclidStepForAccent, const GrainPatternStep& patStep,
                        float patternAmount01);

    void spawnGrainFree (const SampleBuffer& buffer, int numSamples);
    double nextSpawnIntervalFree();

    void processSyncedBoundaries (const SampleBuffer& buffer, int numSamples);
    void updateEuclideanMask();
    void updatePatternLedSnapshot();

    GrainPool          pool_;
    Params             params_;
    GranularBlockTiming timing_;
    Random             rng_ { 0xA5A5A5A5u };

    double sampleRate_            = 44100.0;
    double samplesUntilNext_      = 0.0;
    bool   spawning_              = false;
    double lastFiredBoundaryBeat_ = -1.0e30;
    double lastDivBeats_          = -1.0;
    bool   prevSyncedScheduler_   = false;

    uint16_t cachedEuclidMask_   = 0;
    int      cachedEuclidSteps_ = -1;
    int      cachedEuclidPulses_ = -1;
    int      cachedEuclidRotate_ = -1;

    // Display copy (stable for updateScreenModel)
    bool     patternSyncOn_        = false;
    int      patternDivisionIndex_ = 0;
    int      patternSteps_         = 8;
    int      patternPulses_        = 4;
    int      patternRotate_        = 0;
    int      patternCurrentStep_   = 0;
    uint16_t patternMask_          = 0;

    int      activePatternIndex_            = 0;
    int64_t  lastLatchPatternGridIndex_    = std::numeric_limits<int64_t>::min();
    int      patternSequenceStep16_        = 0;
    std::array<float, 16> patternStepLed_ {};
};

} // namespace sculpt
