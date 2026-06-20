#pragma once

#include <array>
#include <atomic>

#include "SpaceDelay.h"
#include "HallReverb.h"
#include "../util/Constants.h"

namespace sculpt
{

struct GranularBlockTiming;

// Instrument-wide send FX: ONE stereo delay + ONE hall reverb shared by all tracks (the Digitakt
// aux model). Each track contributes to the send buses scaled by its per-track Reverb/Delay Send;
// processAdd() processes the summed sends and adds the wet returns onto the master output. This
// replaces the old per-track SpaceStage so reverb/delay cost is O(1), not O(tracks) — the big
// Raspberry-Pi CPU win. Portable, real-time safe: no allocation, no locks, no file I/O.
class SpaceSendBus
{
public:
    void prepare (double sampleRate);
    void reset();

    void requestClear() { clearRequested_.store (true, std::memory_order_release); }

    // Global character (all normalized 0..1, except freeze). timing supplies tempo for synced delay.
    void setParams (float delayTime01, float delayFb01, float delayTimeMode01,
                    float revSize01, float revDecay01, float damp01, float spread01,
                    bool freeze, const GranularBlockTiming& timing);

    // delaySend / revSend are already scaled by each track's send level and summed across tracks.
    // Adds the wet delay + reverb returns onto outL/outR (which already hold the dry master mix).
    void processAdd (const float* delaySendL, const float* delaySendR,
                     const float* revSendL, const float* revSendR,
                     float* outL, float* outR, int numSamples);

    // LCD visuals (selected on the Space page).
    float getDelaySeconds() const       { return delaySeconds_; }
    float getReverbDecaySeconds() const { return reverbDecaySeconds_; }
    float getDelayWet01() const         { return delayWet01_; }
    float getReverbWet01() const        { return reverbWet01_; }

private:
    double sampleRate_ = 44100.0;

    SpaceDelay delay_;
    HallReverb reverb_;

    std::atomic<bool> clearRequested_ { false };

    std::array<float, kMaxBlockSize> wetDL_ {}, wetDR_ {}, wetRL_ {}, wetRR_ {}, monoRv_ {};

    float delaySeconds_       = 0.25f;
    float reverbDecaySeconds_ = 2.0f;
    float delayWet01_  = 0.0f;
    float reverbWet01_ = 0.0f;

    // CPU bypass: skip an engine once its input is silent AND its wet output has fully decayed
    // (measured on the real wet, so arbitrarily long reverb tails ring out before we bypass).
    bool delayBypassed_  = true;
    bool reverbBypassed_ = true;
    bool delaySilent_    = true;
    bool reverbSilent_   = true;
};

} // namespace sculpt
