#include "TapePlayer.h"
#include "SampleBuffer.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

namespace
{
    // Loop-seam crossfade gains. `elapsed` = samples since the wrap; the new loop head fades in
    // over `attackS` (raised cosine 0->1), the old tail fades out over `releaseS` (1->0). When
    // attack == release this is an amplitude-constant crossfade.
    inline void blendGains (int elapsed, int attackS, int releaseS, float& gIn, float& gOut)
    {
        const float pin  = std::min (1.0f, static_cast<float> (elapsed) / static_cast<float> (std::max (1, attackS)));
        const float pout = std::min (1.0f, static_cast<float> (elapsed) / static_cast<float> (std::max (1, releaseS)));
        gIn  = 0.5f - 0.5f * std::cos (kPi * pin);
        gOut = 0.5f + 0.5f * std::cos (kPi * pout);
    }

    // Upper bound for the "boundary still" frame counter (saturates here; only compared against the
    // disengage hold time, so it just needs to exceed it).
    constexpr int kBoundaryHoldFramesMax = 1 << 20;
} // namespace

void TapePlayer::setLoopFades (float attackSec, float releaseSec)
{
    const double sr   = sampleRate_ > 0.0 ? sampleRate_ : 44100.0;
    const int    maxS = std::max (8, static_cast<int> (0.30 * sr)); // cap ~300 ms
    attackSamples_  = std::clamp (static_cast<int> (std::lround (static_cast<double> (attackSec)  * sr)), 8, maxS);
    releaseSamples_ = std::clamp (static_cast<int> (std::lround (static_cast<double> (releaseSec) * sr)), 8, maxS);
}

void TapePlayer::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    // ~12 ms toward new loop in/out — slower boundary motion while dragging reduces hard jumps.
    loopStartSm_.prepare (sampleRate_, 0.012f);
    loopEndSm_.prepare (sampleRate_, 0.012f);
    loopStartSm_.snap (loopStartTarget_);
    loopEndSm_.snap (loopEndTarget_);
    scrub_.prepare (sampleRate_);
    scrubMix_.prepare (sampleRate_, 0.025f); // ~25 ms engage/disengage crossfade (matches cloud build-up)
    scrubMix_.snap (0.0f);
    oneShotFadeInLen_  = std::max (8, static_cast<int> (sampleRate_ * 0.003)); // ~3 ms
    oneShotFadeOutLen_ = std::max (8, static_cast<int> (sampleRate_ * 0.006)); // ~6 ms
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
    oneShotFadeInRemain_ = 0;
    scrub_.reset();
    scrubMix_.snap (0.0f);
    scrubEngaged_        = false;
    lastRegionStart_     = -1.0e9f; // sentinel: first block primed as "boundaries still"
    lastRegionEnd_       = -1.0e9f;
    boundaryStillFrames_ = 0;
    bounceDir_           = 1.0f;
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
    constexpr float kMinLoop01 = 0.01f;
    loopStartTarget_ = clamp01 (start01);
    loopEndTarget_   = clamp01 (end01);
    // Guarantee a minimum loop length, adjusting BOTH bounds. The old code only clamped the end UP,
    // which collapsed to zero when Loop Start was modulated to the very top of the sample (end can't
    // exceed 1.0, so start==1.0 left start==end). A zero-length region makes regionLen < 2 every
    // sample, so process() skips the tape output entirely -> silent dropout until a re-seek. When
    // there's no room for the gap below 1.0, push the start down instead.
    if (loopEndTarget_ - loopStartTarget_ < kMinLoop01)
    {
        if (loopStartTarget_ + kMinLoop01 <= 1.0f)
            loopEndTarget_ = loopStartTarget_ + kMinLoop01;
        else
        {
            loopEndTarget_   = 1.0f;
            loopStartTarget_ = 1.0f - kMinLoop01;
        }
    }

    // Smooth BOTH boundaries at the same rate. Because they share a coefficient and the targets keep
    // end >= start + kMinLoop01, the smoothed currents can never cross — so the loop length stays
    // positive even when a tiny loop window is swept quickly. (The old code snapped the start while the
    // end lagged, which let a small swept window momentarily collapse the region -> crackle/dropouts.
    // The start-snap only existed to land the now-removed re-trigger jump exactly on the knob.)
    loopStartSm_.setTarget (loopStartTarget_);
    loopEndSm_.setTarget (loopEndTarget_);
}

