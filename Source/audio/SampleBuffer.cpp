#include "SampleBuffer.h"

namespace sculpt
{

void SampleBuffer::resize (int numChannels, int numFrames)
{
    numChannels_ = numChannels > 0 ? numChannels : 0;
    numFrames_   = numFrames   > 0 ? numFrames   : 0;
    data_.assign (static_cast<size_t> (numChannels_) * static_cast<size_t> (numFrames_), 0.0f);
}

void SampleBuffer::clear()
{
    for (auto& s : data_)
        s = 0.0f;
}

float SampleBuffer::getSample (int channel, int frame) const
{
    if (channel < 0 || channel >= numChannels_ || frame < 0 || frame >= numFrames_)
        return 0.0f;
    return data_[static_cast<size_t> (channel) * static_cast<size_t> (numFrames_) + static_cast<size_t> (frame)];
}

void SampleBuffer::setSample (int channel, int frame, float value)
{
    if (channel < 0 || channel >= numChannels_ || frame < 0 || frame >= numFrames_)
        return;
    data_[static_cast<size_t> (channel) * static_cast<size_t> (numFrames_) + static_cast<size_t> (frame)] = value;
}

float SampleBuffer::getSampleLinear (int channel, float framePos) const
{
    if (numFrames_ < 2 || channel < 0 || channel >= numChannels_)
        return 0.0f;

    // Wrap into the valid range.
    const float frames = static_cast<float> (numFrames_);
    while (framePos < 0.0f)
        framePos += frames;
    while (framePos >= frames)
        framePos -= frames;

    const int   i0   = static_cast<int> (framePos);
    const int   i1   = (i0 + 1) % numFrames_;
    const float frac = framePos - static_cast<float> (i0);

    const float* ch = getChannelData (channel);
    return ch[i0] + (ch[i1] - ch[i0]) * frac;
}

const float* SampleBuffer::getChannelData (int channel) const
{
    return data_.data() + static_cast<size_t> (channel) * static_cast<size_t> (numFrames_);
}

float* SampleBuffer::getChannelData (int channel)
{
    return data_.data() + static_cast<size_t> (channel) * static_cast<size_t> (numFrames_);
}

} // namespace sculpt
