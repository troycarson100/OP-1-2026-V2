#include "MixBusStage.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{
namespace
{
    constexpr float kCompRatio      = 4.0f;
    // Fast attack so the level detector tracks peaks without sample-rate zipper from peak-vs-envelope max().
    constexpr float kAttackSec      = 0.001f;
    constexpr float kReleaseSec   = 0.14f;
    constexpr double kGainReleaseMs = 85.0;
    constexpr float kEqLowHz        = 110.0f;
    constexpr float kEqLowQ         = 0.55f;
    constexpr float kEqMidHz        = 720.0f;
    constexpr float kEqMidQ         = 0.95f;
    constexpr float kEqHighHz       = 6200.0f;
    constexpr float kEqHighQ        = 0.65f;

    inline bool nearlyEqual (float a, float b)
    {
        return std::fabs (a - b) < 1.0e-6f;
    }
} // namespace

void MixBusStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 44100.0;

    const double sr = sampleRate_;
    attCoeff_ = static_cast<float> (1.0 - std::exp (-1.0 / (static_cast<double> (kAttackSec) * sr)));
    relCoeff_ = static_cast<float> (1.0 - std::exp (-1.0 / (static_cast<double> (kReleaseSec) * sr)));
    gainRelCoeff_ = static_cast<float> (1.0 - std::exp (-1.0 / (kGainReleaseMs * 0.001 * sr)));

    reset();
    eqCoeffsDirty_ = true;
}

void MixBusStage::reset()
{
    lowL_.reset();
    midL_.reset();
    hiL_.reset();
    lowR_.reset();
    midR_.reset();
    hiR_.reset();
    env_ = 0.0f;
    smoothGainLin_ = 1.0f;
    compReductionMeter01_ = 0.0f;
}

void MixBusStage::setPeaking (Biquad& b, double sr, float freqHz, float Q, float gainDb)
{
    const float w0 = static_cast<float> (kTwoPi * static_cast<double> (freqHz) / sr);
    const float cosw0 = std::cos (w0);
    const float sinw0 = std::sin (w0);
    const float alpha = sinw0 / (2.0f * std::max (0.05f, Q));
    const float A = std::pow (10.0f, gainDb / 40.0f);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cosw0;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cosw0;
    float a2 = 1.0f - alpha / A;

    const float invA0 = 1.0f / a0;
    b.b0 = b0 * invA0;
    b.b1 = b1 * invA0;
    b.b2 = b2 * invA0;
    b.a1 = a1 * invA0;
    b.a2 = a2 * invA0;
}

void MixBusStage::recomputeCoeffs()
{
    const float gL = map::mixEqBandGainDb (eqLow01_);
    const float gM = map::mixEqBandGainDb (eqMid01_);
    const float gH = map::mixEqBandGainDb (eqHigh01_);
    const double sr = sampleRate_;

    setPeaking (lowL_, sr, kEqLowHz, kEqLowQ, gL);
    setPeaking (midL_, sr, kEqMidHz, kEqMidQ, gM);
    setPeaking (hiL_, sr, kEqHighHz, kEqHighQ, gH);

    lowR_.b0 = lowL_.b0;
    lowR_.b1 = lowL_.b1;
    lowR_.b2 = lowL_.b2;
    lowR_.a1 = lowL_.a1;
    lowR_.a2 = lowL_.a2;

    midR_.b0 = midL_.b0;
    midR_.b1 = midL_.b1;
    midR_.b2 = midL_.b2;
    midR_.a1 = midL_.a1;
    midR_.a2 = midL_.a2;

    hiR_.b0 = hiL_.b0;
    hiR_.b1 = hiL_.b1;
    hiR_.b2 = hiL_.b2;
    hiR_.a1 = hiL_.a1;
    hiR_.a2 = hiL_.a2;
}

void MixBusStage::setParams (float eqLow01, float eqMid01, float eqHigh01,
                             float compThreshold01, float compMakeup01)
{
    const float el = clamp01 (eqLow01);
    const float em = clamp01 (eqMid01);
    const float eh = clamp01 (eqHigh01);

    if (! nearlyEqual (el, eqLow01_) || ! nearlyEqual (em, eqMid01_) || ! nearlyEqual (eh, eqHigh01_))
        eqCoeffsDirty_ = true;

    eqLow01_   = el;
    eqMid01_   = em;
    eqHigh01_  = eh;
    compThr01_ = clamp01 (compThreshold01);
    compMk01_  = clamp01 (compMakeup01);
}

void MixBusStage::process (float* left, float* right, int numSamples)
{
    if (eqCoeffsDirty_)
    {
        recomputeCoeffs();
        eqCoeffsDirty_ = false;
    }

    const float gLd = std::fabs (map::mixEqBandGainDb (eqLow01_));
    const float gMd = std::fabs (map::mixEqBandGainDb (eqMid01_));
    const float gHd = std::fabs (map::mixEqBandGainDb (eqHigh01_));
    constexpr float kEqFlatEpsDb = 0.12f;
    const bool      eqOff        = (gLd < kEqFlatEpsDb) && (gMd < kEqFlatEpsDb) && (gHd < kEqFlatEpsDb);

    const float thrDb = map::mixCompThresholdDb (compThr01_);
    const bool  compOff = (compMk01_ <= 1.0e-4f) && (thrDb >= 1.5f);
    const float mkLin   = compOff ? 1.0f : dbToGain (map::mixCompMakeupDb (compMk01_));

    if (eqOff && compOff)
    {
        smoothGainLin_        = 1.0f;
        compReductionMeter01_ *= 0.9f;
        return;
    }

    if (compOff)
        smoothGainLin_ = 1.0f;

    float peakReductionDb = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];

        if (! eqOff)
        {
            l = lowL_.process (l);
            l = midL_.process (l);
            l = hiL_.process (l);

            r = lowR_.process (r);
            r = midR_.process (r);
            r = hiR_.process (r);
        }

        if (compOff)
        {
            smoothGainLin_ = 1.0f;
            left[i]        = clampf (sanitize (l), -1.0f, 1.0f);
            right[i]       = clampf (sanitize (r), -1.0f, 1.0f);
            continue;
        }

        const float pk = std::max (std::fabs (l), std::fabs (r));

        if (pk > env_)
            env_ = pk + attCoeff_ * (env_ - pk);
        else
            env_ = pk + relCoeff_ * (env_ - pk);

        env_ = sanitize (env_);

        const float envDb = 20.0f * std::log10 (std::max (env_, 1.0e-10f));
        float       grDb  = 0.0f;
        if (envDb > thrDb)
        {
            const float over = envDb - thrDb;
            grDb = -over * (1.0f - 1.0f / kCompRatio);
        }
        grDb = std::min (0.0f, grDb);

        const float targetLin = dbToGain (grDb) * mkLin;
        if (targetLin < smoothGainLin_)
            smoothGainLin_ = targetLin;
        else
            smoothGainLin_ += gainRelCoeff_ * (targetLin - smoothGainLin_);

        if (grDb < 0.0f)
            peakReductionDb = std::max (peakReductionDb, -grDb);

        left[i]  = clampf (sanitize (l * smoothGainLin_), -1.0f, 1.0f);
        right[i] = clampf (sanitize (r * smoothGainLin_), -1.0f, 1.0f);
    }

    const float instant = clamp01 (peakReductionDb / 18.0f);
    compReductionMeter01_ = compReductionMeter01_ * 0.82f + instant * 0.18f;
}

} // namespace sculpt
