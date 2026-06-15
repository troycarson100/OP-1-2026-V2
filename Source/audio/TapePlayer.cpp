#include "TapePlayer.h"
#include "SampleBuffer.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

namespace
{
    // Long enough to hide a large jump when loop in/out moves past the playhead during playback.
    constexpr int kWrapBlendSamples = 384; // ~8.7 ms @ 44.1k

    inline float epCos (float u) { return std::cos (0.5f * kPi * u); }
    inline float epSin (float u) { return std::sin (0.5f * kPi * u); }
} // namespace

void TapePlayer::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    // ~12 ms toward new loop in/out — slower boundary motion while dragging reduces hard jumps.
    loopStartSm_.prepare (sampleRate_, 0.012f);
    loopEndSm_.prepare (sampleRate_, 0.012f);
    loopStartSm_.snap (loopStartTarget_);
    loopEndSm_.snap (loopEndTarget_);
    reset();
}

void TapePlayer::reset()
{
    position_           = 0.0;
    scrubTarget_        = 0.0;
    smoothRead_         = 0.0;
    smoothInit_         = false;
    followScrubTarget_  = false;
    scrubOutLpL_        = 0.0f;
    scrubOutLpR_        = 0.0f;
    scrubOutLp2L_       = 0.0f;
    scrubOutLp2R_       = 0.0f;
    scrubLpPrimed_      = false;
    wrapBlendRemain_    = 0;
    loopStartSm_.snap (loopStartTarget_);
    loopEndSm_.snap (loopEndTarget_);
}

void TapePlayer::setFollowScrubTarget (bool shouldFollow) noexcept
{
    if (followScrubTarget_ && ! shouldFollow)
    {
        position_ = smoothInit_ ? smoothRead_ : position_;
        smoothInit_    = false;
        scrubLpPrimed_ = false;
        scrubOutLpL_   = 0.0f;
        scrubOutLpR_   = 0.0f;
        scrubOutLp2L_  = 0.0f;
        scrubOutLp2R_  = 0.0f;
    }
    if (! followScrubTarget_ && shouldFollow)
    {
        smoothRead_ = scrubTarget_;
        smoothInit_ = true;
        scrubLpPrimed_ = false;
    }
    followScrubTarget_ = shouldFollow;
}

void TapePlayer::setLoopRegion (float start01, float end01)
{
    loopStartTarget_ = clamp01 (start01);
    loopEndTarget_   = clamp01 (end01);
    if (loopEndTarget_ < loopStartTarget_ + 0.01f)
        loopEndTarget_ = clampf (loopStartTarget_ + 0.01f, 0.0f, 1.0f);
    loopStartSm_.setTarget (loopStartTarget_);
    loopEndSm_.setTarget (loopEndTarget_);
}

float TapePlayer::getPositionNormalized (int numFrames) const
{
    if (numFrames <= 1)
        return 0.0f;
    const double pos = (followScrubTarget_ && playing_) ? scrubTarget_ : position_;
    return static_cast<float> (pos / static_cast<double> (numFrames));
}

void TapePlayer::seekNormalized (float position01, int numFrames, float loopStart01, float loopEnd01)
{
    if (numFrames < 2)
    {
        position_    = 0.0;
        scrubTarget_ = 0.0;
        smoothRead_  = 0.0;
        return;
    }

    const double last = static_cast<double> (numFrames - 1);
    const double rs   = static_cast<double> (clamp01 (loopStart01)) * last;
    const double re   = static_cast<double> (clamp01 (loopEnd01)) * last;
    const double len  = re - rs;
    if (len < 1.0)
    {
        position_    = 0.0;
        scrubTarget_ = 0.0;
        smoothRead_  = 0.0;
        return;
    }

    double p = static_cast<double> (clamp01 (position01)) * last;
    if (p < rs)
        p = rs;
    const double maxP = std::max (rs, re - 1.0e-4);
    if (p > maxP)
        p = maxP;

    scrubTarget_ = p;
    if (followScrubTarget_ && playing_)
        return;

    position_   = p;
    smoothRead_ = p;
    // Jumping the playhead: align smoothed loop region immediately (avoids transient mismatch).
    loopStartSm_.snap (loopStartTarget_);
    loopEndSm_.snap (loopEndTarget_);
}

bool TapePlayer::wrapReadPosition (double& pos, double regionStart, double regionEnd,
                                   double regionLen) noexcept
{
    if (regionLen < 2.0)
        return true;

    // Only wrap at the leading edge in the current travel direction. If a loop boundary is
    // dragged past the playhead from the trailing side, keep playing instead of force-jumping
    // (force-jumps every block while dragging are what cause the record-scratch artifact).
    if (speed_ >= 0.0f)
    {
        if (pos >= regionEnd)
        {
            if (! loopMode_)
            {
                playing_ = false;
                return false;
            }
            wrapBlendFromPos_ = pos;                       // old tail continues from here
            wrapBlendRemain_  = kWrapBlendSamples;
            pos = regionStart + std::fmod (pos - regionEnd, regionLen);
        }
    }
    else
    {
        if (pos < regionStart)
        {
            if (! loopMode_)
            {
                playing_ = false;
                return false;
            }
            wrapBlendFromPos_ = pos;
            wrapBlendRemain_  = kWrapBlendSamples;
            pos = regionEnd - std::fmod (regionStart - pos, regionLen);
        }
    }
    return true;
}

