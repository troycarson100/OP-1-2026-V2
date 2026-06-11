#include "Engine.h"
#include "../util/MathUtils.h"

namespace sculpt
{

Engine::Engine() = default;

void Engine::prepare (double sampleRate, int blockSize)
{
    (void) blockSize;   // The engine chunks internally at kMaxBlockSize.
    sampleRate_ = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;

    clock_.prepare (sampleRate_);
    mixer_.prepare (sampleRate_);
    inputEnvelope_.prepare (sampleRate_);
    inputEnvelope_.setTimesMs (5.0f, 150.0f);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        tracks_[ts].prepare (sampleRate_, t);
    }

    modEngine_.prepare (sampleRate_);

    macros_.reset();
    prepared_ = true;

    // The prototype should make sound immediately: start all four tracks.
    for (int t = 0; t < kNumTracks; ++t)
        tracks_[static_cast<size_t> (t)].trigger();
}

void Engine::reset()
{
    clock_.reset();
    for (auto& track : tracks_)
        track.reset();
    inputEnvelope_.reset();
    modEngine_.reset();
    trackPeaks_.fill (0.0f);
    masterPeakL_ = masterPeakR_ = 0.0f;
}

// ---- Parameters ------------------------------------------------------------

int Engine::getSelectedTrack() const
{
    const float n = params_.getGlobal (ParameterId::SelectedTrack);
    const int t = static_cast<int> (n * static_cast<float> (kNumTracks - 1) + 0.5f);
    return t < 0 ? 0 : (t >= kNumTracks ? kNumTracks - 1 : t);
}

void Engine::setParameter (ParameterId id, float normalizedValue)
{
    if (isTrackParameter (id))
        params_.setTrack (getSelectedTrack(), id, normalizedValue);
    else
        params_.setGlobal (id, normalizedValue);
}

float Engine::getParameter (ParameterId id) const
{
    return isTrackParameter (id) ? params_.getTrack (getSelectedTrack(), id)
                                 : params_.getGlobal (id);
}

void Engine::setTrackParameter (int trackIndex, ParameterId id, float normalizedValue)
{
    if (isTrackParameter (id))
        params_.setTrack (trackIndex, id, normalizedValue);
    else
        params_.setGlobal (id, normalizedValue);
}

float Engine::getTrackParameter (int trackIndex, ParameterId id) const
{
    return isTrackParameter (id) ? params_.getTrack (trackIndex, id)
                                 : params_.getGlobal (id);
}

// ---- Performance actions (latched, thread-safe) -----------------------------

void Engine::triggerTrack (int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        pendingTriggers_.fetch_or (1u << trackIndex);
}

void Engine::stopTrack (int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        pendingStops_.fetch_or (1u << trackIndex);
}

void Engine::setCurrentScene (int sceneIndex)
{
    recallScene (sceneIndex);
}

void Engine::saveCurrentScene (int sceneIndex)
{
    pendingSceneSave_.store (sceneIndex);
}

void Engine::recallScene (int sceneIndex)
{
    pendingSceneRecall_.store (sceneIndex);
}

void Engine::applyPendingRequests()
{
    const uint32_t triggers = pendingTriggers_.exchange (0);
    const uint32_t stops    = pendingStops_.exchange (0);

    for (int t = 0; t < kNumTracks; ++t)
    {
        if (triggers & (1u << t))
            tracks_[static_cast<size_t> (t)].trigger();
        if (stops & (1u << t))
            tracks_[static_cast<size_t> (t)].stop();
    }

    const int save = pendingSceneSave_.exchange (-1);
    if (save >= 0)
        sceneManager_.saveScene (save, params_, getSelectedTrack(), static_cast<int> (selectedPage_));

    const int recall = pendingSceneRecall_.exchange (-1);
    if (recall >= 0)
        sceneManager_.recallScene (recall, params_);
}

// ---- Capture / host bridge ---------------------------------------------------

void Engine::captureToTrack (int trackIndex, const float** inputs, int numChannels, int numSamples)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        tracks_[static_cast<size_t> (trackIndex)].captureInput (inputs, numChannels, numSamples);
}

void Engine::setCaptureArmed (int trackIndex, bool armed)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        tracks_[static_cast<size_t> (trackIndex)].setCaptureArmed (armed);
}

