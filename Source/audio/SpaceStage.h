#pragma once

#include <array>
#include <atomic>

#include "SpaceDelay.h"
#include "HallReverb.h"
#include "../util/Constants.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

struct GranularBlockTiming;

// Vast-style space: stereo delay + 4-line FDN hall, eight primary controls + time mode + freeze.
class SpaceStage
{
public:
    void prepare (double sampleRate);
    void reset();

    void requestClear() { clearRequested_.store (true, std::memory_order_release); }

    void setParams (float delayAmt01, float delayTime01, float revAmt01, float revSize01,
                    float delayFb01, float spread01, float damp01, float revDecay01,
                    float timeMode01, bool freeze, double engineSampleRate,
                    const GranularBlockTiming& timing);

    void process (float* left, float* right, int numSamples);

    // Smoothed audible-wet levels (0..1) for the Space LCD visuals.
    float getDelayWet01() const  { return delayWet01_; }
    float getReverbWet01() const { return reverbWet01_; }

    // Mapped values for the Space LCD visuals (set in setParams).
    float getDelaySeconds() const        { return delaySeconds_; }
    float getReverbDecaySeconds() const  { return reverbDecaySeconds_; }

private:
    double sampleRate_ = 44100.0;

    SpaceDelay delay_;
    HallReverb reverb_;

    std::atomic<bool> clearRequested_ { false };

    std::array<float, kMaxBlockSize> dryL_ {}, dryR_ {}, wetDL_ {}, wetDR_ {}, wetRL_ {}, wetRR_ {}, monoRv_ {};

    float delayAmt_ = 0.0f;
    float revAmt_   = 0.0f;
    // Per-sample-smoothed wet mix gains so abrupt amount changes (e.g. a per-step p-lock) ramp
    // instead of stepping the wet level mid-tail (which clicks).
    SmoothedValue delayWetGainSm_;
    SmoothedValue reverbWetGainSm_;

    // CPU bypass latches: true while that engine is skipped (amount + smoothed wet gain at zero).
    // Used so the delay/reverb lines are reset exactly once on entering bypass (clean re-enable).
    bool delayBypassed_  = true;
    bool reverbBypassed_ = true;

    float delayWet01_  = 0.0f;
    float reverbWet01_ = 0.0f;

    float delaySeconds_       = 0.25f;
    float reverbDecaySeconds_ = 2.0f;
};

} // namespace sculpt
