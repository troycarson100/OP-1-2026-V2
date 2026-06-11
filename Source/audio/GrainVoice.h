#pragma once

namespace sculpt
{

class SampleBuffer;

// One grain. Started from the pool, renders itself with a Hann window and
// linear-interpolated buffer reads, then deactivates. No allocation.
class GrainVoice
{
public:
    struct StartParams
    {
        float startFrame   = 0.0f;   // fractional frame in the source buffer
        float increment    = 1.0f;   // playback ratio (pitch)
        int   lengthSamples = 0;
        float gainL        = 0.5f;
        float gainR        = 0.5f;
    };

    void start (const StartParams& params);
    void kill()            { active_ = false; }
    bool isActive() const  { return active_; }

    // Fixed spawn position / length for LCD overlay (does not follow playback read head).
    float originStartFrame() const { return originStartFrame_; }
    int   originLength() const     { return originLength_; }
    float phase01() const;

    // Adds the grain into outL/outR. Stops itself when finished.
    void render (const SampleBuffer& buffer, float* outL, float* outR, int numSamples);

private:
    float position_  = 0.0f;
    float increment_ = 1.0f;
    int   age_       = 0;
    int   length_    = 0;
    float gainL_     = 0.0f;
    float gainR_     = 0.0f;
    bool  active_    = false;

    float originStartFrame_ = 0.0f;
    int   originLength_     = 0;
};

} // namespace sculpt
