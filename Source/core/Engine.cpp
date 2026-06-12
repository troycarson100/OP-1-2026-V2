#include "Engine.h"
#include "../audio/GranularEngine.h"
#include "../audio/WarpLaunchSync.h"
#include "../util/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sculpt
{
namespace
{
    void computeMaterialWaveView (int frames, float zoom01, float center01, float& view0, float& view1)
    {
        if (frames < 2)
        {
            view0 = 0.0f;
            view1 = 1.0f;
            return;
        }

        const float lastF = static_cast<float> (frames - 1);
        const float frac  = map::materialWaveVisibleFraction (zoom01);
        if (frac >= 0.98f)
        {
            view0 = 0.0f;
            view1 = 1.0f;
            return;
        }

        const float span  = std::max (1.0f, lastF * frac);
        const float focus = clamp01 (center01) * lastF;
        float       start = focus - 0.5f * span;
        if (start < 0.0f)
            start = 0.0f;
        if (start + span > lastF)
            start = lastF - span;

        view0 = start / lastF;
        view1 = (start + span) / lastF;
    }
} // namespace

Engine::Engine()
{
    for (auto& v : warpSpeedForResyncCompare_)
        v = std::numeric_limits<float>::quiet_NaN();
}

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
    warpLaunchPending_.fill (false);
    for (auto& v : warpSpeedForResyncCompare_)
        v = std::numeric_limits<float>::quiet_NaN();
    for (auto& scrub : materialPlayheadScrubActive_)
        scrub.store (false, std::memory_order_relaxed);
    inputEnvelope_.reset();
    modEngine_.reset();
    trackPeaks_.fill (0.0f);
    masterPeakL_ = masterPeakR_ = 0.0f;
}

void Engine::setMaterialPlayheadScrubActive (int trackIndex, bool active)
{
    if (trackIndex < 0 || trackIndex >= kNumTracks)
        return;
    materialPlayheadScrubActive_[static_cast<size_t> (trackIndex)].store (active, std::memory_order_relaxed);
}

