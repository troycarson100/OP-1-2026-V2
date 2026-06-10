#pragma once

#include <array>
#include "../util/SmoothedValue.h"
#include "../util/Constants.h"

namespace sculpt
{

// Morphing resonator inspired by the S-4 Ring device: a 48-band tuned filter
// bank that IS the signal path.  Bands are TPT SVF bandpass resonators placed
// directly on the scale degrees of the selected Key (Free = continuous grid),
// with a subtle L/R detune and alternate band panning for width.
//   Cutoff    — smooth shoulder across the band index (even knob feel)
//   Resonance — constant-Q bandwidth; narrow bands BLOOM louder (analog-style)
//   Decay     — minimum ring time
//   Mix       — equal-power dry/bank crossfade (with a little dry bleed)
// A slow internal traveling wave animates band gains so the spectrum breathes.
// All processing is real-time safe — no allocation, no locks, no file I/O.
class SpectralFilterStage
{
public:
    static constexpr int kNumBands     = 48;
    static constexpr int kSemisPerBand = 2;    // Free-scale grid step

    void prepare (double sampleRate);
    void reset();

    // All params normalized 0..1. scaleIdx → FilterScale; key01 → tonic 0..11.
    void setParams (float cutoff01, float resonance01, float decay01,
                    float pitch01, float scaleIdx01, float key01, float mix01);

    void process (float* left, float* right, int numSamples);

    // Read-only per-band envelope for the LCD spectrum display (0..1 normalized).
    float getBandEnvelope (int band) const;

private:
    struct Band
    {
        // Live (slewed) TPT SVF coefficients.  g differs per channel: a slight
        // L/R detune gives the bank a wide, chorus-like shimmer.
        float gL = 0.0f, gR = 0.0f;   // tan(pi * f / fs)
        float k  = 1.0f;              // 1 / Q
        float dL = 0.0f, dR = 0.0f;   // 1 / (1 + g*(g + k)), per block

        // Targets set by rebuildCoeffs(); live values slew toward these.
        float tGL = 0.0f, tGR = 0.0f, tK = 1.0f;

        float freqHz = 0.0f;
        float bloom  = 1.0f;          // resonant bloom gain (rises with Q)
        float gainL  = 0.0f, gainR = 0.0f;  // final per-band gains (per block)
        bool  active = false;
    };

    struct ChannelState
    {
        float ic1 = 0.0f;   // TPT SVF integrator states
        float ic2 = 0.0f;
    };

    void rebuildCoeffs();
    void assignBand (int index, float midiNote, float qRes, float t60);

    double sampleRate_ = 44100.0;

    std::array<Band,         kNumBands> bands_       {};
    std::array<ChannelState, kNumBands> leftStates_  {};
    std::array<ChannelState, kNumBands> rightStates_ {};
    std::array<float,        kNumBands> envelopes_   {};  // raw (for normalization)

    SmoothedValue mix_;
    SmoothedValue resonance_;
    SmoothedValue cutoff_;

    // Running maximum of all band envelopes for normalized LCD display.
    float peakEnvelope_ = 0.001f;

    // Slow traveling-wave animation across band gains.
    float animPhase_ = 0.0f;

    float lastCutoff_ = -1.0f;
    float lastRes_    = -1.0f;
    float lastDecay_  = -1.0f;
    float lastPitch_  = -1.0f;
    float lastScale_  = -1.0f;
    float lastKey_    = -1.0f;

    bool coeffsInit_   = false;  // first rebuild snaps live coeffs to targets
    int numActiveBands_ = 0;
};

} // namespace sculpt
