#include "Track.h"
#include "../core/ParameterIds.h"
#include "../core/ParameterState.h"
#include "../core/FilterScales.h"
#include "../util/Constants.h"

namespace sculpt
{

void Track::prepare (double sampleRate, int trackIndex)
{
    // One buffer for placeholders, file import, and circular input capture.
    material_.prepare (sampleRate, kMaxCaptureSeconds);

    // Each track gets distinct placeholder material so the box makes a
    // four-layer texture out of the gate.
    using PT = MaterialSource::PlaceholderType;
    static constexpr PT placeholderForTrack[kNumTracks] = { PT::Tone, PT::Wave, PT::Noise, PT::Pulse };
    material_.generatePlaceholder (placeholderForTrack[trackIndex % kNumTracks]);

    recorder_.prepare (material_.getBuffer());
    engine_.prepare (sampleRate);

    gate_.prepare (sampleRate);
    gate_.setTimesMs (8.0f, 40.0f);

    reset();
}

void Track::reset()
{
    engine_.reset();
    playing_ = false;
}

void Track::trigger()
{
    playing_ = true;
    engine_.getTape().start();
    engine_.getGranular().setActive (true);
    gate_.gateOn();
}

void Track::stop()
{
    playing_ = false;
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
}

void Track::updateParameters (const ParameterState& state, int trackIndex)
{
    const int t = trackIndex;
    auto get = [&state, t] (ParameterId id) { return state.effective (t, id); };

    setCaptureArmed (get (ParameterId::CaptureArm) > 0.5f);

    engine_.setMaterialLevel (get (ParameterId::MaterialLevel));

    auto& tape = engine_.getTape();
    tape.setSpeedRatio (map::tapeSpeedRatio (get (ParameterId::TapeSpeed)));
    tape.setLoopRegion (get (ParameterId::LoopStart), get (ParameterId::LoopEnd));
    tape.setLevel (1.0f);

    GranularEngine::Params gp;
    gp.position = get (ParameterId::GrainPosition);
    gp.size     = get (ParameterId::GrainSize);
    gp.density  = get (ParameterId::GrainDensity);
    gp.pitch    = get (ParameterId::GrainPitch);
    gp.spray    = get (ParameterId::GrainSpray);
    gp.texture  = get (ParameterId::GrainTexture);
    gp.spread   = get (ParameterId::GrainSpread);
    gp.mix      = get (ParameterId::GrainMix);
    engine_.getGranular().setParams (gp);
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
                                  get (ParameterId::ColorTone),
                                  get (ParameterId::ColorMix));

    engine_.getSpace().setParams (get (ParameterId::SpaceAmount),
                                  get (ParameterId::SpaceFeedback),
                                  get (ParameterId::SpaceMix));
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
