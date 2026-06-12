#include "TapePlayer.h"
#include "SampleBuffer.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

void TapePlayer::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
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
        // Gesture just started: latch to latest scrub target (seek must run before this).
        smoothRead_ = scrubTarget_;
        smoothInit_ = true;
        scrubLpPrimed_ = false;
    }
    followScrubTarget_ = shouldFollow;
}

void TapePlayer::setLoopRegion (float start01, float end01)
{
    loopStart_ = clamp01 (start01);
    loopEnd_   = clamp01 (end01);
    if (loopEnd_ < loopStart_ + 0.01f)
        loopEnd_ = clampf (loopStart_ + 0.01f, 0.0f, 1.0f);
}

float TapePlayer::getPositionNormalized (int numFrames) const
{
    if (numFrames <= 1)
        return 0.0f;
    // During live scrub, report the knob target so UI / playhead stays locked to the gesture.
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
}

bool TapePlayer::wrapReadPosition (double& pos, double regionStart, double regionEnd,
                                   double regionLen) noexcept
{
    if (pos >= regionStart && pos < regionEnd)
        return true;
    if (! loopMode_)
    {
        playing_ = false;
        return false;
    }
    if (speed_ >= 0.0f)
        pos = regionStart + std::fmod (pos - regionStart + regionLen, regionLen);
    else
        pos = regionEnd - std::fmod (regionEnd - pos + regionLen, regionLen);
    return true;
}

void TapePlayer::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    const int frames = buffer.getNumFrames();
    if (! playing_ || frames < 2)
        return;

    const double regionStart = static_cast<double> (loopStart_) * (frames - 1);
    const double regionEnd   = static_cast<double> (loopEnd_) * (frames - 1);
    const double regionLen   = regionEnd - regionStart;
    if (regionLen < 2.0)
        return;

    const int rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;

    const bool follow = followScrubTarget_ && playing_;
    // Fast read-head catch-up (responsive knob) + dual LP on output (less zipper / grain).
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
        const double errBlock = scrubTarget_ - smoothRead_;
        const double pullThresh = std::max (320.0, sampleRate_ * 0.0035);
        if (std::fabs (errBlock) > pullThresh)
            smoothRead_ += errBlock * 0.28;
        if (! wrapReadPosition (smoothRead_, regionStart, regionEnd, regionLen))
            return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
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
            // Tighter cap when nearly on target (less buzz); looser when far (snappy knob).
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
            const float rawL = buffer.getSampleLinear (0, pos) * level_;
            const float rawR = buffer.getSampleLinear (rightChannel, pos) * level_;
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
        outL[i] += buffer.getSampleLinear (0, pos) * level_;
        outR[i] += buffer.getSampleLinear (rightChannel, pos) * level_;

        position_ += static_cast<double> (speed_);
    }
}

} // namespace sculpt
