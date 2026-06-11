#pragma once

#include <array>
#include <atomic>

#include "ParameterState.h"
#include "Clock.h"
#include "Transport.h"
#include "SceneManager.h"
#include "../audio/Track.h"
#include "../audio/SampleBuffer.h"
#include "../audio/Mixer.h"
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

    // Performance actions. Thread-safe: requests are latched and applied at
    // the start of the next audio block.
    void triggerTrack (int trackIndex);
    void stopTrack (int trackIndex);

    void setCurrentScene (int sceneIndex);
    void saveCurrentScene (int sceneIndex);
    void recallScene (int sceneIndex);

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
    void fillMaterialWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope) const;

private:
    void applyPendingRequests();
    void updateModulation (float** inputs, int numInputChannels, int offset, int numSamples,
                           double beatAtBlockStart);
    void processChunk (float** inputs, float** outputs,
                       int numInputChannels, int numOutputChannels,
                       int offset, int numSamples);
    void updateScreenModel();

    void fillMixBusWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope) const;

    ParameterState params_;
    Clock          clock_;
    Transport      transport_;
    SceneManager   sceneManager_;

    std::array<Track, kNumTracks> tracks_;
    Mixer mixer_;

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

    // Latched cross-thread requests (bitmask per track / scene index).
    std::atomic<uint32_t> pendingTriggers_ { 0 };
    std::atomic<uint32_t> pendingStops_ { 0 };
    std::atomic<int>      pendingSceneSave_ { -1 };
    std::atomic<int>      pendingSceneRecall_ { -1 };

    double sampleRate_ = kDefaultSampleRate;
    bool   prepared_   = false;

    std::atomic<int> modLcdSlot_ { 0 };
};

} // namespace sculpt
