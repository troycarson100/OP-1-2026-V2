#pragma once

#include <cmath>
#include <limits>

namespace sculpt
{

// Click metronome: a short decaying sine blip on every beat, accented on the bar downbeat.
// Driven by the transport beat so the accent lands on the downbeat. Portable, RT-safe, no alloc.
class Metronome
{
public:
    void prepare (double sampleRate)
    {
        sr_ = sampleRate > 1.0 ? sampleRate : 44100.0;
        decayPerSample_ = static_cast<float> (std::exp (-1.0 / (0.030 * sr_))); // ~30 ms blip
        reset();
    }

    void reset()
    {
        env_      = 0.0f;
        phase_    = 0.0f;
        lastBeat_ = std::numeric_limits<long long>::min();
    }

    void setEnabled (bool e) { enabled_ = e; }
    bool isEnabled() const   { return enabled_; }

    // Adds clicks into outL/outR for this block. beatAtBlockStart is the transport beat at the first
    // sample; samplesPerBeat sets the tempo. Triggers a blip whenever an integer beat is crossed.
    void process (float* outL, float* outR, int numSamples, double beatAtBlockStart, double samplesPerBeat)
    {
        if (outL == nullptr || numSamples <= 0 || samplesPerBeat <= 1.0)
            return;

        const double inc  = 1.0 / samplesPerBeat;
        double       beat = beatAtBlockStart;

        for (int i = 0; i < numSamples; ++i)
        {
            const long long fb = static_cast<long long> (std::floor (beat));
            if (fb != lastBeat_)
            {
                lastBeat_ = fb; // track even when disabled so re-enabling doesn't double-fire
                if (enabled_)
                {
                    env_   = 1.0f;
                    phase_ = 0.0f;
                    const bool accent = (((fb % beatsPerBar_) + beatsPerBar_) % beatsPerBar_) == 0;
                    freq_  = accent ? 1568.0f : 988.0f; // accented downbeat vs regular beat
                }
            }

            if (env_ > 1.0e-4f)
            {
                const float s = std::sin (phase_) * env_ * gain_;
                outL[i] += s;
                if (outR != nullptr)
                    outR[i] += s;
                phase_ += kTwoPi * freq_ / static_cast<float> (sr_);
                if (phase_ > kTwoPi)
                    phase_ -= kTwoPi;
                env_ *= decayPerSample_;
            }

            beat += inc;
        }
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    double    sr_             = 44100.0;
    float     env_           = 0.0f;
    float     phase_         = 0.0f;
    float     freq_          = 988.0f;
    float     decayPerSample_ = 0.999f;
    float     gain_          = 0.30f;
    int       beatsPerBar_   = 4;
    bool      enabled_       = false;
    long long lastBeat_      = std::numeric_limits<long long>::min();
};

} // namespace sculpt