float TapePlayer::getPositionNormalized (int numFrames) const
{
    if (numFrames <= 1)
        return 0.0f;
    // While the grain cloud is tracking a dragged boundary, the dry read head lags behind; report
    // the boundary instead so the visible playhead stays glued to Loop Start/End.
    if (scrubEngaged_)
        return speed_ >= 0.0f ? loopStartSm_.getCurrent() : loopEndSm_.getCurrent();
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

void TapePlayer::retrigger (float position01, int numFrames, float loopStart01, float loopEnd01,
                            float declickMs) noexcept
{
    if (numFrames < 2)
        return;

    const double last = static_cast<double> (numFrames - 1);
    const double rs   = static_cast<double> (clamp01 (loopStart01)) * last;
    const double re   = static_cast<double> (clamp01 (loopEnd01)) * last;
    if (re - rs < 1.0)
        return;

    double target = static_cast<double> (clamp01 (position01)) * last;
    if (target < rs)
        target = rs;
    const double maxP = std::max (rs, re - 1.0e-4);
    if (target > maxP)
        target = maxP;

    // If already sounding, crossfade from the current head to the new one over a short, fixed declick
    // window (independent of the loop crossfade so it never collapses to a click). The process loop's
    // wrap-blend reads the old tail (fading out) against the new head (fading in).
    const bool wasPlaying = playing_ && ! followScrubTarget_ && ! scrubEngaged_;
    if (wasPlaying)
    {
        const double declickSec = std::max (0.001, static_cast<double> (declickMs) * 0.001);
        const int fade    = std::max (32, static_cast<int> (sampleRate_ * declickSec));
        wrapBlendFromPos_ = position_;
        wrapBlendLen_     = fade;
        blendFadeIn_      = fade;
        blendFadeOut_     = fade;
        wrapBlendRemain_  = fade;
        oneShotFadeInRemain_ = 0; // crossfade already eases the new head in
    }
    else
    {
        // Starting from silence: ramp the one-shot in so the jump to a non-zero sample doesn't click.
        oneShotFadeInRemain_ = oneShotFadeInLen_;
    }

    position_    = target;
    smoothRead_  = target;
    scrubTarget_ = target;
    smoothInit_  = false;
    loopStartSm_.snap (loopStartTarget_);
    loopEndSm_.snap (loopEndTarget_);

    // A deliberate jump (sampler trig / Loop Start Follow) is not a boundary sweep: reset the
    // boundary-scrub tracker to the new region and disengage any active grain cloud, so the cloud
    // doesn't smear/slow the audio after the jump. Without this, a moving Loop Start engages the
    // sweep cloud and the jump lands on top of it (the "glitchy slowdown").
    constexpr int kBoundaryHoldFramesMax = 1 << 20;
    lastRegionStart_     = static_cast<float> (rs);
    lastRegionEnd_       = static_cast<float> (re);
    boundaryStillFrames_ = kBoundaryHoldFramesMax;
    bounceDir_           = 1.0f;   // a fresh trig starts ping-pong going forward
    if (scrubEngaged_)
    {
        scrubEngaged_ = false;
        scrub_.disengage();
    }
    scrubMix_.snap (0.0f);

    playing_ = true;
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
            wrapBlendLen_     = std::min (std::max (attackSamples_, releaseSamples_),
                                          std::max (1, static_cast<int> (regionLen * 0.5)));
            wrapBlendRemain_  = wrapBlendLen_;
            blendFadeIn_      = attackSamples_;
            blendFadeOut_     = releaseSamples_;
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
            wrapBlendLen_     = std::min (std::max (attackSamples_, releaseSamples_),
                                          std::max (1, static_cast<int> (regionLen * 0.5)));
            wrapBlendRemain_  = wrapBlendLen_;
            blendFadeIn_      = attackSamples_;
            blendFadeOut_     = releaseSamples_;
            pos = regionEnd - std::fmod (regionStart - pos, regionLen);
        }
    }
    return true;
}

void TapePlayer::reflectReadPosition (double& pos, double regionStart, double regionEnd, double regionLen) noexcept
{
    if (regionLen < 2.0)
        return;
    // Bounce between the boundaries (ping-pong). Set the direction explicitly per boundary so an
    // overshoot lands consistently; the loop guards against pathological speeds.
    for (int guard = 0; guard < 8; ++guard)
    {
        if (pos > regionEnd)
        {
            pos        = regionEnd - (pos - regionEnd);
            bounceDir_ = -1.0f;   // now travelling backward
        }
        else if (pos < regionStart)
        {
            pos        = regionStart + (regionStart - pos);
            bounceDir_ = 1.0f;    // now travelling forward
        }
        else
        {
            break;
        }
    }
}