float Engine::materialWaveCenter01 (int trackIndex) const
{
    const int ti = trackIndex < 0 ? 0 : (trackIndex >= kNumTracks ? kNumTracks - 1 : trackIndex);
    const auto ts = static_cast<size_t> (ti);
    const auto& trk = tracks_[ts];
    const bool usePlayhead = materialPlayheadScrubActive_[ts].load (std::memory_order_relaxed)
                           || ! trk.isPlaying();
    if (usePlayhead)
        return params_.effective (ti, ParameterId::MaterialPlayhead);
    return trk.getTapePositionNormalized();
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

bool Engine::trackIsWarpMode (int trackIndex) const
{
    return params_.effective (trackIndex, ParameterId::MaterialTimeMode) > 0.5f;
}

int Engine::findReferenceWarpPlayingTrack (int excludeTrack) const
{
    for (int r = 0; r < kNumTracks; ++r)
    {
        if (r == excludeTrack)
            continue;
        const auto rs = static_cast<size_t> (r);
        if (! tracks_[rs].isPlaying())
            continue;
        if (! trackIsWarpMode (r))
            continue;
        return r;
    }
    return -1;
}

float Engine::warpEffectiveSpeedRatio (int trackIndex) const
{
    const double hostBpm = clock_.getBpm();
    float        rootBpm = map::sampleRootBpm (params_.effective (trackIndex, ParameterId::SampleRootBpm));
    if (rootBpm < 1.0e-3f)
        rootBpm = 120.0f;
    const float tapeKnob = params_.effective (trackIndex, ParameterId::TapeSpeed);
    const bool  snapOn   = params_.effective (trackIndex, ParameterId::TapeSpeedSnap) > 0.5f;
    return warpLaunchSync::warpTapeSpeedRatio (tapeKnob, snapOn, rootBpm, hostBpm);
}

void Engine::maybeResyncWarpPlayheadsAfterVarispeedChange (double beatNow)
{
    constexpr float nanV = std::numeric_limits<float>::quiet_NaN();
    const auto      prevSnap = warpSpeedForResyncCompare_;

    const double hostBpm = clock_.getBpm();

    std::array<float, kNumTracks> speedNow {};
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        if (tracks_[ts].isPlaying() && trackIsWarpMode (t))
            speedNow[ts] = warpEffectiveSpeedRatio (t);
        else
            speedNow[ts] = nanV;
    }

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        Track&     trk = tracks_[ts];

        if (! trk.isPlaying() || ! trackIsWarpMode (t))
        {
            warpSpeedForResyncCompare_[ts] = nanV;
            continue;
        }

        const float sNow = speedNow[ts];
        const float prev = prevSnap[ts];

        const float mag       = std::max (1.0e-6f, std::fabs (sNow));
        const float threshold = std::max (5.0e-4f, 0.002f * mag);

        bool refSpeedChanged = false;
        const int ref        = findReferenceWarpPlayingTrack (t);
        if (ref >= 0)
        {
            const auto rs    = static_cast<size_t> (ref);
            const float sRef = speedNow[rs];
            const float pRef = prevSnap[rs];
            if (! std::isnan (sRef) && ! std::isnan (pRef))
            {
                const float magR       = std::max (1.0e-6f, std::fabs (sRef));
                const float thresholdR = std::max (5.0e-4f, 0.002f * magR);
                refSpeedChanged        = std::fabs (sRef - pRef) > thresholdR;
            }
        }

        const bool selfChanged
            = (! std::isnan (prev)) && (std::fabs (sNow - prev) > threshold);

        if (! selfChanged && ! refSpeedChanged)
        {
            warpSpeedForResyncCompare_[ts] = sNow;
            continue;
        }

        float rootBpm = map::sampleRootBpm (params_.effective (t, ParameterId::SampleRootBpm));
        if (rootBpm < 1.0e-3f)
            rootBpm = 120.0f;
        const double bpmForWarp = (hostBpm > 1.0 && hostBpm < 999.0) ? hostBpm : static_cast<double> (rootBpm);

        const float loopS  = params_.effective (t, ParameterId::LoopStart);
        const float loopE  = params_.effective (t, ParameterId::LoopEnd);
        const int   frames = trk.getMaterial().getBuffer().getNumFrames();

        float ph01;
        if (ref >= 0)
        {
            const auto rs = static_cast<size_t> (ref);
            ph01 = warpLaunchSync::materialPlayheadForRefPhase (
                tracks_[rs].getTapePositionNormalized(),
                params_.effective (ref, ParameterId::LoopStart),
                params_.effective (ref, ParameterId::LoopEnd),
                loopS, loopE);
        }
        else
        {
            ph01 = warpLaunchSync::materialPlayheadForBeatPhase (beatNow, frames, loopS, loopE, sampleRate_,
                                                              sNow, bpmForWarp);
        }

        trk.getEngine().getTape().seekNormalized (ph01, frames, loopS, loopE);
        warpSpeedForResyncCompare_[ts] = sNow;
    }
}

void Engine::executeWarpLaunchForTrack (int trackIndex, double targetHostBeat)
{
    const auto ts = static_cast<size_t> (trackIndex);
    warpLaunchPending_[ts] = false;

    if (! trackIsWarpMode (trackIndex))
    {
        tracks_[ts].trigger();
        return;
    }

    Track&       trk      = tracks_[ts];
    const double hostBpm = clock_.getBpm();
    const float  loopS   = params_.effective (trackIndex, ParameterId::LoopStart);
    const float  loopE   = params_.effective (trackIndex, ParameterId::LoopEnd);
    const int    frames  = trk.getMaterial().getBuffer().getNumFrames();
    float        rootBpm = map::sampleRootBpm (params_.effective (trackIndex, ParameterId::SampleRootBpm));
    if (rootBpm < 1.0e-3f)
        rootBpm = 120.0f;

    const double bpmForWarp = (hostBpm > 1.0 && hostBpm < 999.0) ? hostBpm : static_cast<double> (rootBpm);
    const float  speed      = warpEffectiveSpeedRatio (trackIndex);

    const int ref = findReferenceWarpPlayingTrack (trackIndex);
    float     ph01;
    if (ref >= 0)
    {
        const auto rs = static_cast<size_t> (ref);
        ph01 = warpLaunchSync::materialPlayheadForRefPhase (
            tracks_[rs].getTapePositionNormalized(),
            params_.effective (ref, ParameterId::LoopStart),
            params_.effective (ref, ParameterId::LoopEnd),
            loopS, loopE);
    }
    else
    {
        ph01 = warpLaunchSync::materialPlayheadForBeatPhase (targetHostBeat, frames, loopS, loopE, sampleRate_,
                                                              speed, bpmForWarp);
    }

    trk.triggerWithWarpPlayhead (ph01, loopS, loopE);
    warpSpeedForResyncCompare_[ts] = speed;
}

