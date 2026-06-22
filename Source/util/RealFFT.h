#pragma once

#include <vector>

namespace sculpt
{

// Portable in-place iterative radix-2 FFT (Cooley-Tukey, decimation-in-time). No JUCE.
// Construct once (allocates twiddles + scratch); forwardMagnitude() does no allocation and is
// real-time safe. Single-threaded use only (shared scratch buffers).
class RealFFT
{
public:
    explicit RealFFT (int order);

    int size() const { return size_; }

    // Magnitude spectrum of `size()` real samples. outMag must hold size()/2 + 1 floats
    // (bins 0..Nyquist). Magnitudes are unnormalized (scale with N and window).
    void forwardMagnitude (const float* timeReal, float* outMag) const;

private:
    int order_ = 0;
    int size_  = 0;

    std::vector<int>   bitRev_;
    std::vector<float> cosT_, sinT_;          // twiddles, size_/2 each
    mutable std::vector<float> re_, im_;      // scratch, size_ each
};

} // namespace sculpt
