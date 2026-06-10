#pragma once

namespace sculpt
{

class SampleBuffer;

// Records live input into a SampleBuffer. Real-time safe: the target buffer
// is pre-sized in prepare(), and process() only writes samples.
// Currently a simple wrap-around writer; designed to become a proper
// circular capture buffer with punch-in/out later.
class SampleRecorder
{
public:
    void prepare (SampleBuffer& target);

    void arm()        { armed_ = true; }
    void disarm()     { armed_ = false; recording_ = false; }

    bool isArmed() const     { return armed_; }
    bool isRecording() const { return recording_; }

    // Called from the audio thread with live input.
    void process (const float* const* inputs, int numChannels, int numSamples);

    int getWritePosition() const { return writePos_; }

private:
    SampleBuffer* target_   = nullptr;
    int  writePos_   = 0;
    bool armed_      = false;
    bool recording_  = false;
};

} // namespace sculpt
