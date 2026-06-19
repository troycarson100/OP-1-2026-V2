// Portable engine tests: StepSequencer clock->step boundary determinism.
#include <cassert>
#include <cstdio>
#include <vector>

#include "core/StepSequencer.h"

namespace
{
constexpr double kSpb = 22050.0; // samples per beat @ 120 BPM, 44100 Hz

// Drive the sequencer over totalSamples in fixed blocks, collecting every fired step in order.
std::vector<int> run (sculpt::StepSequencer& seq, int totalSamples, int block)
{
    using S = sculpt::StepSequencer;
    int buf[S::kMaxStepsPerBlock];
    std::vector<int> fired;
    for (int pos = 0; pos < totalSamples; )
    {
        const int n = (block < totalSamples - pos) ? block : (totalSamples - pos);
        const int c = seq.advance (kSpb, n, buf, S::kMaxStepsPerBlock);
        for (int i = 0; i < c; ++i)
            fired.push_back (buf[i]);
        pos += n;
    }
    return fired;
}
} // namespace

int main()
{
    using namespace sculpt;
    using S = StepSequencer;

    // 1) Stopped: never fires.
    {
        StepSequencer seq;
        seq.prepare (44100.0);
        int buf[S::kMaxStepsPerBlock];
        assert (seq.advance (kSpb, 256, buf, S::kMaxStepsPerBlock) == 0);
        assert (! seq.isPlaying());
    }

    // 2) start() fires step 0 first, then 1,2,3,... in order with no dupes/gaps.
    {
        StepSequencer seq;
        seq.prepare (44100.0);
        seq.start();
        assert (seq.isPlaying());
        const auto fired = run (seq, static_cast<int> (kSpb * 2.0), 256); // 2 beats = 8 sixteenths
        assert (fired.size() >= 8);
        for (int i = 0; i < 8; ++i)
            assert (fired[static_cast<size_t> (i)] == i);
    }

    // 3) Wraps at the pattern length (default kNumSteps): step after index length-1 is 0.
    {
        StepSequencer seq;
        seq.prepare (44100.0);
        seq.start();
        const auto fired = run (seq, static_cast<int> (kSpb * 8.5), 256); // > 32 sixteenths
        assert (static_cast<int> (fired.size()) >= kNumSteps + 1);
        for (int i = 0; i < kNumSteps; ++i)
            assert (fired[static_cast<size_t> (i)] == i);
        assert (fired[static_cast<size_t> (kNumSteps)] == 0); // wrapped
    }

    // 4) Block size independence: tiny blocks and one big block fire the same step sequence.
    {
        StepSequencer a, b;
        a.prepare (44100.0); b.prepare (44100.0);
        a.start(); b.start();
        const int total = static_cast<int> (kSpb * 4.0);
        const auto fa = run (a, total, 32);
        const auto fb = run (b, total, total); // single giant block
        assert (fa == fb);
    }

    // 5) Custom length wraps correctly.
    {
        StepSequencer seq;
        seq.prepare (44100.0);
        seq.setLength (4);
        seq.start();
        const auto fired = run (seq, static_cast<int> (kSpb * 1.5), 64); // 6 sixteenths over length 4
        assert (fired.size() >= 6);
        const int expected[6] = { 0, 1, 2, 3, 0, 1 };
        for (int i = 0; i < 6; ++i)
            assert (fired[static_cast<size_t> (i)] == expected[i]);
    }

    std::printf ("StepSequencer tests passed\n");
    return 0;
}
