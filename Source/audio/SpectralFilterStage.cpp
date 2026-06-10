#include <cmath>
#include "SpectralFilterStage.h"
#include "../core/ParameterIds.h"
#include "../core/FilterScales.h"
#include "../util/MathUtils.h"

namespace sculpt
{

// Base MIDI note for band 0 (~30 Hz).
static constexpr float kBaseMidi = 23.0f;

void SpectralFilterStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    mix_.prepare       (sampleRate_, 0.02f);
    resonance_.prepare (sampleRate_, 0.04f);
    cutoff_.prepare    (sampleRate_, 0.04f);
    mix_.snap       (0.0f);
    resonance_.snap (0.2f);
    cutoff_.snap    (0.8f);
    reset();
}

void SpectralFilterStage::reset()
{
    leftStates_.fill  ({});
    rightStates_.fill ({});
    envelopes_.fill   (0.0f);
    peakEnvelope_     = 0.001f;
    lastCutoff_ = lastRes_ = lastDecay_ = lastPitch_ = lastScale_ = -1.0f;
    numActiveBands_ = 0;
}

void SpectralFilterStage::setParams (float cutoff01, float resonance01, float decay01,
                                     float pitch01,  float scaleIdx01, float mix01)
{
    mix_.setTarget       (mix01);
    resonance_.setTarget (resonance01);
    cutoff_.setTarget    (cutoff01);

    // Coefficients only need rebuilding when decay, pitch or scale change.
    // Cutoff and resonance are applied per-sample via SmoothedValues.
    const bool dirty = (decay01     != lastDecay_)
                    || (pitch01     != lastPitch_)
                    || (scaleIdx01  != lastScale_);
    if (! dirty)
        return;

    lastDecay_  = decay01;
    lastPitch_  = pitch01;
    lastScale_  = scaleIdx01;
    // Track cutoff/res so a full reset re-triggers on next setParams.
    lastCutoff_ = cutoff01;
    lastRes_    = resonance01;

    rebuildCoeffs();
}

void SpectralFilterStage::rebuildCoeffs()
{
    // Pole radius maps decay 0..1 → r ∈ [0.80, 0.999].
    // r = 0.80 → ring ~2ms; r = 0.999 → ring ~1.4s at 44100.
    const float r  = 0.80f + lastDecay_ * 0.199f;
    const float r2 = r * r;

    const float pitchSt   = map::filterPitchSemitones (lastPitch_);
    const float nyqLimit  = static_cast<float> (sampleRate_) * 0.475f;
    const FilterScale scale = normalizedToFilterScale (lastScale_);

    // Root MIDI note shifted by Pitch.
    const float rootMidi = kBaseMidi + pitchSt;
    const int   rootInt  = static_cast<int> (std::round (rootMidi));

    numActiveBands_ = 0;

    for (int b = 0; b < kNumBands; ++b)
    {
        const float rawMidi     = rootMidi + static_cast<float> (b * kSemisPerBand);
        const float snappedMidi = snapToScaleMidi (rawMidi, rootInt, scale);
        const float freqHz      = 440.0f * std::pow (2.0f, (snappedMidi - 69.0f) / 12.0f);

        auto& c = coeffs_[static_cast<size_t> (b)];

        if (freqHz < 20.0f || freqHz > nyqLimit)
        {
            c.active = false;
            continue;
        }

        const float w0 = kTwoPi * freqHz / static_cast<float> (sampleRate_);
        c.b0     = (1.0f - r2) * 0.5f;
        c.a1     = -2.0f * r * std::cos (w0);
        c.a2     = r2;
        // envGain is filled per-block in process() using smoothed cutoff.
        c.envGain = 0.0f;
        c.active  = true;
        ++numActiveBands_;
    }
}

float SpectralFilterStage::processBandSample (ChannelState& s, const BandCoeffs& c, float x) const
{
    // Direct Form II normalized BPF resonator.
    // At the centre frequency, peak output ≈ b0 / (1 - a2) = 0.5.
    const float w = x - c.a1 * s.w1 - c.a2 * s.w2;
    const float y = c.b0 * (w - s.w2);
    s.w2 = sanitize (s.w1);
    s.w1 = sanitize (w);
    return y;
}

