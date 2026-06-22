#include "SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>
#include "../util/MathUtils.h"

namespace sculpt
{

namespace
{
    constexpr float kFreqLo = 20.0f;
    constexpr float kFreqHi = 20000.0f;
    constexpr float kDbFloor = -90.0f;   // bottom of the display
}

void SpectrumAnalyzer::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Hann window.
    for (int i = 0; i < kFftSize; ++i)
        window_[static_cast<size_t> (i)] =
            0.5f - 0.5f * std::cos (kTwoPi * static_cast<float> (i) / static_cast<float> (kFftSize - 1));

    // Log-spaced display-bin edges, mapped to FFT bin indices.
    const float hiClamped = std::min (kFreqHi, static_cast<float> (sampleRate_) * 0.5f);
    for (int b = 0; b <= kNumBins; ++b)
    {
        const float t  = static_cast<float> (b) / static_cast<float> (kNumBins);
        const float hz = kFreqLo * std::pow (hiClamped / kFreqLo, t);
        int idx = static_cast<int> (hz * static_cast<float> (kFftSize) / static_cast<float> (sampleRate_) + 0.5f);
        if (idx < 1)            idx = 1;
        if (idx > kFftSize / 2) idx = kFftSize / 2;
        binEdge_[static_cast<size_t> (b)] = idx;
    }

    reset();
}

void SpectrumAnalyzer::reset()
{
    ring_.fill (0.0f);
    display_.fill (0.0f);
    writePos_   = 0;
    sinceFrame_ = 0;
}

void SpectrumAnalyzer::push (const float* mono, int n)
{
    for (int i = 0; i < n; ++i)
    {
        ring_[static_cast<size_t> (writePos_)] = mono[i];
        writePos_ = (writePos_ + 1) & (kFftSize - 1);
        if (++sinceFrame_ >= kHop)
        {
            sinceFrame_ = 0;
            computeFrame();
        }
    }
}

void SpectrumAnalyzer::computeFrame()
{
    // ring_ is exactly kFftSize long, so it always holds the last kFftSize samples; writePos_ points
    // at the oldest. Window them (oldest first) into a stack frame (4 KB, no heap).
    std::array<float, kFftSize> frame;
    for (int j = 0; j < kFftSize; ++j)
        frame[static_cast<size_t> (j)] =
            ring_[static_cast<size_t> ((writePos_ + j) & (kFftSize - 1))] * window_[static_cast<size_t> (j)];

    fft_.forwardMagnitude (frame.data(), mag_.data());

    // Normalize so a full-scale tone reads near 0 dB, then fold FFT bins into log display bins
    // (take the max in each band for a lively, peak-following look).
    const float norm = 2.0f / static_cast<float> (kFftSize);

    for (int b = 0; b < kNumBins; ++b)
    {
        int lo = binEdge_[static_cast<size_t> (b)];
        int hi = binEdge_[static_cast<size_t> (b + 1)];
        if (hi <= lo) hi = lo + 1;

        float peak = 0.0f;
        for (int k = lo; k < hi; ++k)
            peak = std::max (peak, mag_[static_cast<size_t> (k)]);

        const float db   = 20.0f * std::log10 (peak * norm + 1.0e-9f);
        const float targ = clamp01 ((db - kDbFloor) / (-kDbFloor));   // kDbFloor..0 dB -> 0..1

        // Fast attack, slow decay (per ~hop frame) for an analyzer feel.
        float& d = display_[static_cast<size_t> (b)];
        d = targ > d ? targ : d * 0.82f + targ * 0.18f;
    }
}

} // namespace sculpt
