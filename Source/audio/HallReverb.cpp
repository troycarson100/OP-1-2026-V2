#include <algorithm>
#include <cmath>
#include "HallReverb.h"
#include "../util/MathUtils.h"

namespace sculpt
{

// Dattorro base topology lengths in samples at Fs = 29761 Hz (Effect Design, 1997).
static constexpr float kBaseRate = 29761.0f;

static constexpr int kInDiffLen[4] = { 142, 107, 379, 277 };
static constexpr float kInDiffCoef[4] = { 0.75f, 0.75f, 0.625f, 0.625f };

static constexpr int kApL1 = 672;   // decay diffusion 1 (modulated)
static constexpr int kApR1 = 908;
static constexpr int kDL1  = 4453;
static constexpr int kDR1  = 4217;
static constexpr int kApL2 = 1800;  // decay diffusion 2
static constexpr int kApR2 = 2656;
static constexpr int kDL2  = 3720;
static constexpr int kDR2  = 3163;

static constexpr float kDecayDiff1 = 0.70f;
static constexpr float kDecayDiff2 = 0.50f;

static constexpr float kMaxSizeScale = 2.30f;

void HallReverb::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 1.0e-6 ? sampleRate : 44100.0;
    srScale_    = static_cast<float> (sampleRate_) / kBaseRate;
    excursion_  = 16.0f * srScale_;

    const float a = srScale_ * kMaxSizeScale;
    auto capOf = [a] (int base) { return static_cast<int> (std::ceil (static_cast<float> (base) * a)) + 8; };

    for (int i = 0; i < 4; ++i)
    {
        inDiff_[static_cast<size_t> (i)].prepare (capOf (kInDiffLen[i]));
        inDiff_[static_cast<size_t> (i)].setDelaySamples (static_cast<float> (kInDiffLen[i]) * srScale_);
        inDiff_[static_cast<size_t> (i)].setCoeff (kInDiffCoef[i]);
    }

    apL1_.prepare (capOf (kApL1));
    apR1_.prepare (capOf (kApR1));
    apL2_.prepare (capOf (kApL2));
    apR2_.prepare (capOf (kApR2));
    dL1_.prepare (capOf (kDL1));
    dR1_.prepare (capOf (kDR1));
    dL2_.prepare (capOf (kDL2));
    dR2_.prepare (capOf (kDR2));

    apL1_.setCoeff (-kDecayDiff1);
    apR1_.setCoeff (-kDecayDiff1);
    apL2_.setCoeff (kDecayDiff2);
    apR2_.setCoeff (kDecayDiff2);

    baseApL1_ = static_cast<float> (kApL1) * srScale_;
    baseApR1_ = static_cast<float> (kApR1) * srScale_;
    baseApL2_ = static_cast<float> (kApL2) * srScale_;
    baseApR2_ = static_cast<float> (kApR2) * srScale_;
    baseDL1_  = static_cast<float> (kDL1)  * srScale_;
    baseDR1_  = static_cast<float> (kDR1)  * srScale_;
    baseDL2_  = static_cast<float> (kDL2)  * srScale_;
    baseDR2_  = static_cast<float> (kDR2)  * srScale_;

    bandwidth_.prepare (sampleRate_);
    bandwidth_.setCutoffHz (11000.0f);
    dampL_.prepare (sampleRate_);
    dampR_.prepare (sampleRate_);
    dampL_.setCutoffHz (9000.0f);
    dampR_.setCutoffHz (9000.0f);

    lfoIncL_ = kTwoPi * 0.9f / static_cast<float> (sampleRate_);
    lfoIncR_ = kTwoPi * 1.3f / static_cast<float> (sampleRate_);

    // ~0.12 s glide keeps Size sweeps click-free (a smooth pitch slew, not a tear).
    sizeSm_.prepare (sampleRate_, 0.12f);
    sizeSm_.snap (0.8f);
    sizeTarget_ = 0.8f;

    setRt60 (2.0f);
    reset();
}

void HallReverb::reset()
{
    for (auto& ap : inDiff_) ap.reset();
    apL1_.reset(); apR1_.reset(); apL2_.reset(); apR2_.reset();
    dL1_.reset();  dR1_.reset();  dL2_.reset();  dR2_.reset();
    bandwidth_.reset();
    dampL_.reset(); dampR_.reset();
    zL_ = zR_ = 0.0f;
    lfoPhaseL_ = 0.0f;
    lfoPhaseR_ = 0.31f;
}

