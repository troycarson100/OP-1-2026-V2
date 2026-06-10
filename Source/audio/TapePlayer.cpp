#include "TapePlayer.h"
#include "SampleBuffer.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void TapePlayer::prepare (double sampleRate)
{
    (void) sampleRate;   // Speed is a ratio of the material's native rate for now.
    reset();
}

void TapePlayer::reset()
{
    position_ = 0.0;
}

void TapePlayer::setLoopRegion (float start01, float end01)
{
    loopStart_ = clamp01 (start01);
    loopEnd_   = clamp01 (end01);
    if (loopEnd_ < loopStart_ + 0.01f)
        loopEnd_ = clampf (loopStart_ + 0.01f, 0.0f, 1.0f);
}

float TapePlayer::getPositionNormalized (int numFrames) const
{
    return numFrames > 1 ? static_cast<float> (position_) / static_cast<float> (numFrames) : 0.0f;
}

void TapePlayer::process (const SampleBuffer& buffer, float* outL, float* outR, int numSamples)
{
    const int frames = buffer.getNumFrames();
    if (! playing_ || frames < 2)
        return;

    const double regionStart = static_cast<double> (loopStart_) * (frames - 1);
    const double regionEnd   = static_cast<double> (loopEnd_)   * (frames - 1);
    const double regionLen   = regionEnd - regionStart;
    if (regionLen < 2.0)
        return;

    const int rightChannel = buffer.getNumChannels() > 1 ? 1 : 0;

    for (int i = 0; i < numSamples; ++i)
    {
        // Keep the playhead inside the loop region.
        if (position_ < regionStart || position_ >= regionEnd)
        {
            if (! loopMode_)
            {
                playing_ = false;
                return;
            }
            position_ = speed_ >= 0.0f
                          ? regionStart + std::fmod (position_ - regionStart + regionLen, regionLen)
                          : regionEnd   - std::fmod (regionEnd - position_ + regionLen, regionLen);
        }

        const float pos = static_cast<float> (position_);
        outL[i] += buffer.getSampleLinear (0, pos) * level_;
        outR[i] += buffer.getSampleLinear (rightChannel, pos) * level_;

        position_ += static_cast<double> (speed_);
    }
}

} // namespace sculpt
