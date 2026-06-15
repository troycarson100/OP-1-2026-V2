#pragma once

#include <vector>
#include "OnePole.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

// Tape-flavoured stereo delay: fractional read with wow+flutter modulation,
// cross-feedback ping-pong, and an in-loop highpass + lowpass + soft saturation
// so each repeat degrades musically. Allocates in prepare() only; process() is RT-safe.
class SpaceDelay
{
public:
    void prepare (double sampleRate);
    void reset();

    void setDelaySeconds (float seconds); // target delay time (smoothed toward in process)
    void setFeedback (float feedback01); // 0..1 -> gain
    void setDampHz (float hz);
    void setSpread (float spread01); // 0..0.5 ping-pong blend; 0.5..1 diffusion width
    void setFreeze (bool freeze) { freeze_ = freeze; }

    // Writes wet delay to outL/outR (replaces / is the delay wet path — caller adds dry).
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

private:
    float readInterp (const std::vector<float>& buf, int writePos, float delaySamples) const;
    void  push (std::vector<float>& buf, int& writePos, float v);

    double sampleRate_ = 44100.0;
    int    capacity_   = 1;
    int    writeL_     = 0;
    int    writeR_     = 0;

    std::vector<float> bufL_;
    std::vector<float> bufR_;

    SmoothedValue delaySm_;

    OnePole dampL_;
    OnePole dampR_;
    OnePole hpL_;   // feedback low-cut (removes mud buildup)
    OnePole hpR_;

    // Tape wow (slow) + flutter (fast) read-position modulation.
    float wowPhase_     = 0.0f;
    float flutterPhase_ = 0.0f;
    float wowInc_       = 0.0f;
    float flutterInc_   = 0.0f;
    float wowDepth_     = 0.0f; // samples
    float flutterDepth_ = 0.0f; // samples

    float feedback_ = 0.0f;
    float spread01_ = 0.5f;

    bool freeze_ = false;
};

} // namespace sculpt
