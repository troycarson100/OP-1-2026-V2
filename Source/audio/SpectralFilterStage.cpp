#include <cmath>
#include "SpectralFilterStage.h"
#include "../core/ParameterIds.h"
#include "../core/FilterScales.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void SpectralFilterStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    mix_.prepare (sampleRate_, 0.02f);
    mix_.snap (0.0f);
    reset();
}

void SpectralFilterStage::reset()
{
    leftStates_.fill ({});
    rightStates_.fill ({});
    envelopes_.fill (0.0f);
    lastCutoff_ = lastRes_ = lastDecay_ = lastPitch_ = lastScale_ = -1.0f;
    numActiveBands_ = 0;
}

void SpectralFilterStage::setParams (float cutoff01, float resonance01, float decay01,
                                     float pitch01,  float scaleIdx01, float mix01)
{
    mix_.setTarget (mix01);

    const bool dirty = (cutoff01 != lastCutoff_) || (resonance01 != lastRes_)
                    || (decay01  != lastDecay_)  || (pitch01 != lastPitch_)
                    || (scaleIdx01 != lastScale_);
    if (! dirty)
        return;

    lastCutoff_ = cutoff01;
    lastRes_    = resonance01;
    lastDecay_  = decay01;
    lastPitch_  = pitch01;
    lastScale_  = scaleIdx01;

    rebuildCoeffs();
}

void SpectralFilterStage::rebuildCoeffs()
{
    const float rootHz   = map::filterCutoffHz (lastCutoff_);
    const float pitchSt  = map::filterPitchSemitones (lastPitch_);
    const float shiftedRootHz = rootHz * std::pow (2.0f, pitchSt / 12.0f);
    const float r        = map::spectralPoleRadius (lastDecay_);
    const float r2       = r * r;
    const float nyqLimit = static_cast<float> (sampleRate_) * 0.475f;
    const FilterScale scale = normalizedToFilterScale (lastScale_);

    // Convert shiftedRootHz to MIDI for scale snapping reference.
    const float rootMidi     = 69.0f + 12.0f * std::log2 (shiftedRootHz / 440.0f);
    const int   rootMidiInt  = static_cast<int> (std::round (rootMidi));

    numActiveBands_ = 0;

    for (int b = 0; b < kNumBands; ++b)
    {
        const float rawMidi     = rootMidi + static_cast<float> (b * kSemisPerBand);
        const float snappedMidi = snapToScaleMidi (rawMidi, rootMidiInt, scale);
        const float freqHz      = 440.0f * std::pow (2.0f, (snappedMidi - 69.0f) / 12.0f);

        if (freqHz < 20.0f || freqHz > nyqLimit)
        {
            coeffs_[static_cast<size_t> (b)].active = false;
            continue;
        }

        const float w0  = kTwoPi * freqHz / static_cast<float> (sampleRate_);
        auto& c = coeffs_[static_cast<size_t> (b)];
        c.b0     = (1.0f - r2) * 0.5f;
        c.a1     = -2.0f * r * std::cos (w0);
        c.a2     = r2;
        c.active = true;
        ++numActiveBands_;
    }
}

float SpectralFilterStage::processBandSample (ChannelState& s, const BandCoeffs& c, float x) const
{
    // Direct Form II resonator.
    const float w  = x - c.a1 * s.w1 - c.a2 * s.w2;
    const float y  = c.b0 * (w - s.w2);   // BPF: b0*w[n] + 0*w[n-1] - b0*w[n-2]
    s.w2 = sanitize (s.w1);
    s.w1 = sanitize (w);
    return y;
}

void SpectralFilterStage::process (float* left, float* right, int numSamples)
{
    if (numActiveBands_ == 0)
        return;

    const float mixTarget = mix_.skip (numSamples);
    if (mixTarget < 1.0e-5f)
        return;

    const float invBands  = 1.0f / static_cast<float> (numActiveBands_);
    // Envelope follower coefficient — fast attack, decay tied to lastDecay_.
    const float envRelease = 0.9f + lastDecay_ * 0.099f;

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
            sumL += yL;
            sumR += yR;

            // Envelope follower for LCD visualization.
            const float level = (std::fabs (yL) + std::fabs (yR)) * 0.5f;
            float& env = envelopes_[bs];
            env = level > env ? level : (env * envRelease);
        }

        const float mix = mix_.next();
        left[i]  = lerp (xL, sumL * invBands, mix);
        right[i] = lerp (xR, sumR * invBands, mix);
    }
}

float SpectralFilterStage::getBandEnvelope (int band) const
{
    if (band < 0 || band >= kNumBands)
        return 0.0f;
    return envelopes_[static_cast<size_t> (band)];
}

} // namespace sculpt
