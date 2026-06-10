#pragma once

#include <vector>
#include <cstddef>

namespace sculpt
{

// Fixed-capacity float ring buffer.
// resize() must be called outside the audio thread (it allocates).
// push()/read() are real-time safe afterwards.
class RingBuffer
{
public:
    void resize (int capacity)
    {
        data_.assign (static_cast<size_t> (capacity > 0 ? capacity : 1), 0.0f);
        writePos_ = 0;
    }

    void clear()
    {
        for (auto& s : data_)
            s = 0.0f;
        writePos_ = 0;
    }

    int capacity() const { return static_cast<int> (data_.size()); }

    void push (float sample)
    {
        data_[static_cast<size_t> (writePos_)] = sample;
        if (++writePos_ >= capacity())
            writePos_ = 0;
    }

    // Read a sample delayed by `delaySamples` relative to the latest write.
    float read (int delaySamples) const
    {
        const int cap = capacity();
        int idx = writePos_ - 1 - delaySamples;
        while (idx < 0)
            idx += cap;
        return data_[static_cast<size_t> (idx % cap)];
    }

    int getWritePosition() const { return writePos_; }

private:
    std::vector<float> data_;
    int writePos_ = 0;
};

} // namespace sculpt
