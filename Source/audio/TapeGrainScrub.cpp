#include "TapeGrainScrub.h"
#include "SampleBuffer.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

namespace
{
    constexpr float kTwoPi   = 6.28318530717958647692f;
    constexpr int   kOverlap = 4;     // grains overlapping per sample (hop = len / kOverlap)
    // Normalize so correlated content (Hann amplitude-COLA sum = kOverlap * 0.5) comes out at unity.
    constexpr float kNormGain = 1.0f / (kOverlap * 0.5f); // = 0.5 for 4x overlap
}

void TapeGrainScrub::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    defaultLen_ = std::max (16, static_cast<int> (sampleRate_ * 0.040)); // ~40 ms grains
    setSpawnLength (defaultLen_);
    reset();
}

void TapeGrainScrub::setSpawnLength (int lenSamples)
{
    spawnLen_ = std::clamp (lenSamples, 16, std::max (16, static_cast<int> (sampleRate_ * 0.120)));
    // 4x overlap (hop = len/4). Summing 4 Hann windows per sample is flat for BOTH correlated and
    // uncorrelated grain content (it approaches the window integral), so a backward sweep — whose
    // grains read uncorrelated spots — no longer leaves the -3 dB notch a 50% overlap would.
    hop_ = std::max (2, spawnLen_ / kOverlap);
}

void TapeGrainScrub::reset()
{
    for (auto& g : grains_)
        g = Grain {};
    hopCounter_   = 0;
    active_       = false;
    primePending_ = false;
}

bool TapeGrainScrub::hasSoundingGrains() const
{
    for (const auto& g : grains_)
        if (g.on)
            return true;
    return false;
}

void TapeGrainScrub::spawn (double pos, int age)
{
    // Reuse a free slot; otherwise steal the oldest grain (largest age).
    int slot = -1;
    int oldestAge = -1;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        if (! grains_[i].on)
        {
            slot = i;
            break;
        }
        if (grains_[i].age > oldestAge)
        {
            oldestAge = grains_[i].age;
            slot      = i;
        }
    }

    grains_[slot].pos = pos;
    grains_[slot].age = age;
    grains_[slot].len = spawnLen_;
    grains_[slot].on  = true;
}

void TapeGrainScrub::render (const SampleBuffer& buffer, double targetPos, double speed, float level,
                             int rightChannel, float& outL, float& outR)
{
    if (active_)
    {
        if (primePending_)
        {
            // Pre-fill the overlap: spawn kOverlap grains staggered in age (as if spawned over the
            // last few hops at the target), so the cloud is at full overlap immediately — no dip.
            for (int k = kOverlap - 1; k >= 0; --k)
                spawn (targetPos + static_cast<double> (k * hop_) * speed, k * hop_);
            primePending_ = false;
            hopCounter_   = hop_;
        }
        else
        {
            // Spawn a new grain at the moving target on each hop.
            if (hopCounter_ <= 0)
            {
                spawn (targetPos);
                hopCounter_ = hop_;
            }
            --hopCounter_;
        }
    }

    float accL = 0.0f;
    float accR = 0.0f;

    for (auto& g : grains_)
    {
        if (! g.on)
            continue;

        const float denom = static_cast<float> (std::max (1, g.len - 1));
        const float win   = 0.5f - 0.5f * std::cos (kTwoPi * static_cast<float> (g.age) / denom);
        const float p     = static_cast<float> (g.pos);
        accL += buffer.getSampleLinear (0, p) * win;
        accR += buffer.getSampleLinear (rightChannel, p) * win;

        g.pos += speed;
        if (++g.age >= g.len)
            g.on = false;
    }

    outL += accL * level * kNormGain;
    outR += accR * level * kNormGain;
}

} // namespace sculpt
