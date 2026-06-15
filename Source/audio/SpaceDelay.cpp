#include <algorithm>
#include <cmath>
#include "SpaceDelay.h"
#include "../util/Constants.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void SpaceDelay::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0e-6 ? sampleRate : 44100.0;
    capacity_   = static_cast<int> (sampleRate_ * static_cast<double> (kMaxSpaceDelaySeconds)) + 8;
    capacity_   = std::max (capacity_, 16);
    bufL_.assign (static_cast<size_t> (capacity_), 0.0f);
    bufR_.assign (static_cast<size_t> (capacity_), 0.0f);
    writeL_ = 0;
    writeR_ = 0;

    dampL_.prepare (sampleRate_);
    dampR_.prepare (sampleRate_);
    dampL_.setCutoffHz (8000.0f);
    dampR_.setCutoffHz (8000.0f);

    hpL_.prepare (sampleRate_);
    hpR_.prepare (sampleRate_);
    hpL_.setCutoffHz (120.0f);
    hpR_.setCutoffHz (120.0f);

    wowInc_       = kTwoPi * 0.55f / static_cast<float> (sampleRate_);
    flutterInc_   = kTwoPi * 6.3f  / static_cast<float> (sampleRate_);
    wowDepth_     = static_cast<float> (sampleRate_) * 0.0016f; // ~1.6 ms drift
    flutterDepth_ = static_cast<float> (sampleRate_) * 0.0003f; // ~0.3 ms jitter

    delaySm_.prepare (sampleRate_, 0.12f);
    delaySm_.snap (static_cast<float> (0.05 * sampleRate_));

    reset();
}

void SpaceDelay::reset()
{
    std::fill (bufL_.begin(), bufL_.end(), 0.0f);
    std::fill (bufR_.begin(), bufR_.end(), 0.0f);
    writeL_ = 0;
    writeR_ = 0;
    dampL_.reset();
    dampR_.reset();
    hpL_.reset();
    hpR_.reset();
    wowPhase_     = 0.0f;
    flutterPhase_ = 0.0f;
}

void SpaceDelay::setDelaySeconds (float seconds)
{
    const float mx = std::max (0.0005f, kMaxSpaceDelaySeconds - 0.0005f);
    const float s  = std::clamp (seconds, 0.0005f, mx);
    delaySm_.setTarget (static_cast<float> (s * static_cast<float> (sampleRate_)));
}

void SpaceDelay::setFeedback (float feedback01) { feedback_ = clamp01 (feedback01); }

void SpaceDelay::setDampHz (float hz)
{
    dampL_.setCutoffHz (hz);
    dampR_.setCutoffHz (hz);
}

void SpaceDelay::setSpread (float spread01) { spread01_ = clamp01 (spread01); }

float SpaceDelay::readInterp (const std::vector<float>& buf, int writePos, float delaySamples) const
{
    const int cap = static_cast<int> (buf.size());
    if (cap < 2)
        return 0.0f;

    const float d = std::clamp (delaySamples, 1.0f, static_cast<float> (cap - 1));
    const float readPos = static_cast<float> (writePos) - d;
    float       base    = readPos;
    while (base < 0.0f)
        base += static_cast<float> (cap);

    const int   i0 = static_cast<int> (base) % cap;
    const int   i1 = (i0 + 1) % cap;
    const float frac = base - std::floor (base);
    const float s0   = buf[static_cast<size_t> (i0)];
    const float s1   = buf[static_cast<size_t> (i1)];
    return s0 + frac * (s1 - s0);
}

void SpaceDelay::push (std::vector<float>& buf, int& writePos, float v)
{
    buf[static_cast<size_t> (writePos)] = sanitize (v);
    if (++writePos >= capacity_)
        writePos = 0;
}

void SpaceDelay::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    const float ping = spread01_ < 0.5f ? (1.0f - spread01_ * 2.0f) : 0.0f; // 1 at 0, 0 at 0.5
    const float diff = spread01_ > 0.5f ? (spread01_ - 0.5f) * 2.0f : 0.0f; // 0 at 0.5, 1 at 1

    const float fbGain = freeze_ ? 1.0f : feedback_;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dBase = std::max (2.0f, delaySm_.next());

        const float wow     = wowDepth_ * std::sin (wowPhase_);
        const float flutter = flutterDepth_ * std::sin (flutterPhase_);
        wowPhase_     += wowInc_;     if (wowPhase_ > kTwoPi)     wowPhase_ -= kTwoPi;
        flutterPhase_ += flutterInc_; if (flutterPhase_ > kTwoPi) flutterPhase_ -= kTwoPi;

        const float dL = std::max (2.0f, dBase + wow + flutter);
        const float dR = std::max (2.0f, dBase * (1.0f + 0.008f * diff) - wow + flutter);

        const float wetL = readInterp (bufL_, writeL_, dL);
        const float wetR = readInterp (bufR_, writeR_, dR);

        outL[i] = wetL;
        outR[i] = wetR;

        const float fbInL = wetL * (1.0f - ping) + wetR * ping;
        const float fbInR = wetR * (1.0f - ping) + wetL * ping;

        const float dryL = freeze_ ? 0.0f : inL[i];
        const float dryR = freeze_ ? 0.0f : inR[i];

        // In-loop tone shaping: low-cut, high damping.
        float qL = dampL_.processLowpass (hpL_.processHighpass (fbInL));
        float qR = dampR_.processLowpass (hpR_.processHighpass (fbInR));

        // Soft saturation for analog character + self-limiting (bypassed when frozen
        // so the held tail stays lossless).
        if (! freeze_)
        {
            qL = std::tanh (qL * 1.2f) * 0.9f;
            qR = std::tanh (qR * 1.2f) * 0.9f;
        }

        push (bufL_, writeL_, dryL + qL * fbGain);
        push (bufR_, writeR_, dryR + qR * fbGain);
    }
}

} // namespace sculpt
