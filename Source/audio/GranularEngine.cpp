#include <cmath>
#include <algorithm>
#include "GranularEngine.h"
#include "SampleBuffer.h"
#include "../core/ParameterIds.h"
#include "../util/Constants.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void GranularEngine::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void GranularEngine::reset()
{
    pool_.reset();
    samplesUntilNext_ = 0.0;
}

double GranularEngine::nextSpawnInterval()
{
    const double densityHz = static_cast<double> (map::grainDensityHz (params_.density));
    double interval = sampleRate_ / densityHz;

    // Texture adds timing jitter to the spawn clock.
    interval *= 1.0 + static_cast<double> (params_.texture) * 0.75 * static_cast<double> (rng_.nextBipolar());
    return interval > 16.0 ? interval : 16.0;
}

void GranularEngine::spawnGrain (const SampleBuffer& buffer)
{
    GrainVoice* voice = pool_.findFreeVoice();
    if (voice == nullptr)
        return;   // Pool exhausted - skip this grain, never allocate.

    const float frames = static_cast<float> (buffer.getNumFrames());

    const float sizeSeconds = map::grainSizeSeconds (params_.size);
    const int   lengthSamples = static_cast<int> (sizeSeconds * static_cast<float> (sampleRate_));

    // Position with spray randomness; wrap into valid buffer range.
    float startFrame = params_.position * (frames - 1.0f)
                     + params_.spray * 0.5f * frames * rng_.nextBipolar();
    if (frames > 0.0f)
    {
        startFrame = std::fmod (startFrame, frames);
        if (startFrame < 0.0f)
            startFrame += frames;
    }

    // Pitch with per-grain texture detune (gentler than before).
    float increment = map::grainPitchRatio (params_.pitch)
                    * (1.0f + params_.texture * 0.04f * rng_.nextBipolar());

    // Stereo spread via per-grain equal-power pan.
    float gainL = 0.0f, gainR = 0.0f;
    equalPowerPan (params_.spread * rng_.nextBipolar(), gainL, gainR);

    // Level vs overlap: many grains overlapping raises peak/RMS. We still want
    // turning density or size up to feel fuller (more grains / longer bodies),
    // so do not assume the full "ideal" overlap when the fixed pool caps how
    // many grains can actually sound at once.
    const float overlapIdeal = map::grainDensityHz (params_.density) * sizeSeconds;
    const float overlapForGain = std::min (overlapIdeal, static_cast<float> (kGrainsPerTrack));
    // Gentler than 1/sqrt(overlap): strict sqrt made dense clouds feel too quiet.
    constexpr float kOverlapCompExponent = 0.36f;
    constexpr float kGrainLaunchGain     = 1.0f;
    const float denom = std::pow (std::max (1.0f, overlapForGain), kOverlapCompExponent);
    const float amp   = kGrainLaunchGain / denom;

    GrainVoice::StartParams sp;
    sp.startFrame    = startFrame;
    sp.increment     = increment;
    sp.lengthSamples = lengthSamples;
    sp.gainL         = gainL * amp;
    sp.gainR         = gainR * amp;
    voice->start (sp);
}

void GranularEngine::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    if (buffer.getNumFrames() < 2)
        return;

    if (spawning_)
    {
        samplesUntilNext_ -= static_cast<double> (numSamples);
        while (samplesUntilNext_ <= 0.0)
        {
            spawnGrain (buffer);
            samplesUntilNext_ += nextSpawnInterval();
        }
    }

    pool_.renderAll (buffer, outL, outR, numSamples);
}

void GranularEngine::fillGrainDisplay (const SampleBuffer& material, GrainDisplaySlot* out, int maxSlots) const
{
    if (out == nullptr || maxSlots <= 0)
        return;

    const float tf = static_cast<float> (std::max (1, material.getNumFrames()));
    pool_.fillGrainDisplay (tf, out, maxSlots);
}

} // namespace sculpt
