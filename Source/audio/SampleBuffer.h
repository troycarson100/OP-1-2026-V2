#pragma once

#include <vector>

namespace sculpt
{

// Owns sample material for one track. Channel-major float storage.
// resize() allocates and must only be called outside the audio thread
// (prepare time or a future loading thread). All reads are real-time safe.
class SampleBuffer
{
public:
    void resize (int numChannels, int numFrames);
    void clear();

    int getNumChannels() const { return numChannels_; }
    int getNumFrames() const   { return numFrames_; }

    float getSample (int channel, int frame) const;
    void  setSample (int channel, int frame, float value);

    // Linear interpolation at a fractional frame position, wrapping at the ends.
    float getSampleLinear (int channel, float framePos) const;

    const float* getChannelData (int channel) const;
    float*       getChannelData (int channel);

private:
    std::vector<float> data_;
    int numChannels_ = 0;
    int numFrames_   = 0;
};

} // namespace sculpt
