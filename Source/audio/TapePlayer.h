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

    // Adds playback into outL/outR.
    void process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

private:
    double position_  = 0.0;   // in frames
    float  speed_     = 1.0f;
    float  loopStart_ = 0.0f;
    float  loopEnd_   = 1.0f;
    float  level_     = 1.0f;
    bool   loopMode_  = true;
    bool   playing_   = false;
};

} // namespace sculpt
