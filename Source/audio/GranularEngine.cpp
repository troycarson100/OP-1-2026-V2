#include <cmath>
#include "GranularEngine.h"
#include "SampleBuffer.h"
#include "../core/ParameterIds.h"
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

    // Position with spray randomness, wrapping handled by the buffer read.
    float startFrame = params_.position * (frames - 1.0f)
                     + params_.spray * 0.5f * frames * rng_.nextBipolar();

    // Pitch with per-grain texture detune.
    float increment = map::grainPitchRatio (params_.pitch)
                    * (1.0f + params_.texture * 0.08f * rng_.nextBipolar());

    // Stereo spread via per-grain equal-power pan.
    float gainL = 0.0f, gainR = 0.0f;
    equalPowerPan (params_.spread * rng_.nextBipolar(), gainL, gainR);

    // Compensate level for expected grain overlap so density doesn't clip.
    const float overlap = map::grainDensityHz (params_.density) * sizeSeconds;
    const float amp = 0.85f / std::sqrt (overlap > 1.0f ? overlap : 1.0f);

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

} // namespace sculpt
