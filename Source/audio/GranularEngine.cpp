#include <cmath>
#include <algorithm>
#include <cstdint>

#include "GranularEngine.h"
#include "SampleBuffer.h"
#include "../core/ParameterIds.h"
#include "../util/Constants.h"
#include "../util/Euclidean.h"
#include "../util/GrainPatternBank.h"
#include "../util/GrainPitchScales.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void GranularEngine::setActive (bool shouldSpawn)
{
    if (shouldSpawn && ! spawning_)
    {
        activePatternIndex_        = map::grainPatternIndex (params_.grainPattern);
        lastLatchPatternGridIndex_ = std::numeric_limits<int64_t>::min();
    }
    spawning_ = shouldSpawn;
}

void GranularEngine::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void GranularEngine::reset()
{
    pool_.reset();
    samplesUntilNext_           = 0.0;
    lastFiredBoundaryBeat_      = -1.0e30;
    lastDivBeats_               = -1.0;
    prevSyncedScheduler_        = false;
    cachedEuclidMask_           = 0;
    cachedEuclidSteps_          = -1;
    cachedEuclidPulses_         = -1;
    cachedEuclidRotate_         = -1;
    patternSyncOn_              = false;
    patternDivisionIndex_       = 0;
    patternSteps_               = 8;
    patternPulses_              = 4;
    patternRotate_              = 0;
    patternCurrentStep_         = 0;
    patternMask_                = 0;
    activePatternIndex_         = 0;
    lastLatchPatternGridIndex_  = std::numeric_limits<int64_t>::min();
    patternSequenceStep16_      = 0;
    patternStepLed_.fill (0.0f);
}

void GranularEngine::updateEuclideanMask()
{
    const int s = std::clamp (params_.steps, 1, 16);
    const int p = std::clamp (params_.pulses, 1, s);
    const int r = std::clamp (params_.rotate, 0, std::max (1, s) - 1);
    if (s != cachedEuclidSteps_ || p != cachedEuclidPulses_ || r != cachedEuclidRotate_)
    {
        cachedEuclidSteps_  = s;
        cachedEuclidPulses_ = p;
        cachedEuclidRotate_ = r;
        cachedEuclidMask_   = euclideanRhythm (s, p, r);
    }
}

void GranularEngine::maybeLatchPattern (int64_t patternGridIndex)
{
    if (patternGridIndex != lastLatchPatternGridIndex_)
    {
        lastLatchPatternGridIndex_ = patternGridIndex;
        activePatternIndex_        = map::grainPatternIndex (params_.grainPattern);
    }
}

void GranularEngine::updatePatternLedSnapshot()
{
    const float amt = map::grainPatternDepth01 (params_.grainPatternAmount);
    for (int s = 0; s < 16; ++s)
    {
        const auto& st = grainPatternStep (activePatternIndex_, s);
        const float db = map::grainPatternGainDbEffective (st.gainDb, amt);
        patternStepLed_[static_cast<size_t> (s)] = clamp01 ((db + 12.0f) / 15.0f);
    }
}

double GranularEngine::nextSpawnIntervalFree()
{
    const double densityHz = static_cast<double> (map::grainDensityHz (params_.density));
    double       interval  = sampleRate_ / densityHz;
    interval *= 1.0 + static_cast<double> (params_.texture) * 0.75 * static_cast<double> (rng_.nextBipolar());
    return interval > 10.0 ? interval : 10.0;
}

