#include <cmath>
#include <cstdint>
#include "ColorStage.h"
#include "../core/ParameterIds.h"
#include "../ui/ScreenModel.h"
#include "../util/MathUtils.h"

namespace sculpt
{

static constexpr float kCrossoverHz   = 650.0f;
static constexpr float kCompAttackSec = 0.002f;
static constexpr float kCompRelSec    = 0.200f;
static constexpr float kVisDecaySec   = 0.060f;

void ColorStage::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 44100.0;

    crossL_.prepare (sampleRate_);
    crossR_.prepare (sampleRate_);
    crossL_.setCutoffHz (kCrossoverHz);
    crossR_.setCutoffHz (kCrossoverHz);

    noiseLpL_.prepare (sampleRate_);
    noiseLpR_.prepare (sampleRate_);
    noiseHpL_.prepare (sampleRate_);
    noiseHpR_.prepare (sampleRate_);
    preReduxLpL_.prepare (sampleRate_);
    preReduxLpR_.prepare (sampleRate_);
    crushLpL_.prepare (sampleRate_);
    crushLpR_.prepare (sampleRate_);

    drive_.prepare      (sampleRate_, 0.02f);
    crush_.prepare      (sampleRate_, 0.02f);
    tilt_.prepare       (sampleRate_, 0.02f);
    compress_.prepare   (sampleRate_, 0.02f);
    noise_.prepare      (sampleRate_, 0.02f);
    noiseDecay_.prepare (sampleRate_, 0.05f);
    noiseTone_.prepare  (sampleRate_, 0.05f);
    wet_.prepare        (sampleRate_, 0.02f);

    drive_.snap      (0.15f);
    crush_.snap      (0.0f);
    tilt_.snap       (0.5f);
    compress_.snap   (0.0f);
    noise_.snap      (0.0f);
    noiseDecay_.snap (0.35f);
    noiseTone_.snap  (0.5f);
    wet_.snap        (0.30f);

    reset();
}

void ColorStage::reset()
{
    crossL_.reset();
    crossR_.reset();
    noiseLpL_.reset();
    noiseLpR_.reset();
    noiseHpL_.reset();
    noiseHpR_.reset();
    preReduxLpL_.reset();
    preReduxLpR_.reset();
    crushLpL_.reset();
    crushLpR_.reset();
    heldSampleL_ = 0.0f;
    heldSampleR_ = 0.0f;
    holdCounterL_ = 1;
    holdCounterR_ = 1;
    noiseEnvL_ = 0.0f;
    noiseEnvR_ = 0.0f;
    compEnvL_  = 0.0f;
    compEnvR_  = 0.0f;
    visLowLevel_  = 0.0f;
    visHighLevel_ = 0.0f;
}

void ColorStage::setParams (float drive01, float crush01, float tilt01, float compress01,
                            float noise01, float noiseDecay01, float noiseTone01, float wet01)
{
    drive_.setTarget      (drive01);
    crush_.setTarget      (crush01);
    tilt_.setTarget       (tilt01);
    compress_.setTarget   (compress01);
    noise_.setTarget      (noise01);
    noiseDecay_.setTarget (noiseDecay01);
    noiseTone_.setTarget  (noiseTone01);
    wet_.setTarget        (wet01);
}

float ColorStage::processSampleDrive (float in, float driveGain) const
{
    // Gain-normalised tanh: same peak amplitude at any drive setting.
    const float dg = driveGain > 1.0e-4f ? driveGain : 1.0e-4f;
    return std::tanh (in * dg) / std::tanh (dg);
}


float ColorStage::processSampleCompress (float in, float& envelope, float compress) const
{
    const float sr      = static_cast<float> (sampleRate_);
    const float attCoef = 1.0f - std::exp (-1.0f / (sr * kCompAttackSec));
    const float relCoef = 1.0f - std::exp (-1.0f / (sr * kCompRelSec));

    const float level = std::abs (in);
    if (level > envelope)
        envelope += attCoef * (level - envelope);
    else
        envelope += relCoef * (level - envelope);

    if (compress < 1.0e-4f)
        return in;

    // Threshold: from unity (no effect) down to 0.15 at full compress.
    const float threshold = lerp (1.0f, 0.15f, compress);
    if (envelope < threshold || threshold < 1.0e-6f)
        return in;

    // Blend from bypass toward hard limiting as compress increases.
    const float hardGr = threshold / envelope;
    return in * lerp (1.0f, hardGr, compress);
}

