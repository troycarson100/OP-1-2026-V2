#include "SampleRecorder.h"
#include "SampleBuffer.h"

namespace sculpt
{

void SampleRecorder::prepare (SampleBuffer& target)
{
    target_   = &target;
    writePos_ = 0;
    armed_     = false;
    recording_ = false;
}

void SampleRecorder::process (const float* const* inputs, int numChannels, int numSamples)
{
    if (target_ == nullptr || ! armed_ || inputs == nullptr || numChannels < 1)
        return;

    recording_ = true;

    const int frames     = target_->getNumFrames();
    const int dstChans   = target_->getNumChannels();
    if (frames < 1 || dstChans < 1)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < dstChans; ++ch)
        {
            const int srcCh = ch < numChannels ? ch : numChannels - 1;
            target_->setSample (ch, writePos_, inputs[srcCh][i]);
        }

        if (++writePos_ >= frames)
            writePos_ = 0;   // Wrap: behaves like a circular capture buffer.
    }
}

} // namespace sculpt
