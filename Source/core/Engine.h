#pragma once

#include <array>
#include <atomic>

#include "ParameterState.h"
#include "Clock.h"
#include "Transport.h"
#include "SceneManager.h"
#include "PatternManager.h"
#include "StepSequencer.h"
#include "../audio/Track.h"
#include "../audio/SampleBuffer.h"
#include "../audio/Mixer.h"
#include "../audio/Metronome.h"
#include "../audio/SpaceSendBus.h"
#include "../util/SmoothedValue.h"
#include "../modulation/EnvelopeFollower.h"
#include "../modulation/MacroControls.h"
#include "../modulation/ModEngine.h"
#include "../ui/ScreenModel.h"
#include "../util/Constants.h"

namespace sculpt
{

// The portable instrument. Owns all tracks, modulation, scenes, timing and
// the screen model. Completely JUCE-free: the wrapper feeds it raw audio
// pointers, normalized parameters and host tempo, nothing else.
class Engine
{
public:
    Engine();

    // Allocates scratch/material buffers. Not real-time safe.
    void prepare (double sampleRate, int blockSize);
    void reset();

    // Real-time safe after prepare(). Splits oversized host blocks internally.
    void process (float** inputs, float** outputs,
                  int numInputChannels, int numOutputChannels, int numSamples);

    // Global parameters set directly; track parameters go to the selected track.
    void  setParameter (ParameterId id, float normalizedValue);
    float getParameter (ParameterId id) const;

    void  setTrackParameter (int trackIndex, ParameterId id, float normalizedValue);
    float getTrackParameter (int trackIndex, ParameterId id) const;
    // Effective value (base or active step-lock, plus modulation) — for UI to show locks during playback.
    float getEffectiveTrackParameter (int trackIndex, ParameterId id) const;

    // Performance actions. Thread-safe: requests are latched and applied at
    // the start of the next audio block.
    void triggerTrack (int trackIndex);
    void stopTrack (int trackIndex);

    // Per-track mute (performance state). A muted track's level is taken to 0 in the mix (smoothed).
    void setTrackMuted (int trackIndex, bool muted);
    void toggleTrackMuted (int trackIndex);
    bool isTrackMuted (int trackIndex) const;

    void requestSpaceClear (int trackIndex);

    void setCurrentScene (int sceneIndex);
    void saveCurrentScene (int sceneIndex);
    void recallScene (int sceneIndex);

    // Plugin state (message thread). Portable performance data the host param tree doesn't cover:
    // sequencer patterns (trigs + p-locks), scenes, and per-track mutes. Stored/restored as raw
    // trivially-copyable blobs (the wrapper compresses them); load is a no-op on a size mismatch.
    size_t   patternStateBytes() const;
    void     savePatternState (void* dst) const;
    void     loadPatternState (const void* src, size_t numBytes);
    size_t   sceneStateBytes() const;
    void     saveSceneState (void* dst) const;
    void     loadSceneState (const void* src, size_t numBytes);
    uint32_t getMuteMask() const;
    void     setMuteMask (uint32_t mask);

    // Master transport: start the sequencer AND launch all Torso-machine tracks at once (Sampler
    // tracks are driven by the sequencer); stop halts the sequencer and all tracks. Latched.
    void masterPlay();
    void masterStop();

    // Step sequencer (master transport). Thread-safe: play/stop are latched and applied at the
    // start of the next audio block; trig edits write the live pattern directly (message thread).
    void startSequencer();
    void stopSequencer();
    bool isSequencerPlaying() const     { return stepSeq_.isPlaying(); }
    int  getSequencerCurrentStep() const { return stepSeq_.currentStep(); }

    // Metronome click toggle (UI thread). Clicks every beat, accented on the bar downbeat.
    void setMetronomeEnabled (bool on) { metronomeEnabled_.store (on, std::memory_order_relaxed); }
    bool isMetronomeEnabled() const    { return metronomeEnabled_.load (std::memory_order_relaxed); }

    // Pattern editing (message / UI thread).
    void toggleStepTrig (int track, int step);
    void setStepTrig (int track, int step, bool on);
    bool getStepTrig (int track, int step) const;
    int  getCurrentPatternIndex() const { return patternMgr_.getCurrentIndex(); }
    void setCurrentPatternIndex (int idx);

    // Per-step parameter locks (message / UI thread). paramId is a track ParameterId.
    void setStepLock (int track, int step, ParameterId id, float value);
    void clearStepLock (int track, int step, ParameterId id);
    void clearStepLocks (int track, int step);
    bool stepHasLock (int track, int step, ParameterId id) const;
    bool getStepLock (int track, int step, ParameterId id, float& out) const;
    int  stepLockCount (int track, int step) const;

    // Direct capture entry point (audio thread only).
    void captureToTrack (int trackIndex, const float** inputs, int numChannels, int numSamples);
    void setCaptureArmed (int trackIndex, bool armed);

    // Message thread only (call with audio processing suspended).
    void replaceTrackMaterialStereo (int trackIndex, const float* left, const float* right, int numFrames);

    // Host bridge inputs (values only - no host types).
    void setHostTempo (double bpm);
    void setHostPlaying (bool playing);

    void setSelectedPage (Page page);
    Page getSelectedPage() const          { return selectedPage_; }
    int  getSelectedTrack() const;

    // Granular LCD + hardware editor: two 8-slot encoder pages (0 = core, 1 = sync/Euclidean/quant + patterns).
    void setGranularEncoderPage (int pageIndex);
    int  getGranularEncoderPage() const;

