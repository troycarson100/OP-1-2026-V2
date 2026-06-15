#pragma once

#include <array>
#include "OnePole.h"
#include "AllpassDelay.h"

namespace sculpt
{

// Dattorro-style plate reverb (Effect Design, Part 1, 1997). Mono in, stereo wet out.
// Input bandwidth low-pass -> 4 input diffusers -> figure-eight tank with two
// LFO-modulated decay diffusers + damping -> multi-tap stereo output.
// Allocation only in prepare(); processBlock() is real-time safe.
class HallReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    void setRt60 (float seconds);          // mapped to tank decay coefficient
    void setSize (float size01);           // scales tank delay lengths
    void setDampHz (float hz);             // in-tank high-frequency damping
    void setStereoWidth (float width01);   // 0 mono .. 1 wide
    void setFreeze (bool freeze);

    void processBlock (const float* monoIn, float* wetL, float* wetR, int numSamples);

private:
    void recomputeDecay();

    double sampleRate_ = 44100.0;
    float  srScale_    = 1.0f; // sampleRate_ / 29761 (Dattorro base rate)

    OnePole bandwidth_;        // input high-frequency attenuation
    OnePole dampL_, dampR_;    // in-tank damping

    std::array<AllpassDelay, 4> inDiff_;   // input diffusers

    AllpassDelay apL1_, apR1_; // decay diffusion 1 (modulated)
    AllpassDelay apL2_, apR2_; // decay diffusion 2
    DelayLine    dL1_, dL2_, dR1_, dR2_;

    float lfoPhaseL_ = 0.0f;
    float lfoPhaseR_ = 0.31f;
    float lfoIncL_   = 0.0f;
    float lfoIncR_   = 0.0f;
    float excursion_ = 16.0f;

    float zL_ = 0.0f, zR_ = 0.0f; // cross-feedback state (branch ends)

    float rt60_      = 2.0f;
    float sizeScale_ = 1.0f;
    float decay_     = 0.5f;
    float width_     = 0.5f;
    float inGain_    = 1.0f;
    bool  freeze_    = false;
};

} // namespace sculpt
