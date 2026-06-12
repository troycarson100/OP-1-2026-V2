#pragma once

#include "../core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{
namespace warpLaunchSync
{

// Same warp speed law as Track::updateParameters (TapePlayer ratio; sign = reverse).
inline float warpTapeSpeedRatio (float tapeKnobRaw, bool snapOn, float rootBpm, double hostBpm)
{
    float root = rootBpm < 1.0e-3f ? 120.0f : rootBpm;
    double hb    = hostBpm;
    if (! (hb > 1.0 && hb < 999.0))
        hb = static_cast<double> (root);

    float sync = static_cast<float> (hb / static_cast<double> (root));
    sync         = std::clamp (sync, 0.1f, 10.0f);
    float varis  = map::warpVarispeedMultiplier (tapeKnobRaw);
    if (snapOn)
        varis = map::quantizeWarpVarispeedMultiplier (varis);
    const float sign = (tapeKnobRaw < 0.48f) ? -1.0f : 1.0f;
    return sign * sync * varis;
}

// Beats (host quarter notes) for one pass through the normalized loop region at current warp speed.
inline double loopRegionBeatsHost (int numFrames, float loopStart01, float loopEnd01,
                                   double sampleRate, float warpSpeedRatio, double hostBpm)
{
    if (numFrames < 2 || sampleRate <= 1.0e-6 || ! (hostBpm > 1.0 && hostBpm < 999.0))
        return 0.0;

    const double last   = static_cast<double> (numFrames - 1);
    const double rs     = static_cast<double> (clamp01 (loopStart01)) * last;
    const double re     = static_cast<double> (clamp01 (loopEnd01)) * last;
    const double region = std::max (1.0, re - rs);
    const double spd    = static_cast<double> (std::max (1.0e-6f, std::fabs (warpSpeedRatio)));
    const double wallSec = region / (sampleRate * spd);
    return wallSec * (hostBpm / 60.0);
}

// Phase 0..1 within the loop span from a full-buffer normalized tape position.
inline float loopPhase01FromTapeNorm (float tapeNormFullBuffer, float loopStart01, float loopEnd01)
{
    const double span = static_cast<double> (std::max (1.0e-5f, loopEnd01 - loopStart01));
    double       u    = static_cast<double> (tapeNormFullBuffer) - static_cast<double> (loopStart01);
    u                 = std::fmod (std::fmod (u, span) + span, span);
    return static_cast<float> (u / span);
}

// Material playhead 0..1 (full buffer) so tape starts at the given host-beat phase within the loop.
inline float materialPlayheadForBeatPhase (double targetHostBeat, int numFrames, float loopStart01,
                                           float loopEnd01, double sampleRate, float warpSpeedRatio,
                                           double hostBpm)
{
    const double lb = loopRegionBeatsHost (numFrames, loopStart01, loopEnd01, sampleRate,
                                             warpSpeedRatio, hostBpm);
    if (lb < 1.0e-6 || numFrames < 2)
        return clamp01 (loopStart01);

    double phaseBeats = std::fmod (targetHostBeat, lb);
    if (phaseBeats < 0.0)
        phaseBeats += lb;
    const float phase01 = static_cast<float> (phaseBeats / lb);
    return clamp01 (loopStart01 + phase01 * (loopEnd01 - loopStart01));
}

// Match another track's fractional position inside its loop (DJ-style stack).
inline float materialPlayheadForRefPhase (float refTapeNormFullBuffer, float refLoopStart01,
                                          float refLoopEnd01, float loopStart01, float loopEnd01)
{
    const float phase = loopPhase01FromTapeNorm (refTapeNormFullBuffer, refLoopStart01, refLoopEnd01);
    return clamp01 (loopStart01 + phase * (loopEnd01 - loopStart01));
}

// Next host quarter-note boundary we align to (same as "up" beat for launch scheduling).
inline double warpLaunchQuantizedBeat (double beatPosition)
{
    return std::ceil (beatPosition - 1.0e-9);
}

// True when we're already on (or negligibly past) that boundary — fire immediately.
inline bool warpLaunchIsImmediate (double beatPosition, double quantizedBeat)
{
    return (quantizedBeat - beatPosition) < 1.0e-5;
}

} // namespace warpLaunchSync
} // namespace sculpt