void GranularEngine::spawnOneGrain (const SampleBuffer& buffer, int startOffsetInBlock, bool syncedOverlap,
                                    float accentMul, int euclidStepForAccent, const GrainPatternStep& patStep,
                                    float patternAmount01)
{
    GrainVoice* voice = pool_.findFreeVoice();
    if (voice == nullptr)
        return;

    const float amt = map::grainPatternDepth01 (patternAmount01);

    float   posOff   = 0.0f;
    int     pitchAdd = 0;
    float   sizeSc   = 1.0f;
    float   gainDbP  = 0.0f;

    if (amt > 1.0e-8f)
    {
        posOff   = patStep.posOffset * amt;
        pitchAdd = static_cast<int> (std::lround (static_cast<float> (patStep.pitchSemis) * amt));
        sizeSc   = lerp (1.0f, patStep.sizeScale, amt);
        gainDbP  = patStep.gainDb * amt;
    }

    const float frames = static_cast<float> (buffer.getNumFrames ());

    const float sizeSecondsBase = map::grainSizeSeconds (params_.size);
    const float sizeSeconds     = sizeSecondsBase * sizeSc;
    const int   lengthSamples   = std::max (8, static_cast<int> (sizeSeconds * static_cast<float> (sampleRate_)));

    const float loopA = clamp01 (params_.loopStart01);
    const float loopB = clamp01 (params_.loopEnd01);
    const float ls    = loopA * (frames - 1.0f);
    const float le    = loopB * (frames - 1.0f);
    const float loopLenFrames = std::max (1.0f, le - ls);

    // Base spawn position. With GrainFollow, blend the knob toward the moving tape playhead so the
    // cloud tracks the material loop's rhythm (S-4-style); in synced mode the playhead is converted
    // to loop-relative space. The pattern choreography offset (posOff) is then layered on top.
    float basePos = clamp01 (params_.position);
    if (params_.follow > 1.0e-5f)
    {
        float ph;
        if (params_.syncedMode)
        {
            const float span = std::max (1.0e-6f, loopB - loopA);
            ph = clamp01 ((clamp01 (params_.playhead01) - loopA) / span);
        }
        else
            ph = clamp01 (params_.playhead01);
        basePos = lerp (basePos, ph, clamp01 (params_.follow));
    }

    float posNorm = basePos;
    {
        float w = posNorm + posOff;
        w       = w - std::floor (w);
        posNorm = clamp01 (w);
    }

    float startFrame = 0.0f;

    if (params_.syncedMode)
    {
        const float base = ls + posNorm * loopLenFrames;
        if (params_.spray <= 1.0e-5f)
            startFrame = base;
        else
        {
            const int steps = std::clamp (params_.steps, 1, 16);
            int       si;
            if (params_.follow > 1.0e-5f)
            {
                // Following: scatter on the step grid AROUND the followed position (window = Spray),
                // so grains stay near the playhead instead of jumping across the whole loop.
                const int center = std::clamp (static_cast<int> (std::floor (posNorm * static_cast<float> (steps))), 0, steps - 1);
                const int range  = std::max (1, static_cast<int> (std::lround (params_.spray * static_cast<float> (steps))));
                const int off    = static_cast<int> (rng_.nextUInt () % static_cast<uint32_t> (2 * range + 1)) - range;
                si = std::clamp (center + off, 0, steps - 1);
            }
            else
                si = static_cast<int> (rng_.nextUInt () % static_cast<uint32_t> (steps));
            startFrame = ls + (static_cast<float> (si) + 0.5f) / static_cast<float> (steps) * loopLenFrames;
        }
    }
    else
    {
        startFrame = posNorm * (frames - 1.0f)
                     + params_.spray * 0.5f * frames * rng_.nextBipolar ();
    }

    if (frames > 0.0f)
    {
        startFrame = std::fmod (startFrame, frames);
        if (startFrame < 0.0f)
            startFrame += frames;
    }

    float semis = (params_.pitch - 0.5f) * 12.0f + static_cast<float> (pitchAdd);
    // Quantize to the Material page's scale + key so the granular layer stays in tune with the
    // tape. Falls back to the standalone GrainPitchQuant scale only when Material scale is Free.
    if (params_.pitchScale != FilterScale::Free)
    {
        const int rootMidi = 60 + (((params_.pitchKey % 12) + 12) % 12);
        semis = snapToScaleMidi (60.0f + semis, rootMidi, params_.pitchScale) - 60.0f;
    }
    else if (params_.pitchQuantIndex > 0)
        semis = quantizeGrainPitchSemitones (semis, params_.pitchQuantIndex);

    // Ride the Material page transpose, applied after the snap so grains move locked to the tape.
    semis += params_.materialSemis;

    float increment = semitonesToRatio (semis)
                      * (1.0f + params_.texture * 0.04f * rng_.nextBipolar ());

    float gainL = 0.0f, gainR = 0.0f;
    equalPowerPan (params_.spread * rng_.nextBipolar (), gainL, gainR);

    float overlapIdeal = 0.0f;
    if (syncedOverlap)
    {
        const double divBeats = map::grainRateDivisionBeats (params_.density);
        const double bpm        = timing_.bpm > 1.0e-6 ? timing_.bpm : 120.0;
        const double divSec   = divBeats * (60.0 / bpm);
        if (divSec > 1.0e-9)
            overlapIdeal = static_cast<float> (sizeSeconds / divSec);
    }
    else
        overlapIdeal = map::grainDensityHz (params_.density) * sizeSeconds;

    const float overlapForGain = std::min (overlapIdeal, static_cast<float> (kGrainsPerTrack));
    constexpr float kOverlapCompExponent = 0.36f;
    constexpr float kGrainLaunchGain     = 1.0f;
    const float     denom                = std::pow (std::max (1.0f, overlapForGain), kOverlapCompExponent);
    float           amp                  = kGrainLaunchGain / denom;

    int firstPulseStep = -1;
    if (params_.syncedMode && euclidStepForAccent >= 0)
    {
        const int steps = std::clamp (params_.steps, 1, 16);
        for (int i = 0; i < steps; ++i)
        {
            if ((cachedEuclidMask_ >> i) & 1)
            {
                firstPulseStep = i;
                break;
            }
        }
        if (firstPulseStep >= 0 && euclidStepForAccent == firstPulseStep)
            amp *= dbToGain (2.0f);
    }

    amp *= accentMul;
    amp *= dbToGain (gainDbP);

    // RandAmp: per-grain amplitude variation (S-4 RAND AMP).
    if (params_.randAmp > 1.0e-5f)
        amp *= std::max (0.05f, 1.0f - params_.randAmp * 0.75f * rng_.nextFloat());

    GrainVoice::StartParams sp;
    sp.startFrame         = startFrame;
    sp.increment          = increment;
    sp.lengthSamples      = lengthSamples;
    sp.gainL              = gainL * amp;
    sp.gainR              = gainR * amp;
    sp.startOffsetSamples = std::max (0, startOffsetInBlock);
    sp.contour            = params_.contour;

    // RandRev: probabilistic grain reversal (S-4 RAND REV). Reversed grains start at
    // the far end of their window and walk backward; getSampleLinear wraps at buffer bounds.
    if (params_.randRev > 1.0e-5f && rng_.nextFloat() < params_.randRev)
    {
        float revStart = sp.startFrame + static_cast<float> (sp.lengthSamples - 1) * std::fabs (sp.increment);
        if (frames > 0.0f)
        {
            revStart = std::fmod (revStart, frames);
            if (revStart < 0.0f) revStart += frames;
        }
        sp.startFrame = revStart;
        sp.increment  = -std::fabs (sp.increment);
    }

    voice->start (sp);
}

