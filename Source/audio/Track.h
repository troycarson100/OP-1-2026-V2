#pragma once

#include "MaterialSource.h"
#include "SampleRecorder.h"
#include "TrackEngine.h"
#include "Envelope.h"
#include "SliceMap.h"

namespace sculpt
{

class ParameterState;
struct GranularBlockTiming;

// One of the four performance tracks. Owns its material, capture, trigger
// state and DSP chain. Pulls its normalized parameters from ParameterState
// once per block - no other class pushes values into the DSP directly.
class Track
{
public:
    void prepare (double sampleRate, int trackIndex);
    void reset();

    void trigger();
    // Warp launch: seek material playhead into the loop then start (same as trigger() otherwise).
    void triggerWithWarpPlayhead (float materialPlayhead01, float loopStart01, float loopEnd01);
    void stop();
    bool isPlaying() const { return playing_; }

    // Sampler-machine trig (step sequencer): on the next updateParameters, seek the read head to
    // pos01 within the (snapped) loop region and re-fire playback from there. pos01 < 0 means "loop
    // start". Deferred to updateParameters so the seek uses the same snapped loop bounds the tape
    // gets this block. Phase 4 p-locks will pass a per-step start position here.
    void requestSamplerTrig (float pos01 = -1.0f) { samplerTrigPending_ = true; samplerTrigPos01_ = pos01; }

    // Restart the round-robin slice cursor (called when the sequencer starts, for predictable patterns).
    void resetSliceCursor() { sliceCursor_ = 0; }

    // Live input capture into the material buffer.
    void captureInput (const float* const* inputs, int numChannels, int numSamples);
    void setCaptureArmed (bool armed);
    bool isCaptureArmed() const { return recorder_.isArmed(); }

    // Sync all per-track parameters (with modulation applied) into the DSP.
    // When materialPlayheadScrub is true, MaterialPlayhead re-seeks tape even if the track is playing.
    // hostBpm: transport / manual BPM from Clock (Warp mode only).
    // granularTiming: beat at chunk start, samples/beat, etc. (must match Engine clock chunk).
    void updateParameters (const ParameterState& state, int trackIndex, bool materialPlayheadScrub,
                           double hostBpm, const GranularBlockTiming& granularTiming,
                           double engineSampleRate);

    // Message thread / engine: flush delay+reverb buffers (latched to next audio block via Engine).
    void clearSpaceBuffers();

    // Overwrites outL/outR with this track's output.
    void process (float* outL, float* outR, int numSamples);

    // 0..1 amount of grain voices in use (for the screen model).
    float getGrainActivity() const;
    float getTapePositionNormalized() const;

    MaterialSource&       getMaterial()       { return material_; }
    const MaterialSource& getMaterial() const { return material_; }

    TrackEngine&       getEngine()       { return engine_; }
    const TrackEngine& getEngine() const { return engine_; }

    // Message thread: replaces material audio (used after decoding a file).
    void replaceMaterialStereo (const float* left, const float* right, int numFrames);

private:
    MaterialSource material_;
    SampleRecorder recorder_;
    TrackEngine    engine_;
    Envelope       gate_;

    bool playing_ = false;
    // After STOP, skip mapping APVTS playhead → tape until the user scrubs the playhead again.
    // The stored playhead param can lag live tape during play; snapping it on stop caused a click.
    bool ignoreStoppedPlayheadSeek_ = false;

    // Pending sampler-machine trig from the step sequencer (applied in updateParameters).
    bool  samplerTrigPending_ = false;
    float samplerTrigPos01_   = -1.0f;

    // Slice mode: round-robin cursor + the region the current one-shot is playing (a slice, persisted
    // across blocks so the tape keeps the slice bounds between trigs instead of the loop knobs).
    SliceMap sliceMap_;
    int   sliceCursor_     = 0;
    float samplerRegionLo_ = 0.0f;
    float samplerRegionHi_ = 1.0f;
};

} // namespace sculpt
