#include <algorithm>
#include <cmath>
#include "GrainPool.h"

namespace sculpt
{

void GrainPool::reset()
{
    for (auto& voice : voices_)
        voice.kill();
}

GrainVoice* GrainPool::findFreeVoice()
{
    for (auto& voice : voices_)
        if (! voice.isActive())
            return &voice;
    return nullptr;
}

void GrainPool::renderAll (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    for (auto& voice : voices_)
        if (voice.isActive())
            voice.render (buffer, outL, outR, numSamples);
}

int GrainPool::countActive() const
{
    int count = 0;
    for (const auto& voice : voices_)
        if (voice.isActive())
            ++count;
    return count;
}

void GrainPool::fillGrainDisplay (float totalFrames, GrainDisplaySlot* out, int maxSlots) const
{
    const int n = std::min (maxSlots, static_cast<int> (voices_.size()));
    const float invFrames = (totalFrames > 1.0f) ? (1.0f / totalFrames) : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const auto& voice = voices_[static_cast<std::size_t> (i)];
        if (! voice.isActive())
        {
            out[i].active = false;
            continue;
        }

        const float start = voice.originStartFrame();
        const float len   = static_cast<float> (voice.originLength());
        out[i].start01  = std::clamp (start * invFrames, 0.0f, 1.0f);
        out[i].len01    = std::clamp (len * invFrames, 0.0f, 1.0f);
        out[i].phase01  = voice.phase01();
        out[i].active   = true;
    }

    for (int i = n; i < maxSlots; ++i)
        out[i].active = false;
}

} // namespace sculpt
