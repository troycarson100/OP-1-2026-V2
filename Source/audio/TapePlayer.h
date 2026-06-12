#pragma once

namespace sculpt
{

class SampleBuffer;

// Loop/tape-style varispeed playback over a SampleBuffer.
// Speed is a ratio (negative = reverse). Loop region is normalized 0..1.
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
    float  loopStart_ = 0.0f;
    float  loopEnd_   = 1.0f;
    float  level_     = 1.0f;
    bool   loopMode_  = true;
    bool   playing_   = false;

    double sampleRate_ = 44100.0;

    // Scrub-follow playback: smooth read head toward scrubTarget_ (no per-sample speed advance).
    bool   followScrubTarget_ = false;
    double scrubTarget_       = 0.0;
    double smoothRead_        = 0.0;
    bool   smoothInit_        = false;

    // Two-stage lowpass on scrubbed tape out: fast playhead + darker output (less zipper / grain).
    float  scrubOutLpL_    = 0.0f;
    float  scrubOutLpR_    = 0.0f;
    float  scrubOutLp2L_   = 0.0f;
    float  scrubOutLp2R_   = 0.0f;
    bool   scrubLpPrimed_  = false;
};

} // namespace sculpt