    void        setModPatch (const ModPatch& patch);
    const ModPatch& getModPatch() const;
    void        triggerModAdsr (int trackIndex, int slotIndex);

    // Which mod slot the Mod page LCD follows (0..kModSlotsPerTrack-1). UI thread may set.
    void setModLcdSlot (int slotIndex);

    const ScreenModel& getScreenModel() const { return screen_; }
    SceneManager&      getSceneManager()      { return sceneManager_; }

    // Message / UI thread only: read material PCM for visualization.
    const SampleBuffer& getTrackMaterialBuffer (int trackIndex) const;

    // Message / UI thread only: peak envelope (one value per bin, 0..1) for waveform LCD.
    // When applyWaveZoom is true, uses MaterialWaveZoom + tape position to window the view.
    void fillMaterialWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope,
                                       bool applyWaveZoom) const;

    // Host/UI: true while the user is actively gesturing the Material playhead control for a
    // track (begin/end change gesture). Lets scrub override live tape during playback.
    void setMaterialPlayheadScrubActive (int trackIndex, bool active);

private:
    float materialWaveCenter01 (int trackIndex) const;
    // Normalized loop-snap grid step for a track (tempo note value / buffer duration).
    float gridStep01ForTrack (int trackIndex) const;

    void applyPendingRequests();
    void fireWarpLaunchDeadlines (double beatAtBlockStart);
    void executeWarpLaunchForTrack (int trackIndex, double targetHostBeat);
    void maybeResyncWarpPlayheadsAfterVarispeedChange (double beatNow);
    float warpEffectiveSpeedRatio (int trackIndex) const;
    bool trackIsWarpMode (int trackIndex) const;
    int  findReferenceWarpPlayingTrack (int excludeTrack) const;
    void updateModulation (float** inputs, int numInputChannels, int offset, int numSamples,
                           double beatAtBlockStart);
    void processChunk (float** inputs, float** outputs,
                       int numInputChannels, int numOutputChannels,
                       int offset, int numSamples);
    void updateScreenModel();

    void fillMixBusWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope) const;

    void advanceStepSequencer (double samplesPerBeat, int numSamples);

    ParameterState params_;
    Clock          clock_;
    Transport      transport_;
    SceneManager   sceneManager_;
    PatternManager patternMgr_;
    StepSequencer  stepSeq_;
    // Clock beat captured when the master transport last started. Tempo-synced modulation references
    // (clockBeat - this) so a synced LFO's phase resets to 0 on PLAY and lines up with the sequencer
    // bar/step grid, while the free-running clock itself (granular/warp timing) is left untouched.
    double         transportBeatOrigin_ = 0.0;

    std::array<Track, kNumTracks> tracks_;
    Mixer mixer_;
    // Instrument-wide reverb/delay send FX (shared by all tracks) + per-track send smoothers and the
    // summed send-bus scratch. Replaces the old per-track SpaceStage (O(1) FX cost, the Pi CPU win).
    SpaceSendBus spaceSend_;
    std::array<SmoothedValue, kNumTracks> reverbSendSm_;
    std::array<SmoothedValue, kNumTracks> delaySendSm_;
    std::array<float, kMaxBlockSize> delaySendL_ {}, delaySendR_ {}, revSendL_ {}, revSendR_ {};
    Metronome metronome_;
    std::atomic<bool> metronomeEnabled_ { false };

    EnvelopeFollower inputEnvelope_;
    MacroControls    macros_;
    ModEngine        modEngine_;

    ScreenModel screen_;
    Page        selectedPage_ = Page::Granular;

    // Track bus scratch buffers (fixed size, allocated as members).
    std::array<std::array<float, kMaxBlockSize>, kNumTracks> busL_ {}, busR_ {};
    std::array<float, kNumTracks> trackPeaks_ {};
    float masterPeakL_ = 0.0f, masterPeakR_ = 0.0f;

    int lastBusChunkSamples_ = 0;

    // True while the Material playhead control is in an active change gesture (scrub).
    std::array<std::atomic<bool>, kNumTracks> materialPlayheadScrubActive_ {};

    // Per-track mute (UI thread sets, audio thread reads when summing the mix).
    std::array<std::atomic<bool>, kNumTracks> trackMuted_ {};

    // Warp mode: quantize PLAY to the next host beat; align loop phase to another playing warp track when present.
    std::array<bool, kNumTracks>   warpLaunchPending_{};
    std::array<double, kNumTracks> warpLaunchTargetBeat_{};
    // Last-processed warp tape speed ratio per track (NaN = idle). Used to detect varispeed edits while playing.
    std::array<float, kNumTracks> warpSpeedForResyncCompare_{};

    // Latched cross-thread requests (bitmask per track / scene index).
    std::atomic<uint32_t> pendingTriggers_ { 0 };
    std::atomic<uint32_t> pendingStops_ { 0 };
    std::atomic<int>      pendingSceneSave_ { -1 };
    std::atomic<int>      pendingSceneRecall_ { -1 };
    // Step sequencer transport command latched from the UI: 0 = none, 1 = start, 2 = stop.
    std::atomic<int>      pendingSeqCmd_ { 0 };
    // Master transport command latched from the UI: 0 = none, 1 = play-all, 2 = stop-all.
    std::atomic<int>      pendingMasterCmd_ { 0 };

    double sampleRate_ = kDefaultSampleRate;
    bool   prepared_   = false;

    std::atomic<int> modLcdSlot_ { 0 };
    std::atomic<int> granularEncoderPage_ { 0 };
};

} // namespace sculpt