void TapePlayer::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    const int frames = buffer.getNumFrames();
    if (! playing_ || frames < 2)
        return;

    const int rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;

    const bool follow = followScrubTarget_ && playing_;
    // Slew + low-pass coefficients for the smoothed "scrub" chase. Used both for an external
    // playhead scrub (follow) and when a loop boundary is dragged onto the read head — see loop.
    const double slewCoeff = 1.0 - std::exp (-1.0 / (sampleRate_ * 0.0029));
    const float  scrubLpA  = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 720.0 / sampleRate_));
    const float  scrubLpB  = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 480.0 / sampleRate_));

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
            continue; // degenerate region for this sample only — don't silence the whole block

        // External playhead scrub: slew the read head toward scrubTarget_ with a low-pass so manual
        // scrubbing sounds smooth. Loop-boundary follow is handled separately below (as a jump).
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
                float gIn = 1.0f, gOut = 0.0f;
                blendGains (wrapBlendLen_ - wrapBlendRemain_, blendFadeIn_, blendFadeOut_, gIn, gOut);
                const float fromL =
                    buffer.getSampleLinear (0, static_cast<float> (wrapBlendFromPos_)) * level_;
                const float fromR =
                    buffer.getSampleLinear (rightChannel, static_cast<float> (wrapBlendFromPos_)) * level_;
                rawL = gOut * fromL + gIn * rawL;
                rawR = gOut * fromR + gIn * rawR;
                wrapBlendFromPos_ += static_cast<double> (speed_);
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

        // Manual scrub just ended — drop its state so it re-primes cleanly next time.
        if (smoothInit_)
        {
            smoothInit_    = false;
            scrubLpPrimed_ = false;
        }

        // A loop boundary swept onto the read head (Loop Start up onto it, or Loop End down onto it;
        // mirrored in reverse), or a small loop window swept by automation. Either case is tracked by
        // an overlap-add grain cloud (constant pitch, no seam click) crossfaded against the dry
        // stream, instead of the single-stream wrap which clicks when a boundary moves across it.
        const float rsF = static_cast<float> (regionStart);
        const float reF = static_cast<float> (regionEnd);
        const bool  primed = lastRegionStart_ > -1.0e8f;

        // Either boundary moving counts as "the loop is being swept" (gates engage + settle timer).
        const bool startMovedUp = primed && rsF > lastRegionStart_ + 0.5f;
        const bool endMovedDown = primed && reF < lastRegionEnd_   - 0.5f;
        const bool moved        = primed && (std::fabs (rsF - lastRegionStart_) > 0.5f
                                          || std::fabs (reF - lastRegionEnd_)   > 0.5f);
        if (! primed)
            boundaryStillFrames_ = kBoundaryHoldFramesMax; // first block: treat as already still
        else if (moved)
            boundaryStillFrames_ = 0;
        else if (boundaryStillFrames_ < kBoundaryHoldFramesMax)
            ++boundaryStillFrames_;
        lastRegionStart_ = rsF;
        lastRegionEnd_   = reF;

        const double deadband  = 2.0;
        const double smallLoop = sampleRate_ * 0.120; // loops shorter than ~120 ms render granular when swept
        const bool   smallRegion  = regionLen < smallLoop;
        const bool   headBelowStart = position_ < regionStart - deadband;
        const bool   headAboveEnd   = position_ > regionEnd   + deadband;
        // Overtake = a boundary moved *toward* the head and now sits past it (distinct from a normal
        // wrap, where the boundary did not move onto the head).
        const bool   overtake = (headBelowStart && startMovedUp) || (headAboveEnd && endMovedDown);
        const double boundary = (speed_ >= 0.0f) ? regionStart : regionEnd;
        const int    holdFrames = std::max (8, static_cast<int> (sampleRate_ * 0.060)); // 60 ms still

        const int grainLen = std::min (scrub_.defaultSpawnLength(),
                                       std::max (16, static_cast<int> (regionLen)));

        if (! scrubEngaged_)
        {
            // Engage only while a boundary is actively sweeping (not static), and when it has either
            // overtaken the head or the loop is small enough that single-stream looping would crackle.
            // Static and normal-size loops with the head inside never smear. Disabled entirely during
            // Loop Start Follow, where the rate-limited jumps do the chopping instead.
            if (boundaryScrubEnabled_ && boundaryStillFrames_ == 0 && (overtake || smallRegion))
            {
                scrub_.setSpawnLength (grainLen); // small loop -> loop tone; larger -> scrub smear
                scrubEngaged_ = true;
                scrub_.engage();
                scrubMix_.setTarget (1.0f);
            }
        }
        else
        {
            scrub_.setSpawnLength (grainLen); // keep grains matched to the evolving loop length
            if (boundaryStillFrames_ >= holdFrames || ! boundaryScrubEnabled_)
            {
                // Boundary settled — hand back to the dry single stream. Always glide the dry head
                // onto the boundary (where the cloud's newest grains are) through the seam blend, so
                // the cloud->dry crossfade is correlated (no dip) and the position jump has no pop —
                // the dry stream is only ~90% ducked here, so an instant snap would click. Disengaging
                // only once the boundary settles (never mid-sweep) avoids the flip-flop.
                wrapBlendFromPos_ = position_;
                wrapBlendLen_     = std::max (attackSamples_, releaseSamples_);
                blendFadeIn_      = attackSamples_;
                blendFadeOut_     = releaseSamples_;
                wrapBlendRemain_  = wrapBlendLen_;
                position_         = boundary;
                scrubEngaged_     = false;
                scrub_.disengage();
                scrubMix_.setTarget (0.0f);
            }
        }

        // While the grain cloud is engaged it is the audio source, so FREEZE the dry single stream:
        // don't advance or wrap it. A swept tiny loop would otherwise wrap with a few-sample crossfade
        // hundreds of times a second, and those clicks leak through the (never-quite-100%) duck as
        // crackle. The dry stream only needs to exist for the engage/disengage crossfades; disengage
        // snaps it back onto the boundary, so freezing here is safe.
        if (! scrubEngaged_)
        {
            if (loopBounce_)
                reflectReadPosition (position_, regionStart, regionEnd, regionLen);
            else if (! wrapReadPosition (position_, regionStart, regionEnd, regionLen))
                return;
        }

        const float pos = static_cast<float> (position_);
        float       rawL = buffer.getSampleLinear (0, pos) * level_;
        float       rawR = buffer.getSampleLinear (rightChannel, pos) * level_;
        if (wrapBlendRemain_ > 0)
        {
            float gIn = 1.0f, gOut = 0.0f;
            blendGains (wrapBlendLen_ - wrapBlendRemain_, blendFadeIn_, blendFadeOut_, gIn, gOut);
            const float fromL =
                buffer.getSampleLinear (0, static_cast<float> (wrapBlendFromPos_)) * level_;
            const float fromR =
                buffer.getSampleLinear (rightChannel, static_cast<float> (wrapBlendFromPos_)) * level_;
            rawL = gOut * fromL + gIn * rawL;
            rawR = gOut * fromR + gIn * rawR;
            wrapBlendFromPos_ += static_cast<double> (speed_); // old tail keeps moving
            --wrapBlendRemain_;
        }

        // One-shot amplitude envelope: short fade-in after a from-silence retrigger and a short
        // fade-out as the head nears the loop end, so the non-looping voice never clicks at the seam.
        if (! loopMode_)
        {
            float amp = 1.0f;
            if (oneShotFadeInRemain_ > 0)
            {
                amp = 1.0f - static_cast<float> (oneShotFadeInRemain_) / static_cast<float> (oneShotFadeInLen_);
                --oneShotFadeInRemain_;
            }
            const double spd   = std::max (1.0e-6, static_cast<double> (std::fabs (speed_)));
            const double toEnd = (speed_ >= 0.0f) ? (regionEnd - position_) / spd
                                                  : (position_ - regionStart) / spd;
            if (toEnd < static_cast<double> (oneShotFadeOutLen_))
                amp *= static_cast<float> (std::max (0.0, toEnd / static_cast<double> (oneShotFadeOutLen_)));
            rawL *= amp;
            rawR *= amp;
        }

        // Crossfade dry single stream against the boundary-tracking grain cloud.
        float grainL = 0.0f, grainR = 0.0f;
        scrub_.render (buffer, boundary, static_cast<double> (speed_), level_, rightChannel, grainL, grainR);
        const float mix = scrubMix_.next();
        outL[i] += rawL * (1.0f - mix) + grainL * mix;
        outR[i] += rawR * (1.0f - mix) + grainR * mix;

        if (! scrubEngaged_)
            position_ += static_cast<double> (speed_) * (loopBounce_ ? static_cast<double> (bounceDir_) : 1.0);
    }
}

} // namespace sculpt
