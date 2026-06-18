#pragma once

namespace sculpt
{

class SampleBuffer;

// Overlap-add grain scrubber for the tape Material stage.
//
// Used in two cases where single-stream looping misbehaves:
//   * Loop Start (forward) / Loop End (reverse) dragged onto the read head — the head must follow
//     the sweeping boundary without pitch scrub or seam clicks.
//   * A small loop window swept by automation — too short for the wrap crossfade to loop cleanly.
//
// It spawns short Hann-windowed grains at the moving target position, each playing at normal speed.
// Overlapping grains (50% hop) sum to constant power, so the result tracks the boundary at constant
// pitch with no seam clicks. The spawn length is settable: matched to the loop length it reproduces
// the loop tone; left at the default it gives a soft scrub smear.
//
// Portable C++; real-time safe (fixed pool, reads only, no allocation/locks).
class TapeGrainScrub
{
public:
    void prepare (double sampleRate);
    void reset();

    // Length of grains spawned from now on (samples). Hop is half this (50% overlap). Applies to
    // future grains only; in-flight grains keep the length they were spawned with.
    void setSpawnLength (int lenSamples);
    int  defaultSpawnLength() const { return defaultLen_; }

    // Begin spawning grains. Idempotent while already engaged; on a fresh engage, requests a prime
    // so the next render pre-fills the full overlap (staggered grains) and the cloud is at full power
    // from the first sample — no build-up dip.
    void engage()    { if (! active_) { hopCounter_ = 0; primePending_ = true; } active_ = true; }
    // Stop spawning; grains already in flight keep sounding until they window out.
    void disengage() { active_ = false; }

    bool isActive() const         { return active_; }
    bool hasSoundingGrains() const;

    // Render one sample. targetPos = desired read position in frames (the moving boundary); speed =
    // playback ratio (grains advance at this rate, so pitch is unchanged). Adds into outL/outR.
    void render (const SampleBuffer& buffer, double targetPos, double speed, float level,
                 int rightChannel, float& outL, float& outR);

private:
    void spawn (double pos, int age = 0);

    struct Grain
    {
        double pos = 0.0; // current read position in frames
        int    age = 0;   // samples since spawn
        int    len = 0;   // this grain's window length (captured at spawn)
        bool   on  = false;
    };

    // 4x overlap needs ~4 active grains; the extra headroom covers a quick re-engage while the
    // previous engagement's grains are still windowing out, so a spawn never steals a sounding grain.
    static constexpr int kMaxGrains = 12;
    Grain  grains_[kMaxGrains];

    int    defaultLen_ = 1764; // ~40 ms, set in prepare()
    int    spawnLen_   = 1764; // length for the next spawned grain
    int    hop_        = 882;  // spawnLen_ / 2 -> 50% overlap (Hann sums to ~constant power)
    int    hopCounter_   = 0;
    bool   active_       = false;
    bool   primePending_ = false; // pre-fill the overlap on the next render after a fresh engage
    double sampleRate_   = 44100.0;
};

} // namespace sculpt
