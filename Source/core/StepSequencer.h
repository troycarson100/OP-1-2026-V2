#pragma once

#include <cmath>
#include "../util/Constants.h"

namespace sculpt
{

// Portable, real-time-safe step clock. Converts musical time into 1/16-note step
// boundaries over a kNumSteps grid. It owns only the playhead/phase; the trig data
// lives in a Pattern and the act of triggering lives in the Engine, so this class
// stays trivially unit-testable (no JUCE, no audio, no allocation, no locks).
//
// Usage per audio chunk while playing:
//   int steps[kMaxStepsPerBlock];
//   const int n = seq.advance (samplesPerBeat, numSamples, steps, kMaxStepsPerBlock);
//   for (int i = 0; i < n; ++i) ... // steps[i] is a step index 0..length-1 that just started
class StepSequencer
{
public:
    // Generous upper bound on step boundaries that can fall inside one audio chunk.
    // At kMaxBlockSize (2048) and 1/16 notes, even ~30 BPM crosses far fewer than this.
    static constexpr int kMaxStepsPerBlock = 64;

    void prepare (double sampleRate) { (void) sampleRate; reset(); }

    void reset()
    {
        playing_      = false;
        seqBeat_      = 0.0;
        lastFiredN_   = -1;
        currentStep_  = 0;
    }

    // Begin playback from step 0.
    void start()
    {
        seqBeat_     = 0.0;
        lastFiredN_  = -1;
        currentStep_ = 0;
        playing_     = true;
    }

    void stop()              { playing_ = false; }
    bool isPlaying() const   { return playing_; }
    int  currentStep() const { return currentStep_; }

    int  length() const      { return length_; }
    void setLength (int steps)
    {
        length_ = steps < 1 ? 1 : (steps > kNumSteps ? kNumSteps : steps);
    }

    // Advance the grid by numSamples worth of musical time. Writes the indices of any
    // step boundaries that began during this chunk into outSteps (clamped to maxOut) and
    // returns how many. No-op (returns 0) when stopped or timing is degenerate.
    int advance (double samplesPerBeat, int numSamples, int* outSteps, int maxOut)
    {
        if (! playing_ || samplesPerBeat <= 0.0 || numSamples <= 0 || outSteps == nullptr || maxOut <= 0)
            return 0;

        const double beatStart = seqBeat_;
        const double beatEnd   = seqBeat_ + static_cast<double> (numSamples) / samplesPerBeat;
        const double eps       = 1.0e-9;

        int count = 0;
        long long n = static_cast<long long> (std::floor (beatStart / stepBeats_ + eps));
        if (n < 0)
            n = 0;
        for (; static_cast<double> (n) * stepBeats_ <= beatEnd + eps; ++n)
        {
            if (n <= lastFiredN_)
                continue;
            const double boundary = static_cast<double> (n) * stepBeats_;
            if (boundary < beatStart - eps)
                continue;

            const int step = static_cast<int> (((n % length_) + length_) % length_);
            currentStep_ = step;
            lastFiredN_  = n;
            if (count < maxOut)
                outSteps[count] = step;
            ++count;
        }

        seqBeat_ = beatEnd;
        return count < maxOut ? count : maxOut;
    }

private:
    double    stepBeats_   = 0.25;  // 1/16 note
    double    seqBeat_     = 0.0;
    long long lastFiredN_  = -1;
    int       currentStep_ = 0;
    int       length_      = kNumSteps;
    bool      playing_     = false;
};

} // namespace sculpt
