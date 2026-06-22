#pragma once

#include "../util/SmoothedValue.h"
#include "TapeGrainScrub.h"

namespace sculpt
{

class SampleBuffer;

// Loop/tape-style varispeed playback over a SampleBuffer.
// Speed is a ratio (negative = reverse). Loop region is normalized 0..1.
// Loop boundaries are smoothed toward targets to avoid zipper noise when
// Loop Start/End move during playback; loop wrap uses a short equal-power
// crossfade to reduce boundary clicks.
// Real-time safe: reads only, no allocation.
class TapePlayer
{
public:
    void prepare (double sampleRate);
    void reset();

    void start()             { playing_ = true; }
    void stop()              { playing_ = false; }
    bool isPlaying() const   { return playing_; }

    void setSpeedRatio (float ratio)   { speed_ = ratio; }
    void setLoopRegion (float start01, float end01);
    void setLoopMode (bool shouldLoop) { loopMode_ = shouldLoop; }
    // Ping-pong looping: reflect at the loop boundaries (flip direction) instead of wrapping.
    // Speed should be set positive; the player alternates direction internally. Off = normal wrap.
    void setLoopBounce (bool shouldBounce) noexcept
    {
        if (shouldBounce && ! loopBounce_)
            bounceDir_ = 1.0f;
        loopBounce_ = shouldBounce;
    }
    void setLevel (float gain)         { level_ = gain; }

    // Loop crossfade lengths in seconds: attack = fade-in of the new loop head, release =
    // fade-out of the old loop tail. They overlap at the wrap to crossfade the seam.
    void setLoopFades (float attackSec, float releaseSec);

    float getPositionNormalized (int numFrames) const;

    // Snap read position to normalized time within the loop region (message / param sync).
    void seekNormalized (float position01, int numFrames, float loopStart01, float loopEnd01);

    // Sampler-style retrigger: jump the read head to position01 within the loop and keep playing,
    // crossfading from the old head to the new one over declickMs (equal-power), so the jump is
    // click-free even while sounding. A longer declick makes a smoother "splice" (Loop Start Follow);
    // a short one keeps trig transients tight. Used by the step sequencer + Loop Start Follow.
    void retrigger (float position01, int numFrames, float loopStart01, float loopEnd01,
                    float declickMs = 4.0f) noexcept;

    // Boundary-drag grain-cloud scrub: on by default. Loop Start Follow turns it OFF so the cloud
    // doesn't smear between the (rate-limited) Follow jumps — the jumps alone do the chop.
    void setBoundaryScrubEnabled (bool enabled) noexcept { boundaryScrubEnabled_ = enabled; }

    // While playing + user scrubs the playhead, follow the target with slew limiting instead
    // of snapping each block (avoids zipper/clicks). Call each parameter update before seekNormalized.
    void setFollowScrubTarget (bool shouldFollow) noexcept;

    // Adds playback into outL/outR.
    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

private:
    bool wrapReadPosition (double& pos, double regionStart, double regionEnd, double regionLen) noexcept;
    // Ping-pong: reflect pos back into the region at each boundary and flip bounceDir_.
    void reflectReadPosition (double& pos, double regionStart, double regionEnd, double regionLen) noexcept;

    double position_  = 0.0;   // in frames
    float  speed_     = 1.0f;
    float  loopStartTarget_ = 0.0f;
    float  loopEndTarget_   = 1.0f;
    float  level_     = 1.0f;
    bool   loopMode_  = true;
    bool   loopBounce_ = false;   // ping-pong: reflect at boundaries instead of wrapping
    float  bounceDir_  = 1.0f;    // current ping-pong direction (+1 forward, -1 backward)
    bool   playing_   = false;

    double sampleRate_ = 44100.0;

    SmoothedValue loopStartSm_;
    SmoothedValue loopEndSm_;

    // While playing + user scrubs the playhead
    bool   followScrubTarget_ = false;
    double scrubTarget_       = 0.0;
    double smoothRead_        = 0.0;
    bool   smoothInit_        = false;

    float  scrubOutLpL_    = 0.0f;
    float  scrubOutLpR_    = 0.0f;
    float  scrubOutLp2L_   = 0.0f;
    float  scrubOutLp2R_   = 0.0f;
    bool   scrubLpPrimed_  = false;

    // Crossfade blend after a wrap. attackSamples_/releaseSamples_ shape the new-head fade-in and
    // old-tail fade-out; wrapBlendLen_ is the active window length when a wrap is triggered.
    int    wrapBlendRemain_ = 0;
    double wrapBlendFromPos_ = 0.0;
    int    attackSamples_  = 384;
    int    releaseSamples_ = 384;
    int    wrapBlendLen_   = 384;
    // Fade-in / fade-out lengths of the currently active blend. Loop-seam wraps use the user fade
    // times; boundary re-trigger jumps override them with a longer equal crossfade (granular wash).
    int    blendFadeIn_    = 384;
    int    blendFadeOut_   = 384;

    // Boundary-drag granular scrub: when Loop Start (forward) / Loop End (reverse) is dragged onto
    // the read head, the head tracks the moving boundary via overlap-add grains at constant pitch
    // instead of jumping (no seam click) or reading the sweep directly (no pitch scrub). scrubMix_
    // crossfades the dry single stream against the grain cloud at engage/disengage edges; the hold
    // counter detects the drag stopping so the cloud hands cleanly back to dry playback.
    // One-shot (loopMode_ == false) amplitude envelope: a short fade-in on retrigger-from-silence
    // and a short fade-out as the head approaches the loop end, so each Sampler step is click-free.
    int oneShotFadeInRemain_ = 0;
    int oneShotFadeInLen_    = 1;
    int oneShotFadeOutLen_   = 1;

    TapeGrainScrub scrub_;
    SmoothedValue  scrubMix_;          // 0 = dry single stream, 1 = grain cloud
    bool           scrubEngaged_ = false;
    bool           boundaryScrubEnabled_ = true; // off during Loop Start Follow (jumps do the chop)
    float          lastRegionStart_ = -1.0e9f; // prev-sample boundary positions (frames), to detect
    float          lastRegionEnd_   = -1.0e9f; // motion of either boundary; sentinel primes "still"
    int            boundaryStillFrames_ = 0;
};

} // namespace sculpt
