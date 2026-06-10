#pragma once

namespace sculpt
{

// Simple linear attack/release gate envelope. Used for click-free track
// trigger/stop. Real-time safe.
class Envelope
{
public:
    void prepare (double sampleRate);
    void setTimesMs (float attackMs, float releaseMs);

    void gateOn()   { gate_ = true; }
    void gateOff()  { gate_ = false; }

    bool isActive() const { return gate_ || level_ > 0.0001f; }

    float next()
    {
        if (gate_)
        {
            level_ += attackInc_;
            if (level_ > 1.0f) level_ = 1.0f;
        }
        else
        {
            level_ -= releaseInc_;
            if (level_ < 0.0f) level_ = 0.0f;
        }
        return level_;
    }

    float getLevel() const { return level_; }

private:
    double sampleRate_ = 44100.0;
    float  attackInc_  = 0.001f;
    float  releaseInc_ = 0.001f;
    float  level_      = 0.0f;
    bool   gate_       = false;
};

} // namespace sculpt