void Engine::replaceTrackMaterialStereo (int trackIndex, const float* left, const float* right, int numFrames)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        tracks_[static_cast<size_t> (trackIndex)].replaceMaterialStereo (left, right, numFrames);
}

void Engine::setHostTempo (double bpm)       { clock_.setBpm (bpm); }
void Engine::setHostPlaying (bool playing)   { transport_.setPlaying (playing); }
void Engine::setSelectedPage (Page page)
{
    selectedPage_     = page;
    screen_.selectedPage = page; // Keep LCD in sync even before the next process() / updateScreenModel().
}

void Engine::setModPatch (const ModPatch& patch)
{
    modEngine_.setLivePatch (patch);
}

const ModPatch& Engine::getModPatch() const
{
    return modEngine_.getLivePatch();
}

void Engine::triggerModAdsr (int trackIndex, int slotIndex)
{
    modEngine_.triggerAdsr (trackIndex, slotIndex);
}

void Engine::setModLcdSlot (int slotIndex)
{
    const int s = (slotIndex < 0) ? 0
                                  : (slotIndex >= kModSlotsPerTrack ? kModSlotsPerTrack - 1 : slotIndex);
    modLcdSlot_.store (s, std::memory_order_relaxed);
}

// ---- Audio -------------------------------------------------------------------

void Engine::process (float** inputs, float** outputs,
                      int numInputChannels, int numOutputChannels, int numSamples)
{
    if (! prepared_ || outputs == nullptr || numOutputChannels < 1 || numSamples <= 0)
        return;

    applyPendingRequests();

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = (numSamples - offset) < kMaxBlockSize ? (numSamples - offset) : kMaxBlockSize;
        processChunk (inputs, outputs, numInputChannels, numOutputChannels, offset, chunk);
        offset += chunk;
    }

    updateScreenModel();
}

void Engine::updateModulation (float** inputs, int numInputChannels, int offset, int numSamples,
                               double beatAtBlockStart)
{
    if (inputs != nullptr && numInputChannels > 0)
    {
        const float* inPtrs[2] = {
            inputs[0] + offset,
            (numInputChannels > 1 ? inputs[1] : inputs[0]) + offset
        };
        inputEnvelope_.processAudio (inPtrs, numInputChannels > 1 ? 2 : 1, numSamples);
    }

    macros_.setMacro (0, params_.getGlobal (ParameterId::Macro1));
    macros_.setMacro (1, params_.getGlobal (ParameterId::Macro2));
    macros_.setMacro (2, params_.getGlobal (ParameterId::Macro3));
    macros_.setMacro (3, params_.getGlobal (ParameterId::Macro4));

    params_.clearModOffsets();
    modEngine_.apply (params_, inputEnvelope_.getValue(), numSamples,
                      beatAtBlockStart, clock_.getBpm(), sampleRate_);
}

void Engine::processChunk (float** inputs, float** outputs,
                           int numInputChannels, int numOutputChannels,
                           int offset, int numSamples)
{
    lastBusChunkSamples_ = numSamples;

    const double beatAtBlockStart = clock_.getBeatPosition();
    updateModulation (inputs, numInputChannels, offset, numSamples, beatAtBlockStart);
    clock_.advance (numSamples);

    // Capture live input into any armed track.
    if (inputs != nullptr && numInputChannels > 0)
    {
        const float* inPtrs[2] = {
            inputs[0] + offset,
            (numInputChannels > 1 ? inputs[1] : inputs[0]) + offset
        };
        for (auto& track : tracks_)
            track.captureInput (inPtrs, 2, numSamples);
    }

    // Render each track into its bus and track peak levels.
    const float* busLPtrs[kNumTracks];
    const float* busRPtrs[kNumTracks];

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        auto& track = tracks_[ts];

        track.updateParameters (params_, t);
        track.process (busL_[ts].data(), busR_[ts].data(), numSamples);

        float peak = trackPeaks_[ts] * 0.92f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::fabs (busL_[ts][static_cast<size_t> (i)]);
            if (a > peak)
                peak = a;
        }
        trackPeaks_[ts] = peak;

        busLPtrs[t] = busL_[ts].data();
        busRPtrs[t] = busR_[ts].data();

        mixer_.setTrackLevel (t, params_.effective (t, ParameterId::TrackLevel));
        mixer_.setTrackPan (t, params_.effective (t, ParameterId::TrackPan));
    }

    mixer_.setOutputGain (params_.effectiveGlobal (ParameterId::OutputGain));

    float* outL = outputs[0] + offset;
    float* outR = (numOutputChannels > 1 ? outputs[1] : outputs[0]) + offset;
    mixer_.process (busLPtrs, busRPtrs, outL, outR, numSamples);

    // Silence any extra output channels.
    for (int ch = 2; ch < numOutputChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            outputs[ch][offset + i] = 0.0f;

    float pl = masterPeakL_ * 0.92f, pr = masterPeakR_ * 0.92f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float al = std::fabs (outL[i]);
        const float ar = std::fabs (outR[i]);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
    }
    masterPeakL_ = pl;
    masterPeakR_ = pr;
}

