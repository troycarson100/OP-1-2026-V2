#include "SpaceSendBus.h"
#include "GranularEngine.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

void SpaceSendBus::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0e-6 ? sampleRate : 44100.0;
    delay_.prepare (sampleRate_);
    reverb_.prepare (sampleRate_);
    reset();
}

void SpaceSendBus::reset()
{
    delay_.reset();
    reverb_.reset();
    delayBypassed_  = true;
    reverbBypassed_ = true;
    delaySilent_    = true;
    reverbSilent_   = true;
    delayWet01_     = 0.0f;
    reverbWet01_    = 0.0f;
}

void SpaceSendBus::setParams (float delayTime01, float delayFb01, float delayTimeMode01,
                              float revSize01, float revDecay01, float damp01, float spread01,
                              bool freeze, const GranularBlockTiming& timing)
{
    const double sr = sampleRate_;

    const int modeIdx = map::spaceDelayTimeModeIndex (delayTimeMode01);
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

void SpaceSendBus::processAdd (const float* delaySendL, const float* delaySendR,
                               const float* revSendL, const float* revSendR,
                               float* outL, float* outR, int numSamples)
{
    const int n = numSamples < kMaxBlockSize ? numSamples : kMaxBlockSize;
    if (outL == nullptr || outR == nullptr || n <= 0)
        return;

    if (clearRequested_.exchange (false, std::memory_order_acq_rel))
        reset();

    constexpr float kEps = 1.0e-5f;

    // Peak of what's being sent in this block (decides whether to keep an engine running).
    float dInPeak = 0.0f, rInPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        dInPeak = std::max (dInPeak, std::fabs (delaySendL[idx]) + std::fabs (delaySendR[idx]));
        rInPeak = std::max (rInPeak, std::fabs (revSendL[idx])  + std::fabs (revSendR[idx]));
    }

    // Process while there is input, or while the previous block was still producing a wet tail.
    const bool processDelay  = dInPeak > kEps || ! delaySilent_;
    const bool processReverb = rInPeak > kEps || ! reverbSilent_;

    if (! processDelay && ! processReverb)
    {
        if (! delayBypassed_)  { delay_.reset();  delayBypassed_  = true; }
        if (! reverbBypassed_) { reverb_.reset(); reverbBypassed_ = true; }
        delayWet01_  *= 0.85f;
        reverbWet01_ *= 0.88f;
        return;
    }

    if (processDelay)
    {
        delay_.process (delaySendL, delaySendR, wetDL_.data(), wetDR_.data(), n);
        delayBypassed_ = false;
    }
    else
    {
        if (! delayBypassed_) { delay_.reset(); delayBypassed_ = true; }
        for (int i = 0; i < n; ++i) { wetDL_[static_cast<size_t> (i)] = 0.0f; wetDR_[static_cast<size_t> (i)] = 0.0f; }
    }

    if (processReverb)
    {
        for (int i = 0; i < n; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            // Reverb is fed by its own send plus a little of the delay return for glue.
            monoRv_[idx] = 0.5f * (revSendL[idx] + revSendR[idx]) + 0.18f * (wetDL_[idx] + wetDR_[idx]);
        }
        reverb_.processBlock (monoRv_.data(), wetRL_.data(), wetRR_.data(), n);
        reverbBypassed_ = false;
    }
    else
    {
        if (! reverbBypassed_) { reverb_.reset(); reverbBypassed_ = true; }
        for (int i = 0; i < n; ++i) { wetRL_[static_cast<size_t> (i)] = 0.0f; wetRR_[static_cast<size_t> (i)] = 0.0f; }
    }

    float dPeak = 0.0f, rPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        const float wL = wetDL_[idx], wR = wetDR_[idx];
        const float rL = wetRL_[idx], rR = wetRR_[idx];
        dPeak = std::max (dPeak, std::fabs (wL) + std::fabs (wR));
        rPeak = std::max (rPeak, std::fabs (rL) + std::fabs (rR));
        outL[idx] = sanitize (outL[idx] + wL + rL);
        outR[idx] = sanitize (outR[idx] + wR + rR);
    }

    // Silent (bypass next block) once both the input and the real wet output have decayed away.
    delaySilent_  = (dPeak < kEps && dInPeak < kEps);
    reverbSilent_ = (rPeak < kEps && rInPeak < kEps);

    delayWet01_  = std::min (1.0f, std::max (dPeak, delayWet01_  * 0.85f));
    reverbWet01_ = std::min (1.0f, std::max (rPeak, reverbWet01_ * 0.88f));
}

} // namespace sculpt
