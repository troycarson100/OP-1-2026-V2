#pragma once

#include "MaterialSource.h"
#include "SampleRecorder.h"
#include "TrackEngine.h"
#include "Envelope.h"

namespace sculpt
{

class ParameterState;

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

    // Live input capture into the material buffer.
    void captureInput (const float* const* inputs, int numChannels, int numSamples);
    void setCaptureArmed (bool armed);
    bool isCaptureArmed() const { return recorder_.isArmed(); }

    // Sync all per-track parameters (with modulation applied) into the DSP.
    // When materialPlayheadScrub is true, MaterialPlayhead re-seeks tape even if the track is playing.
    // hostBpm: transport / manual BPM from Clock (Warp mode only).
    void updateParameters (const ParameterState& state, int trackIndex, bool materialPlayheadScrub,
                           double hostBpm);

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
};

} // namespace sculpt