void Engine::updateScreenModel()
{
    screen_.selectedTrack = getSelectedTrack();
    screen_.currentScene  = sceneManager_.getCurrentSceneIndex();
    screen_.selectedPage  = selectedPage_;

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        const auto& track = tracks_[ts];
        screen_.trackPlaying[ts]   = track.isPlaying();
        screen_.trackRecording[ts] = track.isCaptureArmed();
        screen_.trackMeter[ts]     = clamp01 (trackPeaks_[ts]);
        screen_.grainActivity[ts]  = track.getGrainActivity();
        screen_.tapePosition[ts]   = track.getTapePositionNormalized();
    }

    screen_.masterMeterL = clamp01 (masterPeakL_);
    screen_.masterMeterR = clamp01 (masterPeakR_);

    const double bpm = clock_.getBpm();
    screen_.displayBpm = static_cast<float> (bpm);
    screen_.bpmValid   = (bpm > 1.0 && bpm < 999.0);

    for (int m = 0; m < kNumMacros; ++m)
        screen_.macroValues[static_cast<size_t> (m)] = macros_.getMacro (m);

    const int selected = screen_.selectedTrack;
    const auto& matBuf = getTrackMaterialBuffer (selected);
    tracks_[static_cast<size_t> (selected)].getEngine().getGranular().fillGrainDisplay (
        matBuf, screen_.grainDisplay.data(), kGrainsPerTrack);

    screen_.materialLoopStart01 = params_.effective (selected, ParameterId::LoopStart);
    screen_.materialLoopEnd01   = params_.effective (selected, ParameterId::LoopEnd);

    int visible = 0;
    screen_.paramModOffset.fill (0.0f);
    for (int slot = 0; slot < kMaxParamsPerPage; ++slot)
    {
        const ParameterId id = PageModel::parameterForSlot (selectedPage_, slot);
        if (id == ParameterId::Count)
            break;
        const auto ss = static_cast<size_t> (slot);
        screen_.paramIds[ss]   = id;
        screen_.paramNames[ss]  = parameterName (id);
        screen_.paramValues[ss] = isTrackParameter (id)
                                    ? params_.effective (selected, id)
                                    : params_.effectiveGlobal (id);
        screen_.paramModOffset[ss] = isTrackParameter (id)
                                         ? params_.getTrackModOffset (selected, id)
                                         : params_.getGlobalModOffset (id);
        ++visible;
    }
    screen_.numVisibleParams = visible;

    // Spectral filter band display for the Filter page.
    const bool spectral = params_.effective (selected, ParameterId::FilterMode) > 0.5f;
    screen_.filterSpectralMode = spectral;
    if (spectral)
    {
        const auto& sf = tracks_[static_cast<size_t> (selected)].getEngine().getSpectralFilter();
        for (int b = 0; b < ScreenModel::kFilterBands; ++b)
            screen_.filterBandGains[static_cast<size_t> (b)] = sf.getBandEnvelope (b);
    }
    else
    {
        screen_.filterBandGains.fill (0.0f);
    }

    if (selectedPage_ == Page::Mod)
    {
        const int    trk    = getSelectedTrack();
        const int    slot   = modLcdSlot_.load (std::memory_order_relaxed);
        const double beatEnd = clock_.getBeatPosition();
        modEngine_.writeModLcdSnapshot (trk, slot, inputEnvelope_.getValue(), beatEnd, screen_.modLcd);
    }
    else
        screen_.modLcd.active = false;

    if (selectedPage_ == Page::Mixer)
    {
        for (int t = 0; t < kNumTracks; ++t)
        {
            const auto ts = static_cast<size_t> (t);
            fillMixBusWaveformEnvelope (t, kMaterialWaveformBins, screen_.mixBusWaveform[ts].data ());

            const float d0 = map::mixEqBandGainDb (params_.effective (t, ParameterId::MixEqLowGain));
            const float d1 = map::mixEqBandGainDb (params_.effective (t, ParameterId::MixEqMidGain));
            const float d2 = map::mixEqBandGainDb (params_.effective (t, ParameterId::MixEqHighGain));
            screen_.mixEqBandNorm[ts][0] = clamp01 ((d0 + 12.0f) / 24.0f);
            screen_.mixEqBandNorm[ts][1] = clamp01 ((d1 + 12.0f) / 24.0f);
            screen_.mixEqBandNorm[ts][2] = clamp01 ((d2 + 12.0f) / 24.0f);

            screen_.mixCompReduction[ts] = clamp01 (tracks_[ts].getEngine().getMixBus().getCompReductionMeter01 ());
        }
    }
}