void TapePlayer::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    const int frames = buffer.getNumFrames();
    if (! playing_ || frames < 2)
        return;

    const int rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;

    const bool follow = followScrubTarget_ && playing_;
    const double slewCoeff = follow
                                 ? (1.0 - std::exp (-1.0 / (sampleRate_ * 0.0029)))
                                 : 0.0;
    const float scrubLpA = follow
                               ? static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 720.0 / sampleRate_))
                               : 0.0f;
    const float scrubLpB = follow
                               ? static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 480.0 / sampleRate_))
                               : 0.0f;

    if (follow && smoothInit_)
    {
        const double last = static_cast<double> (frames - 1);
        const double regionStart0 = static_cast<double> (loopStartSm_.getCurrent()) * last;
        const double regionEnd0   = static_cast<double> (loopEndSm_.getCurrent()) * last;
        const double regionLen0   = regionEnd0 - regionStart0;
        if (regionLen0 >= 2.0)
        {
            const double errBlock = scrubTarget_ - smoothRead_;
            const double pullThresh = std::max (320.0, sampleRate_ * 0.0035);
            if (std::fabs (errBlock) > pullThresh)
                smoothRead_ += errBlock * 0.28;
            if (! wrapReadPosition (smoothRead_, regionStart0, regionEnd0, regionLen0))
                return;
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const double last = static_cast<double> (frames - 1);
        const float    ls = loopStartSm_.next();
        const float    le = loopEndSm_.next();
        const double regionStart = static_cast<double> (ls) * last;
        const double regionEnd   = static_cast<double> (le) * last;
        const double regionLen   = regionEnd - regionStart;
        if (regionLen < 2.0)
            return;

        if (follow)
        {
            if (! smoothInit_)
            {
                smoothRead_ = position_;
                smoothInit_ = true;
            }
            const double prev = smoothRead_;
            const double err  = scrubTarget_ - prev;
            const double absErr = std::fabs (err);
            const double cap = absErr > 600.0 ? 4.2 : (absErr > 120.0 ? 2.6 : 1.35);
            double       step = err * slewCoeff;
            if (step > cap)
                step = cap;
            else if (step < -cap)
                step = -cap;
            double next = prev + step;
            if (! wrapReadPosition (next, regionStart, regionEnd, regionLen))
                return;
            const float pos = static_cast<float> (0.5 * (prev + next));
            float       rawL = buffer.getSampleLinear (0, pos) * level_;
            float       rawR = buffer.getSampleLinear (rightChannel, pos) * level_;
            if (wrapBlendRemain_ > 0)
            {
                const float u = 1.0f - static_cast<float> (wrapBlendRemain_) / static_cast<float> (kWrapBlendSamples);
                const float c = epCos (u);
                const float s = epSin (u);
                const float fromL =
                    buffer.getSampleLinear (0, static_cast<float> (wrapBlendFromPos_)) * level_;
                const float fromR =
                    buffer.getSampleLinear (rightChannel, static_cast<float> (wrapBlendFromPos_)) * level_;
                rawL = c * fromL + s * rawL;
                rawR = c * fromR + s * rawR;
                --wrapBlendRemain_;
            }
            if (! scrubLpPrimed_)
            {
                scrubOutLpL_   = rawL;
                scrubOutLpR_   = rawR;
                scrubOutLp2L_  = rawL;
                scrubOutLp2R_  = rawR;
                scrubLpPrimed_ = true;
            }
            else
            {
                scrubOutLpL_ += scrubLpA * (rawL - scrubOutLpL_);
                scrubOutLpR_ += scrubLpA * (rawR - scrubOutLpR_);
                scrubOutLp2L_ += scrubLpB * (scrubOutLpL_ - scrubOutLp2L_);
                scrubOutLp2R_ += scrubLpB * (scrubOutLpR_ - scrubOutLp2R_);
            }
            outL[i] += scrubOutLp2L_;
            outR[i] += scrubOutLp2R_;
            smoothRead_ = next;
            position_ = smoothRead_;
            continue;
        }

        if (! wrapReadPosition (position_, regionStart, regionEnd, regionLen))
            return;

        const float pos = static_cast<float> (position_);
        float       rawL = buffer.getSampleLinear (0, pos) * level_;
        float       rawR = buffer.getSampleLinear (rightChannel, pos) * level_;
        if (wrapBlendRemain_ > 0)
        {
            const float u = 1.0f - static_cast<float> (wrapBlendRemain_) / static_cast<float> (kWrapBlendSamples);
            const float c = epCos (u);
            const float s = epSin (u);
            const float fromL =
                buffer.getSampleLinear (0, static_cast<float> (wrapBlendFromPos_)) * level_;
            const float fromR =
                buffer.getSampleLinear (rightChannel, static_cast<float> (wrapBlendFromPos_)) * level_;
            rawL = c * fromL + s * rawL;
            rawR = c * fromR + s * rawR;
            wrapBlendFromPos_ += static_cast<double> (speed_); // old tail keeps moving
            --wrapBlendRemain_;
        }
        outL[i] += rawL;
        outR[i] += rawR;

        position_ += static_cast<double> (speed_);
    }
}

} // namespace sculpt