void SpectralFilterStage::process (float* left, float* right, int numSamples)
{
    if (numActiveBands_ == 0)
        return;

    // Advance smoothers once per block and use their end-of-block values for
    // coefficient computation.  Do NOT also call next() inside the sample loop.
    const float mixVal      = mix_.skip (numSamples);
    const float resonanceVal = resonance_.skip (numSamples);
    const float cutoffNorm  = cutoff_.skip (numSamples);

    if (mixVal < 1.0e-5f)
        return;

    // peakBoost: Resonance 0..1 → additive peak gain per band.
    // Each BPF has peak output ≈ 0.5 at its centre frequency, so ×2 normalises
    // that to 1.  Then we scale by 6 for a max of +15 dB per isolated band.
    // We also normalise by sqrt(numActiveBands) so that the total additive sum
    // stays bounded regardless of how many bands overlap at a given frequency.
    const float sqrtBands  = std::sqrt (static_cast<float> (numActiveBands_));
    const float normFactor = sqrtBands > 0.0f ? 1.0f / sqrtBands : 1.0f;
    const float peakBoost  = resonanceVal * 6.0f * 2.0f * normFactor;

    const float cutoffHz = map::filterCutoffHz (cutoffNorm);

    // Pre-compute per-band spectral envelope × peak boost (once per block).
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& c = coeffs_[static_cast<size_t> (b)];
        if (! c.active)
            continue;

        // Recover band centre frequency from stored coefficients.
        const float r     = std::sqrt (c.a2);
        const float cosW0 = clampf (-c.a1 / (2.0f * r), -1.0f, 1.0f);
        const float w0    = std::acos (cosW0);
        const float fHz   = w0 * static_cast<float> (sampleRate_) / kTwoPi;

        // 2-pole LP spectral envelope: bands below cutoff resonate loudly.
        const float ratio   = fHz / cutoffHz;
        const float envGain = 1.0f / (1.0f + ratio * ratio);

        c.envGain = envGain * peakBoost;
    }

    // Envelope follower release coefficient (~200ms).
    const float envRelease = 1.0f - (1.0f / (0.2f * static_cast<float> (sampleRate_)));
    const float envReleaseC = clampf (envRelease, 0.0f, 0.9999f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float xL = left[i];
        const float xR = right[i];
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int b = 0; b < kNumBands; ++b)
        {
            const auto bs = static_cast<size_t> (b);
            const auto& c = coeffs_[bs];
            if (! c.active)
                continue;

            const float yL = processBandSample (leftStates_[bs],  c, xL);
            const float yR = processBandSample (rightStates_[bs], c, xR);

            sumL += c.envGain * yL;
            sumR += c.envGain * yR;

            // Envelope follower: fast attack, slow release.
            const float level = (std::fabs (yL) + std::fabs (yR)) * 0.5f;
            float& env = envelopes_[bs];
            env = level > env ? level : (env * envReleaseC);
        }

        // Additive: dry always passes through; resonance peaks added on top.
        // Soft-clip the additive sum to prevent digital overload while preserving
        // the character of heavy resonance.
        const float safeL = std::tanh (sumL * 0.5f) * 2.0f;
        const float safeR = std::tanh (sumR * 0.5f) * 2.0f;
        left[i]  = xL + safeL * mixVal;
        right[i] = xR + safeR * mixVal;
    }

    // Update running peak for LCD normalization.
    float maxEnv = peakEnvelope_ * 0.9995f;  // slow decay of the peak
    for (int b = 0; b < kNumBands; ++b)
    {
        const float e = envelopes_[static_cast<size_t> (b)];
        if (e > maxEnv)
            maxEnv = e;
    }
    peakEnvelope_ = maxEnv < 0.001f ? 0.001f : maxEnv;
}

float SpectralFilterStage::getBandEnvelope (int band) const
{
    if (band < 0 || band >= kNumBands)
        return 0.0f;
    // Return normalized 0..1 value relative to running peak.
    return clampf (envelopes_[static_cast<size_t> (band)] / peakEnvelope_, 0.0f, 1.0f);
}

} // namespace sculpt