const SampleBuffer& Engine::getTrackMaterialBuffer (int trackIndex) const
{
    const int ti = (trackIndex < 0) ? 0 : (trackIndex >= kNumTracks ? kNumTracks - 1 : trackIndex);
    return tracks_[static_cast<size_t> (ti)].getMaterial().getBuffer();
}

void Engine::fillMaterialWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope) const
{
    if (outEnvelope == nullptr || numBins <= 0)
        return;

    for (int b = 0; b < numBins; ++b)
        outEnvelope[b] = 0.0f;

    const int ti = (trackIndex < 0) ? 0 : (trackIndex >= kNumTracks ? kNumTracks - 1 : trackIndex);
    const SampleBuffer& buf = tracks_[static_cast<size_t> (ti)].getMaterial().getBuffer();
    const int frames = buf.getNumFrames();
    const int channels = buf.getNumChannels();
    if (frames < 1 || channels < 1)
        return;

    const int rightCh = channels > 1 ? 1 : 0;

    for (int i = 0; i < numBins; ++i)
    {
        int startF = static_cast<int> ((static_cast<long long> (i) * frames) / numBins);
        int endF   = static_cast<int> ((static_cast<long long> (i + 1) * frames) / numBins);
        if (endF <= startF)
            endF = startF + 1;
        if (endF > frames)
            endF = frames;
        if (startF >= frames)
            continue;

        float peak = 0.0f;
        for (int f = startF; f < endF; ++f)
        {
            const float L = buf.getSample (0, f);
            const float R = buf.getSample (rightCh, f);
            const float m = 0.5f * (std::fabs (L) + std::fabs (R));
            if (m > peak)
                peak = m;
        }
        outEnvelope[i] = clamp01 (peak);
    }
}

void Engine::fillMixBusWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope) const
{
    if (outEnvelope == nullptr || numBins <= 0)
        return;

    for (int b = 0; b < numBins; ++b)
        outEnvelope[b] = 0.0f;

    const int ti = (trackIndex < 0) ? 0 : (trackIndex >= kNumTracks ? kNumTracks - 1 : trackIndex);
    const auto ts = static_cast<size_t> (ti);

    const int samples = lastBusChunkSamples_;
    if (samples < 1)
        return;

    const float* L = busL_[ts].data();
    const float* R = busR_[ts].data();

    for (int i = 0; i < numBins; ++i)
    {
        int startF = static_cast<int> ((static_cast<long long> (i) * samples) / numBins);
        int endF   = static_cast<int> ((static_cast<long long> (i + 1) * samples) / numBins);
        if (endF <= startF)
            endF = startF + 1;
        if (endF > samples)
            endF = samples;
        if (startF >= samples)
            continue;

        float peak = 0.0f;
        for (int f = startF; f < endF; ++f)
        {
            const float m = 0.5f * (std::fabs (L[static_cast<size_t> (f)])
                                  + std::fabs (R[static_cast<size_t> (f)]));
            if (m > peak)
                peak = m;
        }
        outEnvelope[i] = clamp01 (peak);
    }
}

} // namespace sculpt
