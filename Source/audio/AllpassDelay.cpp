#include <algorithm>
#include <cmath>
#include "AllpassDelay.h"
#include "../util/MathUtils.h"

namespace sculpt
{

// ---------------- DelayLine ----------------

void DelayLine::prepare (int maxLenSamples)
{
    size_  = std::max (4, maxLenSamples + 4);
    buf_.assign (static_cast<size_t> (size_), 0.0f);
    write_ = 0;
    delay_ = std::min (delay_, static_cast<float> (size_ - 2));
}

void DelayLine::reset()
{
    std::fill (buf_.begin(), buf_.end(), 0.0f);
    write_ = 0;
}

void DelayLine::setDelaySamples (float d)
{
    delay_ = std::clamp (d, 1.0f, static_cast<float> (size_ - 2));
}

float DelayLine::readFrac (float samplesBehind) const
{
    const float d = std::clamp (samplesBehind, 1.0f, static_cast<float> (size_ - 1));
    float base = static_cast<float> (write_) - d;
    while (base < 0.0f)
        base += static_cast<float> (size_);

    const int   i0   = static_cast<int> (base) % size_;
    const int   i1   = (i0 + 1) % size_;
    const float frac = base - std::floor (base);
    return buf_[static_cast<size_t> (i0)] + frac * (buf_[static_cast<size_t> (i1)] - buf_[static_cast<size_t> (i0)]);
}

float DelayLine::readAt (float samplesBehind) const
{
    return readFrac (samplesBehind);
}

void DelayLine::write (float x)
{
    buf_[static_cast<size_t> (write_)] = sanitize (x);
    if (++write_ >= size_)
        write_ = 0;
}

float DelayLine::process (float x)
{
    const float y = readFrac (delay_);
    write (x);
    return y;
}

// ---------------- AllpassDelay ----------------

void AllpassDelay::prepare (int maxLenSamples)
{
    size_  = std::max (4, maxLenSamples + 4);
    buf_.assign (static_cast<size_t> (size_), 0.0f);
    write_ = 0;
    delay_ = std::min (delay_, static_cast<float> (size_ - 2));
}

void AllpassDelay::reset()
{
    std::fill (buf_.begin(), buf_.end(), 0.0f);
    write_ = 0;
}

void AllpassDelay::setDelaySamples (float d)
{
    delay_ = std::clamp (d, 1.0f, static_cast<float> (size_ - 2));
}

float AllpassDelay::readFrac (float samplesBehind) const
{
    const float d = std::clamp (samplesBehind, 1.0f, static_cast<float> (size_ - 1));
    float base = static_cast<float> (write_) - d;
    while (base < 0.0f)
        base += static_cast<float> (size_);

    const int   i0   = static_cast<int> (base) % size_;
    const int   i1   = (i0 + 1) % size_;
    const float frac = base - std::floor (base);
    return buf_[static_cast<size_t> (i0)] + frac * (buf_[static_cast<size_t> (i1)] - buf_[static_cast<size_t> (i0)]);
}

float AllpassDelay::readAt (float samplesBehind) const
{
    return readFrac (samplesBehind);
}

float AllpassDelay::processMod (float x, float modSamples)
{
    const float d  = readFrac (delay_ + modSamples); // w[n-k]
    const float wn = x - g_ * d;                      // recursive node
    const float y  = g_ * wn + d;                     // all-pass output

    buf_[static_cast<size_t> (write_)] = sanitize (wn);
    if (++write_ >= size_)
        write_ = 0;
    return y;
}

} // namespace sculpt
