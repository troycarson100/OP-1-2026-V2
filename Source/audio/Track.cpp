#include "Track.h"
#include "GranularEngine.h"
#include "../core/ParameterIds.h"
#include "../core/ParameterState.h"
#include "../core/FilterScales.h"
#include "../util/GrainPitchScales.h"

#include <algorithm>
#include <cmath>

namespace sculpt
{

void Track::prepare (double sampleRate, int trackIndex)
{
    // One buffer for placeholders, file import, and circular input capture.
    material_.prepare (sampleRate, kMaxCaptureSeconds);

    // Each track gets distinct placeholder material so the box makes a
    // four-layer texture out of the gate.
    if (! material_.hasUserMaterial())
    {
        using PT = MaterialSource::PlaceholderType;
        static constexpr PT placeholderForTrack[kNumTracks] = { PT::Tone, PT::Wave, PT::Noise, PT::Pulse };
        material_.generatePlaceholder (placeholderForTrack[trackIndex % kNumTracks]);
    }

    recorder_.prepare (material_.getBuffer());
    engine_.prepare (sampleRate);

    gate_.prepare (sampleRate);
    gate_.setTimesMs (8.0f, 40.0f);

    reset();
}

void Track::reset()
{
    engine_.reset();
    playing_                   = false;
    ignoreStoppedPlayheadSeek_ = false;
}

void Track::trigger()
{
    playing_                      = true;
    ignoreStoppedPlayheadSeek_    = false;
    engine_.getTape().start();
    engine_.getGranular().setActive (true);
    gate_.gateOn();
}

void Track::triggerWithWarpPlayhead (float materialPlayhead01, float loopStart01, float loopEnd01)
{
    playing_                   = true;
    ignoreStoppedPlayheadSeek_ = false;
    auto& tape = engine_.getTape();
    tape.stop();
    const int matFrames = material_.getBuffer().getNumFrames();
    tape.seekNormalized (materialPlayhead01, matFrames, loopStart01, loopEnd01);
    tape.start();
    engine_.getGranular().setActive (true);
    gate_.gateOn();
}

void Track::stop()
{
    playing_                   = false;
    ignoreStoppedPlayheadSeek_ = true;
    engine_.getGranular().setActive (false);
    gate_.gateOff();
    // The tape keeps its position; it simply fades out through the gate.
}

void Track::captureInput (const float* const* inputs, int numChannels, int numSamples)
{
    recorder_.process (inputs, numChannels, numSamples);
}

void Track::setCaptureArmed (bool armed)
{
    if (armed)
        recorder_.arm();
    else
        recorder_.disarm();
}

void Track::clearSpaceBuffers()
{
    engine_.getSpace().requestClear();
}

void Track::replaceMaterialStereo (const float* left, const float* right, int numFrames)
{
    material_.loadStereoPCM (left, right, numFrames);
    recorder_.prepare (material_.getBuffer());
    engine_.reset();
    ignoreStoppedPlayheadSeek_ = false;
}

void Track::updateParameters (const ParameterState& state, int trackIndex, bool materialPlayheadScrub,
                              double hostBpm, const GranularBlockTiming& granularTiming,
                              double engineSampleRate)
{
    const int t = trackIndex;
    auto get = [&state, t] (ParameterId id) { return state.effective (t, id); };

    setCaptureArmed (get (ParameterId::CaptureArm) > 0.5f);

    engine_.setMaterialLevel (get (ParameterId::MaterialLevel));

    auto& tape = engine_.getTape();
    const float tapeKnobRaw = get (ParameterId::TapeSpeed);
    // Pitch knob: semitone transpose (+/-24 st, center = 0 st), snapped to the Material scale
    // (Free = continuous, no quantize). Applies in both Tape and Warp modes.
    const FilterScale pitchScale = normalizedToFilterScale (get (ParameterId::MaterialPitchScale));
    const float       pitchRatio = materialPitchRatio (tapeKnobRaw, pitchScale);

    float speedRatio = 0.0f;
    if (get (ParameterId::MaterialTimeMode) > 0.5f)
    {
        float rootBpm = map::sampleRootBpm (get (ParameterId::SampleRootBpm));
        if (rootBpm < 1.0e-3f)
            rootBpm = 120.0f;

        double hb = hostBpm;
        if (! (hb > 1.0 && hb < 999.0))
            hb = static_cast<double> (rootBpm);

        float sync = static_cast<float> (hb / static_cast<double> (rootBpm));
        sync       = std::clamp (sync, 0.1f, 10.0f);
        // Warp: tempo-sync to host, then transpose by the Pitch knob (forward only).
        speedRatio = sync * pitchRatio;
    }
    else
    {
        speedRatio = pitchRatio;
    }
    float loopLo = get (ParameterId::LoopStart);
    float loopHi = get (ParameterId::LoopEnd);
    if (get (ParameterId::LoopSnapGrid) > 0.5f)
        map::snapLoopPair01 (loopLo, loopHi);

    tape.setSpeedRatio (speedRatio);
    tape.setLoopRegion (loopLo, loopHi);
    tape.setLevel (1.0f);

    if (materialPlayheadScrub)
        ignoreStoppedPlayheadSeek_ = false;

    const int matFrames = material_.getBuffer().getNumFrames();
    const bool seekTapeToPlayhead = materialPlayheadScrub
                                    || (! playing_ && ! ignoreStoppedPlayheadSeek_);
    if (seekTapeToPlayhead)
        tape.seekNormalized (get (ParameterId::MaterialPlayhead), matFrames, loopLo, loopHi);

    // Follow knob while gesturing (tape keeps running); seek above updates scrubTarget first.
    tape.setFollowScrubTarget (materialPlayheadScrub);

    GranularEngine::Params gp;
    gp.position = get (ParameterId::GrainPosition);
    gp.size     = get (ParameterId::GrainSize);
    gp.density  = get (ParameterId::GrainDensity);
    gp.pitch    = get (ParameterId::GrainPitch);
    gp.spray    = get (ParameterId::GrainSpray);
    gp.texture  = get (ParameterId::GrainTexture);
    gp.spread   = get (ParameterId::GrainSpread);
    gp.mix      = get (ParameterId::GrainMix);
    gp.syncedMode = get (ParameterId::GrainSync) > 0.5f;
    gp.steps      = map::grainSteps (get (ParameterId::GrainSteps));
    gp.pulses     = map::grainPulses (get (ParameterId::GrainPulses), gp.steps);
    gp.rotate     = map::grainRotate (get (ParameterId::GrainRotate), gp.steps);
    gp.pitchQuantIndex = grainPitchQuantScaleIndex (get (ParameterId::GrainPitchQuant));
    gp.loopStart01        = loopLo;
    gp.loopEnd01          = loopHi;
    gp.grainPattern       = get (ParameterId::GrainPattern);
    gp.grainPatternAmount = get (ParameterId::GrainPatternAmount);
    gp.contour            = get (ParameterId::GrainContour);
    gp.randRev            = get (ParameterId::GrainRandRev);
    gp.randAmp            = get (ParameterId::GrainRandAmp);
    engine_.getGranular().setParams (gp);
    engine_.setGranularBlockTiming (granularTiming);
    if (materialPlayheadScrub)
        // Strong duck of granular layer while scanning; tape path does the scrubbing.
        engine_.snapGrainMix (gp.mix * 0.05f);
    else
        engine_.setGrainMix (gp.mix);

    {
        const bool spectral = get (ParameterId::FilterMode) > 0.5f;
        engine_.setSpectralMode (spectral);

        const float cutoff = get (ParameterId::FilterCutoff);
        const float res    = get (ParameterId::FilterResonance);
        const float mix    = get (ParameterId::FilterMix);
        const float decay  = get (ParameterId::FilterDecay);
        const float pitch  = get (ParameterId::FilterPitch);
        const float scale  = snapNormalizedFilterScale (get (ParameterId::FilterScale));
        const float key    = snapNormalizedFilterKey (get (ParameterId::FilterKey));

        if (spectral)
        {
            engine_.getSpectralFilter().setParams (cutoff, res, decay, pitch, scale, key, mix);
            engine_.getFilter().setParams (cutoff, res, 0.0f);    // silenced
        }
        else
        {
            // FilterDecay is repurposed as SVF type (LP/BP/HP) in LPF mode.
            engine_.getFilter().setParams (cutoff, res, mix, decay);
            engine_.getSpectralFilter().setParams (cutoff, res, decay, pitch, scale, key, 0.0f); // silenced
        }
    }

    engine_.getColor().setParams (get (ParameterId::ColorDrive),
                                  get (ParameterId::ColorCrush),
                                  get (ParameterId::ColorTilt),
                                  get (ParameterId::ColorCompress),
                                  get (ParameterId::ColorNoise),
                                  get (ParameterId::ColorNoiseDecay),
                                  get (ParameterId::ColorNoiseTone),
                                  get (ParameterId::ColorWet));

    engine_.getSpace().setParams (get (ParameterId::SpaceDelayAmount),
                                  get (ParameterId::SpaceDelayTime),
                                  get (ParameterId::SpaceReverbAmount),
                                  get (ParameterId::SpaceReverbSize),
                                  get (ParameterId::SpaceDelayFeedback),
                                  get (ParameterId::SpaceSpread),
                                  get (ParameterId::SpaceDamp),
                                  get (ParameterId::SpaceReverbDecay),
                                  get (ParameterId::SpaceDelayTimeMode),
                                  get (ParameterId::SpaceFreeze) > 0.5f,
                                  engineSampleRate,
                                  granularTiming);

    engine_.getMixBus().setParams (get (ParameterId::MixEqLowGain),
                                  get (ParameterId::MixEqMidGain),
                                  get (ParameterId::MixEqHighGain),
                                  get (ParameterId::MixCompThreshold),
                                  get (ParameterId::MixCompMakeup));
}

void Track::process (float* outL, float* outR, int numSamples)
{
    if (! gate_.isActive())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }
        return;
    }

    engine_.process (material_.getBuffer(), outL, outR, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gate_.next();
        outL[i] *= g;
        outR[i] *= g;
    }
}

float Track::getGrainActivity() const
{
    return static_cast<float> (engine_.getGranular().getActiveGrains())
         / static_cast<float> (kGrainsPerTrack);
}

float Track::getTapePositionNormalized() const
{
    return engine_.getTape().getPositionNormalized (material_.getBuffer().getNumFrames());
}

} // namespace sculpt
