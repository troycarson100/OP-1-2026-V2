#include "RealFFT.h"

#include <cmath>

namespace sculpt
{

RealFFT::RealFFT (int order)
    : order_ (order < 1 ? 1 : order),
      size_  (1 << (order < 1 ? 1 : order))
{
    bitRev_.resize (static_cast<size_t> (size_));
    for (int i = 0; i < size_; ++i)
    {
        int x = i, r = 0;
        for (int b = 0; b < order_; ++b) { r = (r << 1) | (x & 1); x >>= 1; }
        bitRev_[static_cast<size_t> (i)] = r;
    }

    const double pi = 3.14159265358979323846;
    cosT_.resize (static_cast<size_t> (size_ / 2));
    sinT_.resize (static_cast<size_t> (size_ / 2));
    for (int i = 0; i < size_ / 2; ++i)
    {
        const double a = -2.0 * pi * static_cast<double> (i) / static_cast<double> (size_);
        cosT_[static_cast<size_t> (i)] = static_cast<float> (std::cos (a));
        sinT_[static_cast<size_t> (i)] = static_cast<float> (std::sin (a));
    }

    re_.resize (static_cast<size_t> (size_));
    im_.resize (static_cast<size_t> (size_));
}

void RealFFT::forwardMagnitude (const float* x, float* outMag) const
{
    const int N = size_;

    // Bit-reversal permutation into the complex scratch (imag = 0 for real input).
    for (int i = 0; i < N; ++i)
    {
        re_[static_cast<size_t> (i)] = x[bitRev_[static_cast<size_t> (i)]];
        im_[static_cast<size_t> (i)] = 0.0f;
    }

    // Butterflies, doubling the transform length each stage.
    for (int len = 2; len <= N; len <<= 1)
    {
        const int half = len >> 1;
        const int step = N / len;
        for (int i = 0; i < N; i += len)
        {
            int tw = 0;
            for (int j = 0; j < half; ++j)
            {
                const float c = cosT_[static_cast<size_t> (tw)];
                const float s = sinT_[static_cast<size_t> (tw)];
                const auto a = static_cast<size_t> (i + j);
                const auto b = static_cast<size_t> (i + j + half);
                const float tr = re_[b] * c - im_[b] * s;
                const float ti = re_[b] * s + im_[b] * c;
                re_[b] = re_[a] - tr;
                im_[b] = im_[a] - ti;
                re_[a] += tr;
                im_[a] += ti;
                tw += step;
            }
        }
    }

    for (int i = 0; i <= N / 2; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        outMag[i] = std::sqrt (re_[idx] * re_[idx] + im_[idx] * im_[idx]);
    }
}

} // namespace sculpt
