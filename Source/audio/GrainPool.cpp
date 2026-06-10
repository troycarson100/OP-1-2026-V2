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

} // namespace sculpt