void GranularEngine::spawnGrainFree (const SampleBuffer& buffer, int numSamples)
{
    const double divBeats = map::grainRateDivisionBeats (params_.density);
    const double spb      = timing_.samplesPerBeat;
    const double beat0    = timing_.beatAtBlockStart;

    while (samplesUntilNext_ <= 0.0)
    {
        const double interval = nextSpawnIntervalFree ();
        const double beatSpawn =
            beat0 + (static_cast<double> (numSamples) + samplesUntilNext_) / spb;
        const int64_t g = (divBeats > 1.0e-12 && spb > 1.0e-12 && std::isfinite (divBeats) && std::isfinite (spb))
                              ? static_cast<int64_t> (std::floor (beatSpawn / divBeats + 1.0e-12))
                              : 0LL;
        maybeLatchPattern (g);
        const int step16 = static_cast<int> (((g % 16) + 16) % 16);
        const auto& st   = grainPatternStep (activePatternIndex_, step16);

        int sampleOffset = static_cast<int> (std::lround ((beatSpawn - beat0) * spb));
        sampleOffset     = std::clamp (sampleOffset, 0, std::max (0, numSamples - 1));

        uint8_t rat = st.ratchet;
        if (rat < 1)
            rat = 1;
        if (rat > 4)
            rat = 4;
        if (params_.grainPatternAmount <= 1.0e-8f)
            rat = 1;

        const int divSamples = (divBeats > 1.0e-12 && spb > 1.0e-12)
                                   ? std::max (1, static_cast<int> (std::lround (divBeats * spb)))
                                   : std::max (1, numSamples);
        const int stride   = std::max (1, divSamples / static_cast<int> (rat));

        const int stepsE = std::clamp (params_.steps, 1, 16);
        const int acc    = static_cast<int> (((g % stepsE) + stepsE) % stepsE);

        for (int k = 0; k < static_cast<int> (rat); ++k)
        {
            const int off = std::clamp (sampleOffset + k * stride, 0, std::max (0, numSamples - 1));
            spawnOneGrain (buffer, off, false, 1.0f, acc, st, params_.grainPatternAmount);
        }

        samplesUntilNext_ += interval;
    }
}

