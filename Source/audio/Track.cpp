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
    samplerTrigPending_        = false;
    followLoopStartLast_       = -1.0f;
    followArmed_               = true;
    followStableSamples_       = 0;
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
    const int matFrames = material_.getActiveBuffer().getNumFrames();
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

void Track::replaceMaterialStereo (const float* left, const float* right, int numFrames)
{
    material_.loadStereoPCM (left, right, numFrames);
    recorder_.prepare (material_.getBuffer());
    engine_.reset();
    ignoreStoppedPlayheadSeek_ = false;
}

void Track::updateParameters (const ParameterState& state, int trackIndex, bool materialPlayheadScrub,
                              double hostBpm, const GranularBlockTiming& granularTiming,
                              double engineSampleRate, int numSamples)
{
    const int t = trackIndex;
    auto get = [&state, t] (ParameterId id) { return state.effective (t, id); };

    // Sampler machine: sequencer-triggered voice. Sample Mode chooses how each trig plays the loop
    // region: One-Shot (once), Loop Forward, Loop Reverse, or Loop Fwd/Back (ping-pong). Torso
    // machine is unaffected (continuous forward loop).
    const bool sampler    = get (ParameterId::MaterialMachine) > 0.5f;
    const int  sampleMode = sampler ? map::materialSampleModeIndex (get (ParameterId::MaterialSampleMode)) : 1;
    const bool oneShot     = sampler && sampleMode == 0;
    const bool loopReverse = sampler && sampleMode == 2;
    const bool loopBounce  = sampler && sampleMode == 3;

    setCaptureArmed (get (ParameterId::CaptureArm) > 0.5f);

    engine_.setMaterialLevel (get (ParameterId::MaterialLevel));

    auto& tape = engine_.getTape();
    const float tapeKnobRaw = get (ParameterId::TapeSpeed);
    // Pitch knob: semitone transpose (+/-24 st, center = 0 st), snapped to the Material scale
    // (Free = continuous, no quantize). Applies in both Tape and Warp modes.
    const FilterScale pitchScale = normalizedToFilterScale (get (ParameterId::MaterialPitchScale));
    const int         pitchKey   = normalizedToKeyIndex (get (ParameterId::MaterialPitchKey));
    const float       materialSemis = materialPitchSemitones (tapeKnobRaw, pitchScale, pitchKey);
    const float       pitchRatio    = std::pow (2.0f, materialSemis / 12.0f);

    float speedRatio = 0.0f;
    if (get (ParameterId::MaterialTimeMode) > 0.5f)
    {
        float rootBpm = activeSampleRootBpm_;   // per-sample native tempo (pushed by Engine)
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
    {
        const int   gridN   = map::materialGridDivisions (get (ParameterId::MaterialGridDivision));
        const float gridBpm = activeSampleRootBpm_;   // per-sample native tempo (pushed by Engine)
        const int   gFrames = material_.getActiveBuffer().getNumFrames();
        const float gDurSec = engineSampleRate > 1.0e-6 ? static_cast<float> (gFrames / engineSampleRate) : 0.0f;
        map::snapLoopPair01 (loopLo, loopHi, map::materialGridStep01 (gridN, gridBpm, gDurSec));
    }

    // Loop Start Follow: fire ONE crisp playhead jump per Loop Start change. Fire on the first
    // changed block (immediate, on-beat for a stepped random S&H), then lock out re-triggering until
    // the value has been stable for a short window — so a still-moving (slewed/continuous) value
    // can't machine-gun retriggers into jitter.
    const bool  loopFollow = get (ParameterId::LoopStartFollow) > 0.5f;
    const int   kFollowRearmSamples =
        std::max (1, static_cast<int> (engineSampleRate * 0.015)); // 15 ms stable to re-arm
    bool followReadyToSeek = false;
    if (loopFollow)
    {
        if (followLoopStartLast_ < 0.0f)
        {
            followLoopStartLast_ = loopLo;   // just enabled: arm without an initial jump
            followArmed_         = true;
            followStableSamples_ = 0;
        }
        else if (std::fabs (loopLo - followLoopStartLast_) > 1.0e-4f)
        {
            followLoopStartLast_ = loopLo;
            followStableSamples_ = 0;        // moving: hold off re-arming
            if (followArmed_)
            {
                followReadyToSeek = true;     // fire once at the start of this change burst
                followArmed_      = false;     // lock out until it settles again
            }
        }
        else
        {
            followStableSamples_ += std::max (0, numSamples);
            if (followStableSamples_ >= kFollowRearmSamples)
                followArmed_ = true;          // settled: ready for the next change
        }
    }
    else
    {
        followArmed_         = true;
        followLoopStartLast_ = -1.0f;        // sentinel so re-enabling doesn't jump
        followStableSamples_ = 0;
    }

    const float regionLo = loopLo, regionHi = loopHi;

    // Loop Reverse plays the loop region backward (negative speed); ping-pong starts forward and
    // alternates inside the TapePlayer. One-Shot stops at the loop end; everything else loops.
    tape.setSpeedRatio (loopReverse ? -speedRatio : speedRatio);
    tape.setLoopRegion (regionLo, regionHi);
    tape.setLevel (1.0f);
    tape.setLoopMode (! oneShot);     // Torso + Sampler loop modes loop; only One-Shot stops at end
    tape.setLoopBounce (loopBounce);  // ping-pong (reflect at boundaries)
    // With Loop Start Follow the playhead jumps do the chopping, so turn off the boundary grain-cloud
    // scrub (it would smear between the rate-limited jumps and sound jittery).
    tape.setBoundaryScrubEnabled (! loopFollow);
    const float xfadeSec = map::loopFadeSeconds (get (ParameterId::MaterialLoopXfade));
    tape.setLoopFades (xfadeSec, xfadeSec);

    if (materialPlayheadScrub)
        ignoreStoppedPlayheadSeek_ = false;

    const int matFrames = material_.getActiveBuffer().getNumFrames();
    const bool seekTapeToPlayhead = materialPlayheadScrub
                                    || (! playing_ && ! ignoreStoppedPlayheadSeek_);
    if (seekTapeToPlayhead)
        tape.seekNormalized (get (ParameterId::MaterialPlayhead), matFrames, loopLo, loopHi);

    // Follow knob while gesturing (tape keeps running); seek above updates scrubTarget first.
    tape.setFollowScrubTarget (materialPlayheadScrub);

    // Sampler-machine trig from the step sequencer: re-seek the read head and re-fire so every step
    // restarts cleanly. Reverse mode starts at the loop end (and plays backward); forward / ping-pong
    // start at the loop start. A p-locked position overrides the default start.
    if (samplerTrigPending_)
    {
        float pos = samplerTrigPos01_ < 0.0f ? (loopReverse ? loopHi : loopLo)
                                             : std::clamp (samplerTrigPos01_, 0.0f, 1.0f);
        // Crossfaded read-head jump (declick) rather than a hard seek; the gate just opens (it ramps
        // up from silence only when the track was stopped, so no level discontinuity either way).
        tape.retrigger (pos, matFrames, loopLo, loopHi);
        engine_.getGranular().setActive (true);
        gate_.gateOn();
        playing_                   = true;
        ignoreStoppedPlayheadSeek_ = true;
        samplerTrigPending_        = false;
        // The trig already placed the read head; don't let Follow also snap this resting position.
        followArmed_         = false;
        followLoopStartLast_ = loopLo;
        followStableSamples_ = 0;
    }

    // Loop Start Follow: crisp single splice to the new Loop Start (short equal-power declick — a
    // longer crossfade flams the old chunk against the new). One fire per change (armed/lockout above).
    if (followReadyToSeek && ! materialPlayheadScrub && playing_)
        tape.retrigger (loopLo, matFrames, loopLo, loopHi, 5.0f);

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
    // Share the Material page's scale + key so grains stay in tune with the tape layer.
    gp.pitchScale = pitchScale;
    gp.pitchKey   = pitchKey;
    // Grains ride the Material page transpose so the whole track pitches together.
    gp.materialSemis = materialSemis;
    // Playhead follow: grains can track the moving tape position (GrainFollow).
    gp.playhead01 = getTapePositionNormalized();
    gp.follow     = get (ParameterId::GrainFollow);
    // Granulate the loop region the tape is playing so grains track the current voice.
    gp.loopStart01        = regionLo;
    gp.loopEnd01          = regionHi;
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

    // Sampler machine: the granular cloud is part of the per-step voice, not a free-running drone.
    // Spawn grains only while the voice is sounding; when the tape stops (One-Shot reaching the loop
    // end), stop spawning (in-flight grains finish naturally, leaving a granular tail). GrainMix
    // blends it against the dry voice, so the cloud is fully usable — just gated to the hit.
    if (sampler)
        engine_.getGranular().setActive (tape.isPlaying());

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

    // Reverb/delay are global now (Engine::SpaceSendBus); this track only contributes a send level
    // (SpaceReverbAmount / SpaceDelayAmount), applied when the Engine builds the shared send buses.
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

    engine_.process (material_.getActiveBuffer(), outL, outR, numSamples);

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
    return engine_.getTape().getPositionNormalized (material_.getActiveBuffer().getNumFrames());
}

} // namespace sculpt