void Engine::fireWarpLaunchDeadlines (double beatAtBlockStart)
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        if (! warpLaunchPending_[ts])
            continue;
        if (beatAtBlockStart + 1.0e-4 >= warpLaunchTargetBeat_[ts])
            executeWarpLaunchForTrack (t, warpLaunchTargetBeat_[ts]);
    }
}

void Engine::applyPendingRequests()
{
    const uint32_t triggers = pendingTriggers_.exchange (0);
    const uint32_t stops    = pendingStops_.exchange (0);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        if (stops & (1u << t))
        {
            warpLaunchPending_[ts] = false;
            warpSpeedForResyncCompare_[ts] = std::numeric_limits<float>::quiet_NaN();
            tracks_[ts].stop();
        }
    }

    for (int t = 0; t < kNumTracks; ++t)
    {
        if (! (triggers & (1u << t)))
            continue;

        const auto ts = static_cast<size_t> (t);
        if (! trackIsWarpMode (t))
        {
            warpLaunchPending_[ts] = false;
            tracks_[ts].trigger();
            continue;
        }

        const double beat = clock_.getBeatPosition();
        const double qb   = warpLaunchSync::warpLaunchQuantizedBeat (beat);
        if (warpLaunchSync::warpLaunchIsImmediate (beat, qb))
        {
            warpLaunchPending_[ts] = false;
            executeWarpLaunchForTrack (t, qb);
        }
        else
        {
            warpLaunchPending_[ts]     = true;
            warpLaunchTargetBeat_[ts] = qb;
        }
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
    if (page != Page::Granular)
    {
        granularEncoderPage_.store (0, std::memory_order_relaxed);
        screen_.granularEncoderPage = 0;
    }
}

void Engine::setGranularEncoderPage (int page01)
{
    const int p = (page01 > 0) ? 1 : 0;
    granularEncoderPage_.store (p, std::memory_order_relaxed);
    screen_.granularEncoderPage = static_cast<uint8_t> (p);
}

