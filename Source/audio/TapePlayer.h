#pragma once

#include "../util/SmoothedValue.h"

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
    void setLevel (float gain)         { level_ = gain; }

    // Loop crossfade lengths in seconds: attack = fade-in of the new loop head, release =
    // fade-out of the old loop tail. They overlap at the wrap to crossfade the seam.
    void setLoopFades (float attackSec, float releaseSec);

    float getPositionNormalized (int numFrames) const;

    // Snap read position to normalized time within the loop region (message / param sync).
    void seekNormalized (float position01, int numFrames, float loopStart01, float loopEnd01);

    // While playing + user scrubs the playhead, follow the target with slew limiting instead
    // of snapping each block (avoids zipper/clicks). Call each parameter update before seekNormalized.
    void setFollowScrubTarget (bool shouldFollow) noexcept;

    // Adds playback into outL/outR.
    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

private:
    bool wrapReadPosition (double& pos, double regionStart, double regionEnd, double regionLen) noexcept;

    double position_  = 0.0;   // in frames
    float  speed_     = 1.0f;
    float  loopStartTarget_ = 0.0f;
    float  loopEndTarget_   = 1.0f;
    float  level_     = 1.0f;
    bool   loopMode_  = true;
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
    // Samples until the next boundary re-trigger jump is allowed (rate limit). The crossfade spans
    // this whole interval so consecutive jumps blend continuously instead of clicking.
    int    boundaryJumpCooldown_ = 0;
};

} // namespace sculpt
