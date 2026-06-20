#pragma once

#include "SampleBuffer.h"

namespace sculpt
{

// Material stage: owns the source audio for one track.
// Holds placeholder material, live input capture, or audio loaded from disk
// (decoded in the JUCE wrapper, copied into the buffer on the message thread).
class MaterialSource
{
public:
    enum class PlaceholderType
    {
        Tone,       // soft sine-based tone
        Wave,       // darker harmonic wave
        Noise,      // filtered noise texture
        Pulse       // chord-like pulsing texture
    };

    // Allocates the buffer. Call outside the audio thread only.
    void prepare (double sampleRate, float seconds);

    // Fill the buffer with loopable generated material.
    void generatePlaceholder (PlaceholderType type);

    // Replace buffer with stereo PCM (message / offline thread only). Right may
    // be null for mono (duplicated to both channels). Frames capped by caller.
    void loadStereoPCM (const float* left, const float* right, int numFrames);

    SampleBuffer&       getBuffer()       { return buffer_; }
    const SampleBuffer& getBuffer() const { return buffer_; }

    bool hasMaterial() const { return buffer_.getNumFrames() > 0; }

    // True after loadStereoPCM; false after generated placeholder. Used so prepare()
    // does not wipe user-imported audio when regenerating defaults.
    bool hasUserMaterial() const noexcept { return userMaterial_; }

    // External bank buffer: when set, getActiveBuffer() returns this instead of the owned buffer.
    // Set by Engine each block based on MaterialSampleSlot. RT-safe (pointer assignment only).
    void                 setExternalBuffer (const SampleBuffer* buf) { externalBuf_ = buf; }
    const SampleBuffer&  getActiveBuffer() const { return externalBuf_ ? *externalBuf_ : buffer_; }

    // Human-readable name of the loaded sample (empty = placeholder). Written on file load.
    void        setSampleName (const char* name);
    const char* getSampleName() const { return name_; }

private:
    SampleBuffer        buffer_;
    const SampleBuffer* externalBuf_ = nullptr;
    double              sampleRate_  = 44100.0;
    bool                userMaterial_ = false;
    char                name_[64]    {};
};

} // namespace sculpt
