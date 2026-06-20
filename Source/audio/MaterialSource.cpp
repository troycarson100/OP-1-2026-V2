#include <algorithm>
#include <cmath>
#include <cstring>
#include "MaterialSource.h"
#include "../util/MathUtils.h"
#include "../util/Random.h"

namespace sculpt
{

void MaterialSource::prepare (double sampleRate, float seconds)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int minFrames = static_cast<int> (sampleRate_ * static_cast<double> (seconds));
    const int curFrames = buffer_.getNumFrames();
    // Grow-only: keep a loaded buffer from being truncated when the minimum capacity grows.
    const int target = std::max (minFrames, curFrames);
    if (target != curFrames)
        buffer_.resize (2, target);
}

// All placeholders are built from whole numbers of cycles so the buffer
// loops without clicks.
void MaterialSource::generatePlaceholder (PlaceholderType type)
{
    userMaterial_ = false;

    const int frames = buffer_.getNumFrames();
    if (frames < 2)
        return;

    const float invFrames = 1.0f / static_cast<float> (frames);
    Random rng { 0xC0FFEEu };

    auto cyclesToPhaseInc = [invFrames] (float cycles) { return cycles * kTwoPi * invFrames; };

    // Choose cycle counts near musical frequencies for a 2s buffer at 44.1k.
    const float seconds = static_cast<float> (frames) / static_cast<float> (sampleRate_);
    auto cyclesFor = [seconds] (float hz) { return std::round (hz * seconds); };

    float noiseLp = 0.0f;

    for (int i = 0; i < frames; ++i)
    {
        const float t01 = static_cast<float> (i) * invFrames;
        float l = 0.0f, r = 0.0f;

        switch (type)
        {
            case PlaceholderType::Tone:
            {
                const float p  = static_cast<float> (i) * cyclesToPhaseInc (cyclesFor (220.0f));
                const float p2 = static_cast<float> (i) * cyclesToPhaseInc (cyclesFor (220.0f) * 2.0f);
                const float trem = 0.8f + 0.2f * std::sin (t01 * kTwoPi * 2.0f);
                l = (std::sin (p) * 0.8f + std::sin (p2) * 0.15f) * trem * 0.5f;
                r = (std::sin (p + 0.3f) * 0.8f + std::sin (p2) * 0.15f) * trem * 0.5f;
                break;
            }
            case PlaceholderType::Wave:
            {
                const float base = cyclesFor (110.0f);
                float v = 0.0f;
                for (int h = 1; h <= 5; ++h)
                    v += std::sin (static_cast<float> (i) * cyclesToPhaseInc (base * static_cast<float> (h))) / static_cast<float> (h);
                v *= 0.35f;
                l = v;
                r = v * 0.9f + 0.1f * std::sin (static_cast<float> (i) * cyclesToPhaseInc (base * 1.5f));
                break;
            }
            case PlaceholderType::Noise:
            {
                const float n = rng.nextBipolar();
                noiseLp += 0.08f * (n - noiseLp);
                const float swell = 0.6f + 0.4f * std::sin (t01 * kTwoPi * 3.0f);
                l = noiseLp * swell * 0.9f;
                r = noiseLp * swell * 0.9f * (1.0f - 0.3f * std::sin (t01 * kTwoPi));
                break;
            }
            case PlaceholderType::Pulse:
            {
                // Minor-ish chord stack with a slow pulse.
                const float root = cyclesFor (146.83f);   // D3
                const float p1 = static_cast<float> (i) * cyclesToPhaseInc (root);
                const float p2 = static_cast<float> (i) * cyclesToPhaseInc (cyclesFor (174.61f)); // F3
                const float p3 = static_cast<float> (i) * cyclesToPhaseInc (cyclesFor (220.0f));  // A3
                const float pulse = 0.55f + 0.45f * std::sin (t01 * kTwoPi * 4.0f);
                const float v = (std::sin (p1) + std::sin (p2) * 0.8f + std::sin (p3) * 0.7f) * 0.22f * pulse;
                l = v;
                r = (std::sin (p1) * 0.8f + std::sin (p2) + std::sin (p3) * 0.6f) * 0.22f * pulse;
                break;
            }
        }

        buffer_.setSample (0, i, l);
        buffer_.setSample (1, i, r);
    }
}

void MaterialSource::loadStereoPCM (const float* left, const float* right, int numFrames)
{
    if (left == nullptr || numFrames < 1)
        return;

    userMaterial_ = true;
    buffer_.resize (2, numFrames);
    for (int i = 0; i < numFrames; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        buffer_.setSample (0, i, l);
        buffer_.setSample (1, i, r);
    }
}

void MaterialSource::setSampleName (const char* name)
{
    if (name == nullptr)
    {
        name_[0] = '\0';
        return;
    }
    std::strncpy (name_, name, 63);
    name_[63] = '\0';
}

} // namespace sculpt
