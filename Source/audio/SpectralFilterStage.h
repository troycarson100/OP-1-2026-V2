#pragma once

#include <array>
#include "../util/SmoothedValue.h"
#include "../util/Constants.h"

namespace sculpt
{

// Additive spectral resonator bank inspired by the S-4 Ring device.
// Architecture: dry signal passes through unchanged; 32 scale-quantized BPF
// resonators add tuned resonant peaks on top.  Cutoff shapes a spectral
// envelope (LP rolloff) over which bands are loudest.  Resonance controls
// peak boost strength.  Decay controls pole radius / ring time.
// All processing is real-time safe — no allocation, no locks, no file I/O.
class SpectralFilterStage
{
public:
    static constexpr int kNumBands     = 32;
    static constexpr int kSemisPerBand = 3;    // base semitone step between bands

    void prepare (double sampleRate);
    void reset();

    // All params normalized 0..1. scaleIdx maps to FilterScale enum.
    void setParams (float cutoff01, float resonance01, float decay01,
                    float pitch01,  float scaleIdx01, float mix01);

    void process (float* left, float* right, int numSamples);

    // Read-only per-band envelope for the LCD spectrum display (0..1 normalized).
    float getBandEnvelope (int band) const;

private:
    struct BandCoeffs
    {
        float b0      = 0.0f;   // (1 - r^2) / 2  — BPF gain coefficient
        float a1      = 0.0f;   // -2*r*cos(w0)
        float a2      = 0.0f;   // r^2
        float envGain = 0.0f;   // spectral envelope × peak boost (pre-multiplied)
        bool  active  = false;
    };

    struct ChannelState
    {
        float w1 = 0.0f;    // Direct Form II delay: w[n-1]
        float w2 = 0.0f;    // w[n-2]
    };

    void rebuildCoeffs();

    float processBandSample (ChannelState& s, const BandCoeffs& c, float x) const;

    double sampleRate_ = 44100.0;

    std::array<BandCoeffs,   kNumBands> coeffs_      {};
    std::array<ChannelState, kNumBands> leftStates_  {};
    std::array<ChannelState, kNumBands> rightStates_ {};
    std::array<float,        kNumBands> envelopes_   {};  // raw (for normalization)

    SmoothedValue mix_;
    SmoothedValue resonance_;  // smoothed peak boost
    SmoothedValue cutoff_;     // smoothed cutoff for spectral envelope

    // Running maximum of all band envelopes for normalized LCD display.
    float peakEnvelope_ = 0.001f;

    float lastCutoff_ = -1.0f;
    float lastRes_    = -1.0f;
    float lastDecay_  = -1.0f;
    float lastPitch_  = -1.0f;
    float lastScale_  = -1.0f;

    int numActiveBands_ = 0;
};

} // namespace sculpt