void HallReverb::setSize (float sizeScale)
{
    // Receives the line-length scale directly (see map::spaceReverbSizeLineScale).
    // Smoothed per-sample in processBlock so Size sweeps glide instead of tearing.
    sizeTarget_ = std::clamp (sizeScale, 0.30f, kMaxSizeScale);
    sizeSm_.setTarget (sizeTarget_);
    recomputeDecay();
}

void HallReverb::recomputeDecay()
{
    if (freeze_)
    {
        decay_ = 0.9999f;
        return;
    }
    const float s        = srScale_ * sizeTarget_;
    const double loopLen  = static_cast<double> (kApL1 + kDL1 + kApL2 + kDL2) * static_cast<double> (s);
    const double loopTime = std::max (1.0e-4, loopLen / sampleRate_);
    const double g        = std::exp (-6.90776 * loopTime / std::max (0.05, static_cast<double> (rt60_)));
    decay_ = static_cast<float> (std::clamp (g, 0.10, 0.99985));
}

void HallReverb::setRt60 (float seconds)
{
    rt60_ = std::clamp (seconds, 0.08f, 20.0f);
    recomputeDecay();
}

void HallReverb::setDampHz (float hz)
{
    dampL_.setCutoffHz (hz);
    dampR_.setCutoffHz (hz);
}

void HallReverb::setStereoWidth (float width01) { width_ = clamp01 (width01); }

void HallReverb::setFreeze (bool freeze)
{
    freeze_ = freeze;
    inGain_ = freeze ? 0.0f : 1.0f;
    recomputeDecay();
}

void HallReverb::processBlock (const float* monoIn, float* wetL, float* wetR, int numSamples)
{
    for (int n = 0; n < numSamples; ++n)
    {
        const float scale = sizeSm_.next();         // smoothed size scale (click-free)
        const float tapS  = srScale_ * scale;       // tap-position scale (base @ 29761)

        float x = bandwidth_.processLowpass (monoIn[static_cast<size_t> (n)] * inGain_);
        x = inDiff_[0].process (x);
        x = inDiff_[1].process (x);
        x = inDiff_[2].process (x);
        x = inDiff_[3].process (x);

        const float modL = excursion_ * std::sin (lfoPhaseL_);
        const float modR = excursion_ * std::sin (lfoPhaseR_);
        lfoPhaseL_ += lfoIncL_; if (lfoPhaseL_ > kTwoPi) lfoPhaseL_ -= kTwoPi;
        lfoPhaseR_ += lfoIncR_; if (lfoPhaseR_ > kTwoPi) lfoPhaseR_ -= kTwoPi;

        const float oldZL = zL_;
        const float oldZR = zR_;

        float l = x + decay_ * oldZR;
        l = apL1_.processVarMod (l, baseApL1_ * scale, modL);
        l = dL1_.processVar (l, baseDL1_ * scale);
        l = dampL_.processLowpass (l);
        l *= decay_;
        l = apL2_.processVarMod (l, baseApL2_ * scale, 0.0f);
        zL_ = dL2_.processVar (l, baseDL2_ * scale);

        float r = x + decay_ * oldZL;
        r = apR1_.processVarMod (r, baseApR1_ * scale, modR);
        r = dR1_.processVar (r, baseDR1_ * scale);
        r = dampR_.processLowpass (r);
        r *= decay_;
        r = apR2_.processVarMod (r, baseApR2_ * scale, 0.0f);
        zR_ = dR2_.processVar (r, baseDR2_ * scale);

        float yL = dR1_.readAt (266.0f * tapS) + dR1_.readAt (2974.0f * tapS)
                 - apR2_.readAt (1913.0f * tapS) + dR2_.readAt (1996.0f * tapS)
                 - dL1_.readAt (1990.0f * tapS) - apL2_.readAt (187.0f * tapS)
                 - dL2_.readAt (1066.0f * tapS);

        float yR = dL1_.readAt (353.0f * tapS) + dL1_.readAt (3627.0f * tapS)
                 - apL2_.readAt (1228.0f * tapS) + dL2_.readAt (2673.0f * tapS)
                 - dR1_.readAt (2111.0f * tapS) - apR2_.readAt (335.0f * tapS)
                 - dR2_.readAt (121.0f * tapS);

        constexpr float outGain = 0.6f;
        yL *= outGain;
        yR *= outGain;

        const float mid = 0.5f * (yL + yR);
        wetL[static_cast<size_t> (n)] = sanitize (lerp (mid, yL, width_));
        wetR[static_cast<size_t> (n)] = sanitize (lerp (mid, yR, width_));
    }
}

} // namespace sculpt