void GranularEngine::processSyncedBoundaries (const SampleBuffer& buffer, int numSamples)
{
    const double divBeats = map::grainRateDivisionBeats (params_.density);
    const double spb      = timing_.samplesPerBeat;
    const double beat0    = timing_.beatAtBlockStart;

    if (divBeats <= 1.0e-12 || spb <= 1.0e-12 || ! std::isfinite (divBeats) || ! std::isfinite (spb))
        return;

    if (params_.syncedMode != prevSyncedScheduler_
        || std::fabs (divBeats - lastDivBeats_) > 1.0e-9)
        lastFiredBoundaryBeat_ = -1.0e30;
    lastDivBeats_ = divBeats;

    const double beat1 = beat0 + static_cast<double> (numSamples) / spb;

    const int steps = std::clamp (params_.steps, 1, 16);
    updateEuclideanMask ();

    const int64_t nMin = static_cast<int64_t> (std::ceil (beat0 / divBeats - 1.0e-12));
    const int64_t nMax = static_cast<int64_t> (std::floor (beat1 / divBeats + 1.0e-12));

    for (int64_t n = nMin; n <= nMax; ++n)
    {
        const double B = static_cast<double> (n) * divBeats;
        if (B <= lastFiredBoundaryBeat_ + 1.0e-9)
            continue;
        if (B < beat0 - 1.0e-9 || B > beat1 + 1.0e-9)
            continue;

        maybeLatchPattern (n);
        const int step16 = static_cast<int> (((n % 16) + 16) % 16);
        patternSequenceStep16_ = step16;

        int stepIndex = 0;
        if (steps > 0)
        {
            const int64_t si = static_cast<int64_t> (std::floor (B / divBeats + 1.0e-12));
            stepIndex          = static_cast<int> (((si % steps) + steps) % steps);
        }

        patternCurrentStep_ = stepIndex;

        if (((cachedEuclidMask_ >> stepIndex) & 1) == 0)
        {
            lastFiredBoundaryBeat_ = B;
            continue;
        }

        // Boundary time in samples (double) so ratchet bursts divide the period evenly;
        // integer stride (divSamples / rat) truncates and drifts against triplets / host BPM.
        const double beatRel    = B - beat0;
        double       tHitSample = beatRel * spb;

        const float hum = map::grainSyncHumanizeMaxSamples (sampleRate_);
        const float humAmt = 0.18f + 0.82f * params_.texture;
        tHitSample += static_cast<double> (humAmt * rng_.nextBipolar () * hum);

        const auto& st = grainPatternStep (activePatternIndex_, step16);

        uint8_t rat = st.ratchet;
        if (rat < 1)
            rat = 1;
        if (rat > 4)
            rat = 4;
        if (params_.grainPatternAmount <= 1.0e-8f)
            rat = 1;

        const double divSamplesD = divBeats * spb;
        const int    ratI        = static_cast<int> (rat);

        const float accent = 1.0f;
        for (int k = 0; k < ratI; ++k)
        {
            const double tk =
                tHitSample + static_cast<double> (k) * divSamplesD / static_cast<double> (ratI);
            int off = static_cast<int> (std::lround (tk));
            off       = std::clamp (off, 0, std::max (0, numSamples - 1));
            spawnOneGrain (buffer, off, true, accent, stepIndex, st, params_.grainPatternAmount);
        }

        lastFiredBoundaryBeat_ = B;
    }

    if (steps > 0)
    {
        const int64_t siEnd = static_cast<int64_t> (std::floor (beat1 / divBeats + 1.0e-12));
        patternCurrentStep_ = static_cast<int> (((siEnd % steps) + steps) % steps);
        patternSequenceStep16_ =
            static_cast<int> (((siEnd % 16) + 16) % 16);
    }
}

