#include <cmath>
#include "SpectralFilterStage.h"
#include "../core/ParameterIds.h"
#include "../core/FilterScales.h"
#include "../util/MathUtils.h"

namespace sculpt
{

// Base MIDI note for band 0 (~30 Hz).
static constexpr float kBaseMidi = 23.0f;

// Subtle fixed L/R detune (~±2.5 cents) for chorus-like width.
static constexpr float kDetuneL = 0.99855645f;   // 2^(-2.5/1200)
static constexpr float kDetuneR = 1.00144563f;   // 2^(+2.5/1200)

// Internal spectral animation: slow traveling wave across band gains.
static constexpr float kAnimRateHz = 0.13f;
static constexpr float kAnimDepth  = 0.30f;
static constexpr float kAnimSpread = 0.55f;      // phase offset per band

static constexpr float kWetTrim    = 0.8f;
static constexpr float kDryBleed   = 0.12f;      // body kept at full wet

static inline float midiToHz (float midi)
{
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

// Soft-knee safety: unity below |4|, then smoothly compresses.  High enough
// that it is a net for accidents, never part of the tone.
static inline float softLimit (float x)
{
    constexpr float knee = 4.0f;
    const float a = std::fabs (x);
    if (a <= knee)
        return x;
    const float over = std::tanh (a - knee);
    return x > 0.0f ? knee + over : -(knee + over);
}

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
    peakEnvelope_ = 0.001f;
    animPhase_    = 0.0f;
    lastCutoff_ = lastRes_ = lastDecay_ = lastPitch_ = lastScale_ = lastKey_ = -1.0f;
    coeffsInit_ = false;
    numActiveBands_ = 0;
}

void SpectralFilterStage::setParams (float cutoff01, float resonance01, float decay01,
                                     float pitch01, float scaleIdx01, float key01, float mix01)
{
    mix_.setTarget       (mix01);
    resonance_.setTarget (resonance01);
    cutoff_.setTarget    (cutoff01);

    lastCutoff_ = cutoff01;

    const bool dirty = (decay01     != lastDecay_)
                    || (resonance01 != lastRes_)
                    || (pitch01     != lastPitch_)
                    || (scaleIdx01  != lastScale_)
                    || (key01       != lastKey_);
    if (! dirty)
        return;

    lastDecay_  = decay01;
    lastRes_    = resonance01;
    lastPitch_  = pitch01;
    lastScale_  = scaleIdx01;
    lastKey_    = key01;

    rebuildCoeffs();
}

void SpectralFilterStage::assignBand (int index, float midiNote, float qRes, float t60)
{
    const float fs  = static_cast<float> (sampleRate_);
    const float nyq = fs * 0.475f;
    const float f   = midiToHz (midiNote);

    auto& bd = bands_[static_cast<size_t> (index)];

    // Q combines both knobs: Resonance sets constant-Q bandwidth, Decay sets
    // a minimum ring time (T60 → Q via Q = pi * f * t60 / ln(1000)).
    const float qDecay = kPi * f * t60 / 6.91f;
    const float q      = clampf (qRes > qDecay ? qRes : qDecay, 0.5f, 500.0f);

    bd.tK  = 1.0f / q;
    bd.tGL = std::tan (kPi * clampf (f * kDetuneL, 20.0f, nyq) / fs);
    bd.tGR = std::tan (kPi * clampf (f * kDetuneR, 20.0f, nyq) / fs);

    bd.freqHz = f;

    // Resonant bloom: narrower bands ring louder, like an analog SVF pushed
    // into resonance (gain grows with Q).  Capped at +18 dB.
    bd.bloom = clampf (0.7f * std::sqrt (q), 1.0f, 8.0f);

    // First build (or band just became active): snap live coeffs to targets.
    if (! coeffsInit_ || ! bd.active)
    {
        bd.gL = bd.tGL;
        bd.gR = bd.tGR;
        bd.k  = bd.tK;
    }

    bd.active = true;
}

void SpectralFilterStage::rebuildCoeffs()
{
    const float fs  = static_cast<float> (sampleRate_);
    const float nyq = fs * 0.475f;
    const float d   = clamp01 (lastDecay_);
    const float qn  = clamp01 (lastRes_);

    // Resonance → constant-Q bandwidth in semitones (4 st → 0.08 st).
    const float bwSemis = 4.0f * std::pow (0.02f, qn);
    const float bwRatio = std::pow (2.0f, bwSemis / 12.0f) - 1.0f;
    const float qRes    = 1.0f / bwRatio;

    // Decay → ring time (T60 ~40ms .. ~3s).
    const float t60 = 0.04f + d * d * 3.0f;

    const float pitchSt   = map::filterPitchSemitones (lastPitch_);
    const FilterScale scale = normalizedToFilterScale (lastScale_);
    const int         tonicPc = normalizedToKeyIndex (lastKey_);

    numActiveBands_ = 0;
    int b = 0;

    if (scale == FilterScale::Free)
    {
        // Continuous grid, unquantized pitch offset.  Sub-audible steps are
        // skipped (deep negative Pitch) rather than truncating the bank.
        for (int gi = 0; gi < kNumBands * 2 && b < kNumBands; ++gi)
        {
            const float midi = kBaseMidi + pitchSt + static_cast<float> (gi * kSemisPerBand);
            const float f    = midiToHz (midi);
            if (f < 20.0f)
                continue;
            if (f > nyq)
                break;
            assignBand (b, midi, qRes, t60);
            ++numActiveBands_;
            ++b;
        }
    }
    else
    {
        // One band per scale note of the Key, walked upward from the lowest
        // note >= base + pitch (rounded so the grid stays in key).
        int count = 0;
        const int* iv = scaleIntervals (scale, count);

        const int startNote = static_cast<int> (std::ceil (kBaseMidi + std::round (pitchSt)));
        int oct = (startNote - tonicPc) / 12 - 2;   // safely below the start
        bool done = false;

        while (! done && b < kNumBands)
        {
            for (int i = 0; i < count && b < kNumBands; ++i)
            {
                const int note = oct * 12 + tonicPc + iv[i];
                if (note < startNote)
                    continue;

                const float f = midiToHz (static_cast<float> (note));
                if (f > nyq)
                {
                    done = true;
                    break;
                }
                if (f < 20.0f)
                    continue;

                assignBand (b, static_cast<float> (note), qRes, t60);
                ++numActiveBands_;
                ++b;
            }
            ++oct;
        }
    }

    for (; b < kNumBands; ++b)
        bands_[static_cast<size_t> (b)].active = false;

    coeffsInit_ = true;
}

void SpectralFilterStage::process (float* left, float* right, int numSamples)
{
    if (numActiveBands_ == 0)
        return;

    const float mixVal       = mix_.skip (numSamples);
    const float resonanceVal = resonance_.skip (numSamples);
    const float cutoffNorm   = cutoff_.skip (numSamples);

    if (mixVal < 1.0e-5f)
        return;

    const float fs = static_cast<float> (sampleRate_);

    // Equal-power crossfade: mix = 1 means 100% resonator bank (plus bleed).
    const float dryGain = std::cos (mixVal * kPi * 0.5f);
    const float wetGain = std::sin (mixVal * kPi * 0.5f);

    // Cutoff sweeps a smooth shoulder across the band index so the knob feels
    // even over its whole travel; Resonance narrows the shoulder.
    const float cutPos    = 1.0f + cutoffNorm * static_cast<float> (kNumBands + 6);
    const float fadeBands = 10.0f - 6.0f * resonanceVal;
    const float invFade   = 1.0f / fadeBands;

    // Advance the spectral animation (slow traveling wave across the bank).
    animPhase_ += kTwoPi * kAnimRateHz * static_cast<float> (numSamples) / fs;
    if (animPhase_ > kTwoPi)
        animPhase_ -= kTwoPi;

    for (int b = 0; b < kNumBands; ++b)
    {
        auto& bd = bands_[static_cast<size_t> (b)];
        if (! bd.active)
            continue;

        // Slew live coefficients toward targets — click-free knob moves.
        bd.gL += 0.18f * (bd.tGL - bd.gL);
        bd.gR += 0.18f * (bd.tGR - bd.gR);
        bd.k  += 0.18f * (bd.tK  - bd.k);
        bd.dL = 1.0f / (1.0f + bd.gL * (bd.gL + bd.k));
        bd.dR = 1.0f / (1.0f + bd.gR * (bd.gR + bd.k));

        const float t   = clamp01 ((cutPos - static_cast<float> (b)) * invFade);
        const float env = t * t * (3.0f - 2.0f * t);   // smoothstep shoulder

        const float anim = 1.0f + kAnimDepth * std::sin (animPhase_ + static_cast<float> (b) * kAnimSpread);

        const float gBase = env * anim * bd.bloom * kWetTrim;

        // Alternate band panning: odd bands lean left, even lean right.
        const bool odd = (b & 1) != 0;
        bd.gainL = gBase * (odd ? 1.15f : 0.85f);
        bd.gainR = gBase * (odd ? 0.85f : 1.15f);
    }

    const float envRelease  = 1.0f - (1.0f / (0.2f * fs));
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
            const auto& bd = bands_[bs];
            if (! bd.active)
                continue;

            // TPT SVF bandpass (Zavalishin); k*v1 is the unity-peak BP output.
            auto& sL = leftStates_[bs];
            const float v1L = (sL.ic1 + bd.gL * (xL - sL.ic2)) * bd.dL;
            const float v2L = sL.ic2 + bd.gL * v1L;
            sL.ic1 = sanitize (2.0f * v1L - sL.ic1);
            sL.ic2 = sanitize (2.0f * v2L - sL.ic2);

            auto& sR = rightStates_[bs];
            const float v1R = (sR.ic1 + bd.gR * (xR - sR.ic2)) * bd.dR;
            const float v2R = sR.ic2 + bd.gR * v1R;
            sR.ic1 = sanitize (2.0f * v1R - sR.ic1);
            sR.ic2 = sanitize (2.0f * v2R - sR.ic2);

            const float wL = bd.gainL * bd.k * v1L;
            const float wR = bd.gainR * bd.k * v1R;
            sumL += wL;
            sumR += wR;

            // Display follows the audible (post-gain) band output.
            const float level = (std::fabs (wL) + std::fabs (wR)) * 0.5f;
            float& env = envelopes_[bs];
            env = level > env ? level : (env * envReleaseC);
        }

        // Dry bleed keeps body at full wet; limiter only touches the bank.
        const float wetL = softLimit (sumL) + kDryBleed * xL;
        const float wetR = softLimit (sumR) + kDryBleed * xR;

        left[i]  = dryGain * xL + wetGain * wetL;
        right[i] = dryGain * xR + wetGain * wetR;
    }

    float maxEnv = peakEnvelope_ * 0.9995f;
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
    return clampf (envelopes_[static_cast<size_t> (band)] / peakEnvelope_, 0.0f, 1.0f);
}

} // namespace sculpt