float ColorStage::generateNoise()
{
    noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
    return static_cast<float> (static_cast<int32_t> (noiseSeed_)) * 4.656612875e-10f;
}

void ColorStage::process (float* left, float* right, int numSamples)
{
    // Block-rate params (inaudible to advance per-block).
    const float tilt01    = tilt_.skip (numSamples);
    const float crush01   = crush_.skip (numSamples);
    const float driveGain = map::colorDriveGain (drive_.skip (numSamples));

    // --- Crush setup (block-rate) ---
    // Redux: quadratic taper → 1..32 sample hold.
    const int holdLen = 1 + static_cast<int> (crush01 * crush01 * 31.0f);

    // Bit reduction: runs across the full knob range (16 bits clean → 3 bits destroyed).
    // Consistent character at every setting rather than a mode switch.
    const float crushBits = 16.0f - crush01 * 13.0f;
    const float crushStep = std::pow (2.0f, crushBits - 1.0f);

    // Pre-decimation anti-aliasing LP: prevents aliased images from folding back into
    // the audible band when the sample-and-hold drops the effective sample rate.
    // Clamped at 4 kHz minimum so the effect stays audibly interesting at heavy settings.
    const float sr = static_cast<float> (sampleRate_);
    const float preReduxHz = std::max (4000.0f, sr / (static_cast<float> (holdLen) * 1.5f));
    preReduxLpL_.setCutoffHz (preReduxHz);
    preReduxLpR_.setCutoffHz (preReduxHz);

    // Post-crush LP: smooths remaining quantisation edges (10 kHz → 4 kHz across knob).
    const float crushLpHz = 10000.0f - crush01 * 6000.0f;
    crushLpL_.setCutoffHz (crushLpHz);
    crushLpR_.setCutoffHz (crushLpHz);

    const float noiseTone01  = noiseTone_.skip (numSamples);
    const float noiseDecay01 = noiseDecay_.skip (numSamples);
    const float noiseCutoff  = map::colorNoiseCutoffHz (noiseTone01);
    const float noiseDecayMs = map::colorNoiseDecayMs (noiseDecay01);

    const float noiseRelSec = noiseDecayMs * 0.001f;
    const float noiseRelCoef = 1.0f - std::exp (-1.0f / (sr * (noiseRelSec > 0.001f ? noiseRelSec : 0.001f)));
    const float noiseAttCoef = 1.0f - std::exp (-1.0f / (sr * 0.001f));  // 1 ms attack

    noiseLpL_.setCutoffHz (noiseCutoff);
    noiseLpR_.setCutoffHz (noiseCutoff);
    noiseHpL_.setCutoffHz (noiseCutoff * 0.1f);  // HP 1 decade below LP = bandpass
    noiseHpR_.setCutoffHz (noiseCutoff * 0.1f);

    const float lowGain  = map::colorTiltLowGain  (tilt01);
    const float highGain = map::colorTiltHighGain (tilt01);

    float peakLow = 0.0f, peakHigh = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float comp  = compress_.next();
        const float nAmt  = noise_.next();
        const float wet   = wet_.next();

        // ---- LEFT ----
        const float dryL = left[i];

        const float lpL = crossL_.processLowpass (dryL);
        const float hpL = dryL - lpL;

        const float drvLowL  = processSampleDrive (lpL, driveGain);
        const float drvHighL = processSampleDrive (hpL, driveGain);

        peakLow  = std::max (peakLow,  std::abs (drvLowL));
        peakHigh = std::max (peakHigh, std::abs (drvHighL));

        float mixedL = drvLowL * lowGain + drvHighL * highGain;

        if (crush01 > 0.001f)
        {
            // 1. Pre-decimation anti-alias LP (removes images above decimated Nyquist).
            const float prefiltL = preReduxLpL_.processLowpass (mixedL);

            // 2. Sample-and-hold decimation (redux).
            if (--holdCounterL_ <= 0)
            {
                heldSampleL_  = prefiltL;
                holdCounterL_ = holdLen;
            }

            // 3. Dithered bit reduction — TPDF dither at exactly 1 LSB amplitude.
            float crushedL = heldSampleL_;
            if (crushStep > 1.0e-3f)
            {
                const float dither = (generateNoise() + generateNoise()) * 0.5f / crushStep;
                crushedL = std::round ((crushedL + dither) * crushStep) / crushStep;
            }

            // 4. Post-crush smoothing LP.
            mixedL = crushLpL_.processLowpass (crushedL);
        }

        float processedL = processSampleCompress (mixedL, compEnvL_, comp);

        if (nAmt > 0.001f)
        {
            const float inLvl = std::abs (dryL);
            if (inLvl > noiseEnvL_)
                noiseEnvL_ += noiseAttCoef * (inLvl - noiseEnvL_);
            else
                noiseEnvL_ += noiseRelCoef * (inLvl - noiseEnvL_);

            const float nRaw = generateNoise();
            const float nLp  = noiseLpL_.processLowpass (nRaw);
            const float nBp  = nLp - noiseHpL_.processLowpass (nLp);
            processedL += nBp * noiseEnvL_ * nAmt;
        }

        left[i] = lerp (dryL, processedL, wet);

        // ---- RIGHT ----
        const float dryR = right[i];

        const float lpR = crossR_.processLowpass (dryR);
        const float hpR = dryR - lpR;

        const float drvLowR  = processSampleDrive (lpR, driveGain);
        const float drvHighR = processSampleDrive (hpR, driveGain);

        float mixedR = drvLowR * lowGain + drvHighR * highGain;

        if (crush01 > 0.001f)
        {
            const float prefiltR = preReduxLpR_.processLowpass (mixedR);

            if (--holdCounterR_ <= 0)
            {
                heldSampleR_  = prefiltR;
                holdCounterR_ = holdLen;
            }

            float crushedR = heldSampleR_;
            if (crushStep > 1.0e-3f)
            {
                const float dither = (generateNoise() + generateNoise()) * 0.5f / crushStep;
                crushedR = std::round ((crushedR + dither) * crushStep) / crushStep;
            }

            mixedR = crushLpR_.processLowpass (crushedR);
        }

        float processedR = processSampleCompress (mixedR, compEnvR_, comp);

        if (nAmt > 0.001f)
        {
            const float inLvl = std::abs (dryR);
            if (inLvl > noiseEnvR_)
                noiseEnvR_ += noiseAttCoef * (inLvl - noiseEnvR_);
            else
                noiseEnvR_ += noiseRelCoef * (inLvl - noiseEnvR_);

            const float nRaw = generateNoise();
            const float nLp  = noiseLpR_.processLowpass (nRaw);
            const float nBp  = nLp - noiseHpR_.processLowpass (nLp);
            processedR += nBp * noiseEnvR_ * nAmt;
        }

        right[i] = lerp (dryR, processedR, wet);
    }

    // Update visual levels: fast attack, ~60ms decay.
    const float visAtt = 0.9f;
    const float visDcy = 1.0f - std::exp (-1.0f / (sr * kVisDecaySec));
    if (peakLow > visLowLevel_)
        visLowLevel_ += visAtt * (peakLow - visLowLevel_);
    else
        visLowLevel_ += visDcy * (peakLow - visLowLevel_);

    if (peakHigh > visHighLevel_)
        visHighLevel_ += visAtt * (peakHigh - visHighLevel_);
    else
        visHighLevel_ += visDcy * (peakHigh - visHighLevel_);
}

void ColorStage::updateVisual (ColorVisualSnapshot& snap) const
{
    snap.drive01       = drive_.getCurrent();
    snap.crush01       = crush_.getCurrent();
    snap.tilt01        = tilt_.getCurrent();
    snap.compress01    = compress_.getCurrent();
    snap.noise01       = noise_.getCurrent();
    snap.noiseTone01   = noiseTone_.getCurrent();
    snap.wet01         = wet_.getCurrent();
    snap.lowBandLevel  = clamp01 (visLowLevel_);
    snap.highBandLevel = clamp01 (visHighLevel_);
}

} // namespace sculpt