void GranularEngine::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    if (buffer.getNumFrames () < 2)
    {
        prevSyncedScheduler_ = params_.syncedMode;
        return;
    }

    updateEuclideanMask ();

    patternSyncOn_        = params_.syncedMode;
    patternDivisionIndex_ = map::grainRateDivisionTableIndex (params_.density);
    patternSteps_         = std::clamp (params_.steps, 1, 16);
    patternPulses_        = std::clamp (params_.pulses, 1, patternSteps_);
    patternRotate_        = std::clamp (params_.rotate, 0, std::max (1, patternSteps_) - 1);
    patternMask_          = cachedEuclidMask_;

    if (spawning_)
    {
        if (params_.syncedMode)
            processSyncedBoundaries (buffer, numSamples);
        else
        {
            if (prevSyncedScheduler_ && ! params_.syncedMode)
                samplesUntilNext_ = 0.0;
            samplesUntilNext_ -= static_cast<double> (numSamples);
            while (samplesUntilNext_ <= 0.0)
                spawnGrainFree (buffer, numSamples);
        }
    }

    pool_.renderAll (buffer, outL, outR, numSamples);

    updatePatternLedSnapshot ();

    prevSyncedScheduler_ = params_.syncedMode;
}

void GranularEngine::fillGrainDisplay (const SampleBuffer& material, GrainDisplaySlot* out, int maxSlots) const
{
    if (out == nullptr || maxSlots <= 0)
        return;

    const float tf = static_cast<float> (std::max (1, material.getNumFrames ()));
    pool_.fillGrainDisplay (tf, out, maxSlots);
}

void GranularEngine::getGrainFocusWindow01 (float totalFrames, float& outStart01, float& outLen01) const noexcept
{
    if (totalFrames < 2.0f)
    {
        outStart01 = 0.0f;
        outLen01   = 0.0f;
        return;
    }

    const float tf         = totalFrames;
    float       basePos    = clamp01 (params_.position);
    if (params_.follow > 1.0e-5f) // overlay box tracks the playhead when following
        basePos = lerp (basePos, clamp01 (params_.playhead01), clamp01 (params_.follow));
    const float startFrame = basePos * (tf - 1.0f);
    const float sizeSec    = map::grainSizeSeconds (params_.size);
    const float lenFrames  = sizeSec * static_cast<float> (sampleRate_);

    outStart01 = std::clamp (startFrame / tf, 0.0f, 1.0f);
    outLen01   = std::clamp (lenFrames / tf, 0.0f, 1.0f);
}

} // namespace sculpt
