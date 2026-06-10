#include "Mixer.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void Mixer::prepare (double sampleRate)
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        levels_[static_cast<size_t> (t)].prepare (sampleRate, 0.02f);
        levels_[static_cast<size_t> (t)].snap (parameterDefault (ParameterId::TrackLevel));
        pans_[static_cast<size_t> (t)].prepare (sampleRate, 0.02f);
        pans_[static_cast<size_t> (t)].snap (parameterDefault (ParameterId::TrackPan));
    }

    outputGain_.prepare (sampleRate, 0.02f);
    outputGain_.snap (parameterDefault (ParameterId::OutputGain));
}

void Mixer::setTrackLevel (int track, float level01)
{
    if (track >= 0 && track < kNumTracks)
        levels_[static_cast<size_t> (track)].setTarget (level01);
}

void Mixer::setTrackPan (int track, float pan01)
{
    if (track >= 0 && track < kNumTracks)
        pans_[static_cast<size_t> (track)].setTarget (pan01);
}

void Mixer::setOutputGain (float gain01)
{
    outputGain_.setTarget (gain01);
}

void Mixer::process (const float* const* trackL, const float* const* trackR,
                     float* outL, float* outR, int numSamples)
{
    // Per-track gains update at block rate; output gain per sample.
    float gainL[kNumTracks];
    float gainR[kNumTracks];

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        const float gain = map::trackGain (levels_[ts].skip (numSamples));
        float pl = 0.0f, pr = 0.0f;
        equalPowerPan (map::pan (pans_[ts].skip (numSamples)), pl, pr);
        gainL[t] = gain * pl;
        gainR[t] = gain * pr;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.0f, r = 0.0f;
        for (int t = 0; t < kNumTracks; ++t)
        {
            l += trackL[t][i] * gainL[t];
            r += trackR[t][i] * gainR[t];
        }

        const float master = map::outputGain (outputGain_.next());
        outL[i] = sanitize (l * master);
        outR[i] = sanitize (r * master);
    }
}

} // namespace sculpt
