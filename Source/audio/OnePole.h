#pragma once

namespace sculpt
{

// One-pole low-pass (and derived high-pass). Cheap tone shaping used by the
// Color and Space stages. Real-time safe.
class OnePole
{
public:
    void prepare (double sampleRate);
    void reset()                  { z_ = 0.0f; }

    void setCutoffHz (float hz);

    float processLowpass (float in)
    {
        z_ += coeff_ * (in - z_);
        return z_;
    }

    float processHighpass (float in)
    {
        return in - processLowpass (in);
    }

private:
    double sampleRate_ = 44100.0;
    float  coeff_      = 1.0f;
    float  z_          = 0.0f;
};

} // namespace sculpt
