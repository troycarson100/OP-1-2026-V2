#pragma once

#include <array>
#include "../util/SmoothedValue.h"
#include "../util/Constants.h"

namespace sculpt
{

// Spectral resonator filter bank: N parallel bandpass resonators whose center
// frequencies are tuned to a musical scale.  Inspired by the S-4 Ring device.
// All processing is real-time safe — no allocation, no locks, no file I/O.
class SpectralFilterStage
{
public:
    static constexpr int kNumBands      = 32;
    static constexpr int kSemisPerBand  = 3;   // base step before scale snap

    void prepare (double sampleRate);
    void reset();

    // All params normalized 0..1. scaleIdx maps to FilterScale enum.
    void setParams (float cutoff01, float resonance01, float decay01,
                    float pitch01,  float scaleIdx01, float mix01);

    void process (float* left, float* right, int numSamples);

    // Read-only per-band envelope for the LCD spectrum display.
    float getBandEnvelope (int band) const;

private:
    struct BandCoeffs
    {
        float b0 = 0.0f;    // (1 - r^2) / 2
        float a1 = 0.0f;    // -2*r*cos(w0)
        float a2 = 0.0f;    // r^2
        bool  active = false;
    };

    struct ChannelState
    {
        float w1 = 0.0f;    // Direct Form II delay line: w[n-1]
        float w2 = 0.0f;    // w[n-2]
    };

    void rebuildCoeffs();

    float processBandSample (ChannelState& s, const BandCoeffs& c, float x) const;

    double sampleRate_ = 44100.0;

    std::array<BandCoeffs,   kNumBands>  coeffs_ {};
    std::array<ChannelState, kNumBands>  leftStates_  {};
    std::array<ChannelState, kNumBands>  rightStates_ {};
    std::array<float,        kNumBands>  envelopes_   {};   // for LCD

    SmoothedValue mix_;

    float lastCutoff_ = -1.0f;
    float lastRes_    = -1.0f;
    float lastDecay_  = -1.0f;
    float lastPitch_  = -1.0f;
    float lastScale_  = -1.0f;

    int   numActiveBands_ = 0;
};

} // namespace sculpt
