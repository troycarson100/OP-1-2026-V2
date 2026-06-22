#pragma once

#include <array>
#include "../util/RealFFT.h"

namespace sculpt
{

// Real-time-safe log-spaced spectrum analyzer (raw C++, no JUCE). The audio thread feeds mono
// samples via push(); every hop it windows the most recent FFT frame, transforms it, folds the
// magnitude into log-spaced display bins (~20 Hz .. 20 kHz) in dB, and peak-decays them to 0..1.
// The UI just reads bins() and draws them. Fixed-size members; no allocation after prepare().
class SpectrumAnalyzer
{
public:
    static constexpr int kFftOrder = 10;            // 1024-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kHop      = kFftSize / 2;   // 50% overlap
    static constexpr int kNumBins  = 96;             // log-spaced display bins

    void prepare (double sampleRate);
    void reset();

    // Audio thread: push n mono samples.
    void push (const float* mono, int n);

    const float* bins() const { return display_.data(); }   // kNumBins, normalized 0..1
    static constexpr int numBins() { return kNumBins; }

private:
    void computeFrame();

    double sampleRate_ = 44100.0;
    RealFFT fft_ { kFftOrder };

    std::array<float, kFftSize>        ring_ {};
    int                                writePos_   = 0;
    int                                sinceFrame_ = 0;

    std::array<float, kFftSize>            window_ {};       // Hann
    std::array<float, kFftSize / 2 + 1>    mag_ {};          // FFT magnitude scratch
    std::array<int,   kNumBins + 1>        binEdge_ {};      // FFT-bin index per display-bin edge
    std::array<float, kNumBins>            display_ {};      // smoothed 0..1
};

} // namespace sculpt