int Engine::getGranularEncoderPage() const
{
    return granularEncoderPage_.load (std::memory_order_relaxed);
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

    const double beatForTiming = clock_.getBeatPosition();
    fireWarpLaunchDeadlines (beatForTiming);
    maybeResyncWarpPlayheadsAfterVarispeedChange (beatForTiming);

    const double beatAtBlockStart = beatForTiming;
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

    const double beatAtChunkStart = beatAtBlockStart;
    const double hostBpm          = clock_.getBpm();
    const bool   bpmOk            = hostBpm > 1.0 && hostBpm < 999.0;
    const double bpmUse           = bpmOk ? hostBpm : 120.0;
    GranularBlockTiming granularTiming {};
    granularTiming.beatAtBlockStart = beatAtChunkStart;
    granularTiming.bpm              = bpmUse;
    granularTiming.samplesPerBeat = sampleRate_ * 60.0 / bpmUse;
    granularTiming.hostPlaying    = transport_.isPlaying();

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        auto& track = tracks_[ts];

        const bool playheadScrub = materialPlayheadScrubActive_[ts].load (std::memory_order_relaxed);
        track.updateParameters (params_, t, playheadScrub, bpmUse, granularTiming);
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
    screen_.selectedTrackMaterialTimeMode01 =
        params_.effective (selected, ParameterId::MaterialTimeMode);
    screen_.selectedTrackTapeSpeedSnap01 =
        params_.effective (selected, ParameterId::TapeSpeedSnap);

    const auto& matBuf = getTrackMaterialBuffer (selected);
    const auto& grSel  = tracks_[static_cast<size_t> (selected)].getEngine().getGranular();
    grSel.fillGrainDisplay (matBuf, screen_.grainDisplay.data(), kGrainsPerTrack);

    screen_.granularPattern.syncOn        = grSel.getPatternSyncOn();
    screen_.granularPattern.divisionIndex  = grSel.getPatternDivisionIndex();
    screen_.granularPattern.steps         = grSel.getPatternSteps();
    screen_.granularPattern.pulses         = grSel.getPatternPulses();
    screen_.granularPattern.rotate         = grSel.getPatternRotate();
    screen_.granularPattern.currentStep    = grSel.getPatternCurrentStep();
    screen_.granularPattern.mask           = grSel.getPatternMask();

    const int matFrameCount = matBuf.getNumFrames();
    if (selectedPage_ == Page::Granular && matFrameCount > 1)
    {
        const float tf = static_cast<float> (matFrameCount);
        grSel.getGrainFocusWindow01 (tf, screen_.grainFocusStart01, screen_.grainFocusLen01);
    }
    else
    {
        screen_.grainFocusStart01 = 0.0f;
        screen_.grainFocusLen01   = 0.0f;
    }

    screen_.materialLoopStart01 = params_.effective (selected, ParameterId::LoopStart);
    screen_.materialLoopEnd01   = params_.effective (selected, ParameterId::LoopEnd);

    if (prepared_ && sampleRate_ > 1.0e-6 && matBuf.getNumFrames() > 0)
        screen_.materialDurationSec = static_cast<float> (static_cast<double> (matBuf.getNumFrames()) / sampleRate_);
    else
        screen_.materialDurationSec = 0.0f;

    const int matFrames = matBuf.getNumFrames();
    if (selectedPage_ == Page::Material && matFrames > 1)
    {
        const float zoom   = params_.effective (selected, ParameterId::MaterialWaveZoom);
        const float center = materialWaveCenter01 (selected);
        computeMaterialWaveView (matFrames, zoom, center, screen_.materialViewStart01,
                                 screen_.materialViewEnd01);
    }
    else
    {
        screen_.materialViewStart01 = 0.0f;
        screen_.materialViewEnd01   = 1.0f;
    }

    const int granularEncPage = getGranularEncoderPage();
    screen_.granularEncoderPage = static_cast<uint8_t> (granularEncPage);

    int visible = 0;
    screen_.paramModOffset.fill (0.0f);
    for (int slot = 0; slot < kMaxParamsPerPage; ++slot)
    {
        const ParameterId id = PageModel::parameterForSlot (selectedPage_, slot, granularEncPage);
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

void Engine::fillMaterialWaveformEnvelope (int trackIndex, int numBins, float* outEnvelope,
                                           bool applyWaveZoom) const
{
    if (outEnvelope == nullptr || numBins <= 0)
        return;

    for (int b = 0; b < numBins; ++b)
        outEnvelope[b] = 0.0f;

    const int ti = (trackIndex < 0) ? 0 : (trackIndex >= kNumTracks ? kNumTracks - 1 : trackIndex);
    const SampleBuffer& buf = tracks_[static_cast<size_t> (ti)].getMaterial().getBuffer();
    const int           frames = buf.getNumFrames();
    const int           channels = buf.getNumChannels();
    if (frames < 1 || channels < 1)
        return;

    const int rightCh = channels > 1 ? 1 : 0;

    float view0 = 0.0f;
    float view1 = 1.0f;
    if (applyWaveZoom && frames > 1)
    {
        const float zoom   = params_.effective (ti, ParameterId::MaterialWaveZoom);
        const float center = materialWaveCenter01 (ti);
        computeMaterialWaveView (frames, zoom, center, view0, view1);
    }

    const double last = static_cast<double> (frames - 1);
    const double win0 = static_cast<double> (view0) * last;
    const double win1 = static_cast<double> (view1) * last;
    const double winLen = std::max (1.0e-6, win1 - win0);

    for (int i = 0; i < numBins; ++i)
    {
        const double t0 = win0 + (static_cast<double> (i) / static_cast<double> (numBins)) * winLen;
        const double t1 = win0 + (static_cast<double> (i + 1) / static_cast<double> (numBins)) * winLen;
        int          startF = static_cast<int> (std::floor (t0));
        int          endF   = static_cast<int> (std::ceil (t1));
        if (endF <= startF)
            endF = startF + 1;
        if (startF < 0)
            startF = 0;
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
