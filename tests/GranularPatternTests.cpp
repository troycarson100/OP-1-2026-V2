// Portable engine tests: grain pattern bank determinism + latch + amount bypass.
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

#include "audio/GranularEngine.h"
#include "audio/SampleBuffer.h"
#include "core/ParameterIds.h"

namespace
{

void fillSineMaterial (sculpt::SampleBuffer& buf, int frames, double sr)
{
    buf.resize (2, frames);
    for (int i = 0; i < frames; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (sr);
        const float s = 0.2f * std::sin (2.0f * 3.14159265f * 220.0f * t);
        buf.setSample (0, i, s);
        buf.setSample (1, i, s);
    }
}

void renderTwoBars (sculpt::GranularEngine& eng, sculpt::SampleBuffer& mat, std::vector<float>& L,
                    std::vector<float>& R)
{
    constexpr double kBpm        = 120.0;
    constexpr int    kSr         = 44100;
    constexpr int    kTwoBars    = static_cast<int> (kSr * (60.0 / kBpm) * 8.0); // 8 quarter notes
    constexpr int    kBlock      = 256;

    L.assign (static_cast<size_t> (kTwoBars), 0.0f);
    R.assign (static_cast<size_t> (kTwoBars), 0.0f);

    double beat = 0.0;
    sculpt::GranularBlockTiming timing {};
    timing.bpm              = kBpm;
    timing.samplesPerBeat   = static_cast<double> (kSr) * 60.0 / kBpm;
    timing.hostPlaying      = true;

    for (int pos = 0; pos < kTwoBars; pos += kBlock)
    {
        const int n = std::min (kBlock, kTwoBars - pos);
        timing.beatAtBlockStart = beat;
        eng.setBlockTiming (timing);
        eng.process (mat, L.data () + pos, R.data () + pos, n);
        beat += static_cast<double> (n) / timing.samplesPerBeat;
    }
}

sculpt::GranularEngine::Params makeSyncedPattern7Params()
{
    sculpt::GranularEngine::Params p {};
    p.position        = 0.35f;
    p.size            = 0.35f;
    p.density         = 7.0f / 13.0f; // table index 7 → 1/4 note divisions
    p.pitch           = 0.5f;
    p.spray           = 0.0f;
    p.texture         = 0.0f; // no humanize randomness
    p.spread          = 0.5f;
    p.mix             = 1.0f;
    p.syncedMode      = true;
    p.steps           = 16;
    p.pulses          = 16;
    p.rotate          = 0;
    p.pitchQuantIndex = 0;
    p.loopStart01     = 0.0f;
    p.loopEnd01       = 1.0f;
    p.grainPattern        = 7.0f / 15.0f; // pattern index 7 (Stutter)
    p.grainPatternAmount  = 1.0f;
    return p;
}

} // namespace

int main()
{
    using sculpt::GranularEngine;
    using sculpt::SampleBuffer;

    // --- Determinism: two identical renders of pattern 7 (Stutter) ---
    {
        SampleBuffer mat;
        fillSineMaterial (mat, 48000, 44100);

        std::vector<float> L1, R1, L2, R2;

        GranularEngine e1;
        e1.prepare (44100.0);
        e1.setRngSeedForTesting (0xC0FFEEu);
        e1.setParams (makeSyncedPattern7Params ());
        e1.setActive (true);
        renderTwoBars (e1, mat, L1, R1);

        GranularEngine e2;
        e2.prepare (44100.0);
        e2.setRngSeedForTesting (0xC0FFEEu);
        e2.setParams (makeSyncedPattern7Params ());
        e2.setActive (true);
        renderTwoBars (e2, mat, L2, R2);

        assert (L1.size () == L2.size () && R1.size () == R2.size ());
        assert (std::memcmp (L1.data (), L2.data (), L1.size () * sizeof (float)) == 0);
        assert (std::memcmp (R1.data (), R2.data (), R1.size () * sizeof (float)) == 0);
    }

    // --- Amount = 0 matches Flat pattern regardless of selected bank index ---
    {
        SampleBuffer mat;
        fillSineMaterial (mat, 24000, 44100);
        std::vector<float> La, Ra, Lb, Rb;

        auto pFlat = makeSyncedPattern7Params ();
        pFlat.grainPattern       = 0.0f;
        pFlat.grainPatternAmount = 1.0f;

        auto pChaos = makeSyncedPattern7Params ();
        pChaos.grainPattern       = 1.0f; // Max index
        pChaos.grainPatternAmount = 0.0f;

        GranularEngine ea;
        ea.prepare (44100.0);
        ea.setRngSeedForTesting (0x111u);
        ea.setParams (pFlat);
        ea.setActive (true);
        renderTwoBars (ea, mat, La, Ra);

        GranularEngine eb;
        eb.prepare (44100.0);
        eb.setRngSeedForTesting (0x111u);
        eb.setParams (pChaos);
        eb.setActive (true);
        renderTwoBars (eb, mat, Lb, Rb);

        assert (La.size () == Lb.size ());
        assert (std::memcmp (La.data (), Lb.data (), La.size () * sizeof (float)) == 0);
        assert (std::memcmp (Ra.data (), Rb.data (), Ra.size () * sizeof (float)) == 0);
    }

    // --- Pattern knob latches on next division boundary (not mid-grid) ---
    {
        SampleBuffer mat;
        fillSineMaterial (mat, 12000, 44100);

        GranularEngine eng;
        eng.prepare (44100.0);
        eng.setRngSeedForTesting (0x222u);

        sculpt::GranularEngine::Params p = makeSyncedPattern7Params ();
        p.grainPattern       = 0.0f;
        p.grainPatternAmount = 1.0f;
        eng.setParams (p);
        eng.setActive (true);
        assert (eng.getGrainPatternActiveIndex () == 0);

        p.grainPattern = 1.0f; // index 15
        eng.setParams (p);

        constexpr double kBpm = 120.0;
        constexpr int    kSr  = 44100;
        sculpt::GranularBlockTiming timing {};
        timing.bpm            = kBpm;
        timing.samplesPerBeat = static_cast<double> (kSr) * 60.0 / kBpm;
        timing.hostPlaying    = true;

        // Stay in (0.25, 0.5) beats without crossing 0.5 (no division boundaries at 0.25 grid).
        double beat = 0.251;
        for (int i = 0; i < 27; ++i)
        {
            timing.beatAtBlockStart = beat;
            eng.setBlockTiming (timing);
            float ol[200], or_[200];
            eng.process (mat, ol, or_, 200);
            beat += 200.0 / timing.samplesPerBeat;
            assert (eng.getGrainPatternActiveIndex () == 0);
        }

        // Cross 0.5 beat (next boundary on 1/4-note grid) → latch applies pattern 15.
        timing.beatAtBlockStart = beat;
        eng.setBlockTiming (timing);
        float ol[512], or_[512];
        eng.process (mat, ol, or_, 300);
        assert (eng.getGrainPatternActiveIndex () == 15);
    }

    return 0;
}
