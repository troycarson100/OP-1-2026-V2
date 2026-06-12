#pragma once

namespace sculpt
{

// Per-track mix bus: 3-band peaking EQ + stereo-linked feed-forward compressor.
// Runs after Space, before level/pan. Portable, allocation-free in process().
class MixBusStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // All arguments normalized 0..1 (host / engine convention).
    void setParams (float eqLow01, float eqMid01, float eqHigh01,
                    float compThreshold01, float compMakeup01);

    void process (float* left, float* right, int numSamples);

    // Smoothed 0..1 for LCD (approximate visible gain reduction this block).
    float getCompReductionMeter01() const noexcept { return compReductionMeter01_; }

private:
    double sampleRate_ = 44100.0;

    float eqLow01_  = 0.5f;
    float eqMid01_  = 0.5f;
    float eqHigh01_ = 0.5f;
    float compThr01_ = 0.82f;
    float compMk01_  = 0.0f;

    bool eqCoeffsDirty_ = true;

    // Biquad DF I: y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2 (a0 normalized to 1).
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        void reset() { x1 = x2 = y1 = y2 = 0.0f; }

        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    // Three bands, stereo = two chains of three biquads.
    Biquad lowL_, midL_, hiL_;
    Biquad lowR_, midR_, hiR_;

    float env_ = 0.0f;
    float attCoeff_ = 0.01f;
    float relCoeff_ = 0.0001f;

    // Smoothed linear gain toward targetLin (separate attack/release coeffs — no instant downward steps).
    float smoothGainLin_   = 1.0f;
    float gainAttCoeff_    = 0.01f;
    float gainRelCoeff_    = 0.001f;

    // Smoothes min(compGain, kWetCeil/pk) toward target; hard-capped each sample to instantaneous ceiling.
    float gApplySmoothed_     = 1.0f;
    float gApplySmoothCoeff_  = 0.05f;

    float compReductionMeter01_ = 0.0f;

    void recomputeCoeffs();
    static void setPeaking (Biquad& b, double sr, float freqHz, float Q, float gainDb);
};

} // namespace sculpt
