#include "TrackEngine.h"
#include "SampleBuffer.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void TrackEngine::prepare (double sampleRate)
{
    tape_.prepare (sampleRate);
    granular_.prepare (sampleRate);
    filter_.prepare (sampleRate);
    spectralFilter_.prepare (sampleRate);
    color_.prepare (sampleRate);
    space_.prepare (sampleRate);
    mixBus_.prepare (sampleRate);

    materialLevel_.prepare (sampleRate, 0.02f);
    materialLevel_.snap (parameterDefault (ParameterId::MaterialLevel));
    grainMix_.prepare (sampleRate, 0.02f);
    grainMix_.snap (parameterDefault (ParameterId::GrainMix));

    reset();
}

void TrackEngine::reset()
{
    tape_.reset();
    granular_.reset();
    filter_.reset();
    spectralFilter_.reset();
    color_.reset();
    space_.reset();
    mixBus_.reset();
}

void TrackEngine::process (const SampleBuffer& material, float* outL, float* outR, int numSamples)
{
    const int n = numSamples < kMaxBlockSize ? numSamples : kMaxBlockSize;

    for (int i = 0; i < n; ++i)
    {
        dryL_[static_cast<size_t> (i)] = 0.0f;
        dryR_[static_cast<size_t> (i)] = 0.0f;
        grainL_[static_cast<size_t> (i)] = 0.0f;
        grainR_[static_cast<size_t> (i)] = 0.0f;
    }

    // Material stage: tape/loop playback of the source buffer.
    tape_.process (material, dryL_.data(), dryR_.data(), n);

    // Granular stage: wet grain cloud from the same material.
    granular_.process (material, grainL_.data(), grainR_.data(), n);

    // Blend dry material against the grain cloud, scaled by material level.
    for (int i = 0; i < n; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        const float level = materialLevel_.next();
        const float mix   = grainMix_.next();
        outL[i] = lerp (dryL_[idx], grainL_[idx], mix) * level;
        outR[i] = lerp (dryR_[idx], grainR_[idx], mix) * level;
    }

    // Filter stage: SVF (LPF/BP/HP) or spectral resonator bank.
    if (spectralMode_)
        spectralFilter_.process (outL, outR, n);
    else
        filter_.process (outL, outR, n);

    color_.process (outL, outR, n);
    space_.process (outL, outR, n);
    mixBus_.process (outL, outR, n);
}

} // namespace sculpt
