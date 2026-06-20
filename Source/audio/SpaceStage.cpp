#include "SpaceStage.h"
#include "GranularEngine.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

void SpaceStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0e-6 ? sampleRate : 44100.0;
    delay_.prepare (sampleRate_);
    reverb_.prepare (sampleRate_);
    delayWetGainSm_.prepare (sampleRate_, 0.015f);  // ~15 ms wet-mix ramp (declick p-lock jumps)
    reverbWetGainSm_.prepare (sampleRate_, 0.015f);
    delayWetGainSm_.snap (0.0f);
    reverbWetGainSm_.snap (0.0f);
    reset();
}

void SpaceStage::reset()
{
    delay_.reset();
    reverb_.reset();
}

void SpaceStage::setParams (float delayAmt01, float delayTime01, float revAmt01, float revSize01,
                            float delayFb01, float spread01, float damp01, float revDecay01,
                            float timeMode01, bool freeze, double engineSampleRate,
                            const GranularBlockTiming& timing)
{
    (void) engineSampleRate;
    const double sr = sampleRate_;

    delayAmt_ = clamp01 (delayAmt01);
    revAmt_   = clamp01 (revAmt01);

    const int modeIdx = map::spaceDelayTimeModeIndex (timeMode01);
    float     delaySec = 0.05f;
    if (modeIdx >= 3)
        delaySec = map::spaceDelayTimeFreeSeconds (delayTime01);
    else
    {
        double       spb = timing.samplesPerBeat;
        const double bpm = std::max (1.0, timing.bpm);
        if (spb <= 1.0)
            spb = sr * 60.0 / bpm;
        const double beats = map::spaceSyncedDelayBeats (delayTime01, modeIdx);
        delaySec           = static_cast<float> (beats * spb / sr);
    }

    delaySeconds_ = delaySec;

    delay_.setDelaySeconds (delaySec);
    delay_.setFeedback (map::spaceDelayFeedbackGain (delayFb01));
    delay_.setDampHz (map::spaceDampCutoffHz (damp01));
    delay_.setSpread (spread01);
    delay_.setFreeze (freeze);

    reverbDecaySeconds_ = map::spaceReverbRt60Seconds (revDecay01);

    reverb_.setSize (map::spaceReverbSizeLineScale (revSize01));
    reverb_.setRt60 (reverbDecaySeconds_);
    reverb_.setDampHz (map::spaceDampCutoffHz (damp01));
    const float diffW = spread01 > 0.5f ? (spread01 - 0.5f) * 2.0f : 0.0f;
    reverb_.setStereoWidth (diffW);
    reverb_.setFreeze (freeze);
}

void SpaceStage::process (float* left, float* right, int numSamples)
{
    const int n = numSamples < kMaxBlockSize ? numSamples : kMaxBlockSize;
    if (left == nullptr || right == nullptr || n <= 0)
        return;

    if (clearRequested_.exchange (false, std::memory_order_acq_rel))
    {
        delay_.reset();
        reverb_.reset();
        delayBypassed_  = true;
        reverbBypassed_ = true;
    }

    // Targets for the wet mix gains; smoothed per sample below so amount jumps don't click.
    delayWetGainSm_.setTarget  (delayAmt_ * delayAmt_ * 1.15f);
    reverbWetGainSm_.setTarget (revAmt_   * revAmt_   * 1.15f);

    // CPU bypass: when an amount is at (or smoothing through) zero its wet contribution is exactly
    // zero, so skip that engine entirely instead of running a full delay/Dattorro hall for silence.
    // On entering bypass we reset the lines once so a later re-enable starts cleanly from silence.
    constexpr float kAmtEps  = 1.0e-4f;
    constexpr float kGainEps = 1.0e-5f;
    const bool delayActive  = delayAmt_ > kAmtEps || delayWetGainSm_.getCurrent()  > kGainEps;
    const bool reverbActive = revAmt_   > kAmtEps || reverbWetGainSm_.getCurrent() > kGainEps;

    if (! delayActive && ! reverbActive)
    {
        if (! delayBypassed_)  { delay_.reset();  delayBypassed_  = true; }
        if (! reverbBypassed_) { reverb_.reset(); reverbBypassed_ = true; }
        // Output is the dry signal untouched; just decay the LCD meters.
        delayWet01_  *= 0.85f;
        reverbWet01_ *= 0.88f;
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        dryL_[static_cast<size_t> (i)] = left[i];
        dryR_[static_cast<size_t> (i)] = right[i];
    }

    if (delayActive)
    {
        delay_.process (dryL_.data(), dryR_.data(), wetDL_.data(), wetDR_.data(), n);
        delayBypassed_ = false;
    }
    else
    {
        if (! delayBypassed_) { delay_.reset(); delayBypassed_ = true; }
        for (int i = 0; i < n; ++i) { wetDL_[static_cast<size_t> (i)] = 0.0f; wetDR_[static_cast<size_t> (i)] = 0.0f; }
    }

    if (reverbActive)
    {
        for (int i = 0; i < n; ++i)
        {
            const float d  = dryL_[static_cast<size_t> (i)];
            const float e  = dryR_[static_cast<size_t> (i)];
            const float wL = wetDL_[static_cast<size_t> (i)];
            const float wR = wetDR_[static_cast<size_t> (i)];
            monoRv_[static_cast<size_t> (i)] = 0.5f * (d + e) + 0.18f * (wL + wR);
        }
        reverb_.processBlock (monoRv_.data(), wetRL_.data(), wetRR_.data(), n);
        reverbBypassed_ = false;
    }
    else
    {
        if (! reverbBypassed_) { reverb_.reset(); reverbBypassed_ = true; }
        for (int i = 0; i < n; ++i) { wetRL_[static_cast<size_t> (i)] = 0.0f; wetRR_[static_cast<size_t> (i)] = 0.0f; }
    }

    float delayPeak  = 0.0f;
    float reverbPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float dLin = delayWetGainSm_.next();
        const float rLin = reverbWetGainSm_.next();

        const float d  = dryL_[static_cast<size_t> (i)];
        const float e  = dryR_[static_cast<size_t> (i)];
        const float wL = wetDL_[static_cast<size_t> (i)];
        const float wR = wetDR_[static_cast<size_t> (i)];
        const float rL = wetRL_[static_cast<size_t> (i)];
        const float rR = wetRR_[static_cast<size_t> (i)];

        delayPeak  = std::max (delayPeak,  std::fabs (wL * dLin) + std::fabs (wR * dLin));
        reverbPeak = std::max (reverbPeak, std::fabs (rL * rLin) + std::fabs (rR * rLin));

        left[i]  = sanitize (d + wL * dLin + rL * rLin);
        right[i] = sanitize (e + wR * dLin + rR * rLin);
    }

    // Falling-peak meters for the LCD visuals (audible wet contribution).
    delayWet01_  = std::min (1.0f, std::max (delayPeak,  delayWet01_ * 0.85f));
    reverbWet01_ = std::min (1.0f, std::max (reverbPeak, reverbWet01_ * 0.88f));
}

} // namespace sculpt
