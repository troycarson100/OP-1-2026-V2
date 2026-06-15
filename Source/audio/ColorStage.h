#pragma once

#include "OnePole.h"
#include "../util/SmoothedValue.h"

namespace sculpt
{

struct ColorVisualSnapshot;

// DEFORM-style color/distortion stage.
// Signal chain: dual-band split (LP/HP at 650 Hz) → drive (tanh) → tilt mix
//               → bitcrush → compress → noise → wet blend.
// All processing is real-time safe: no allocation, no locks, no I/O.
class ColorStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // All parameters normalized 0..1.
    void setParams (float drive01, float crush01, float tilt01, float compress01,
                    float noise01, float noiseDecay01, float noiseTone01, float wet01);

    void process (float* left, float* right, int numSamples);

    // Called after process() to fill the LCD snapshot for the UI thread.
    void updateVisual (ColorVisualSnapshot& snap) const;

private:
    float processSampleDrive (float in, float driveGain) const;
    float processSampleCompress (float in, float& envelope, float compress) const;
    float generateNoise();

    // Dual-band crossover (1-pole LP; HP = input − LP).
    OnePole crossL_, crossR_;

    // Noise bandpass: LP then HP subtraction.
    OnePole noiseLpL_, noiseLpR_;
    OnePole noiseHpL_, noiseHpR_;

    // Pre-decimation anti-aliasing LP (must run before sample-and-hold to prevent fold-back).
    OnePole preReduxLpL_, preReduxLpR_;

    // Post-crush smoothing LP (tames remaining quantization edges).
    OnePole crushLpL_, crushLpR_;

    // Redux (sample-rate reduction): sample-and-hold state per channel.
    float heldSampleL_ = 0.0f;
    float heldSampleR_ = 0.0f;
    int   holdCounterL_ = 1;
    int   holdCounterR_ = 1;

    // Noise envelope follower (one per channel, smoothed asymmetrically).
    float noiseEnvL_ = 0.0f;
    float noiseEnvR_ = 0.0f;

    // Compressor peak envelope per channel.
    float compEnvL_ = 0.0f;
    float compEnvR_ = 0.0f;

    // Cheap LCG noise state (shared; used for both dither and noise generator).
    uint32_t noiseSeed_ = 0x12345678u;

    SmoothedValue drive_, crush_, tilt_, compress_, noise_, noiseDecay_, noiseTone_, wet_;

    double sampleRate_ = 44100.0;

    // Snapshot written during process() for LCD display.
    float visLowLevel_  = 0.0f;
    float visHighLevel_ = 0.0f;
};

} // namespace sculpt
