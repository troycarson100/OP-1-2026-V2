#include <cmath>
#include "ColorStage.h"
#include "../core/ParameterIds.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void ColorStage::prepare (double sampleRate)
{
    toneL_.prepare (sampleRate);
    toneR_.prepare (sampleRate);
    drive_.prepare (sampleRate, 0.02f);
    tone_.prepare (sampleRate, 0.02f);
    mix_.prepare (sampleRate, 0.02f);
    drive_.snap (0.15f);
    tone_.snap (0.5f);
    mix_.snap (0.3f);
    reset();
}

void ColorStage::reset()
{
    toneL_.reset();
    toneR_.reset();
}

void ColorStage::setParams (float drive01, float tone01, float mix01)
{
    drive_.setTarget (drive01);
    tone_.setTarget (tone01);
    mix_.setTarget (mix01);
}

float ColorStage::processSample (OnePole& toneFilter, float in, float driveGain, float tone, float mix)
{
    // Drive normalized so unity-level material doesn't just get louder.
    const float shaped = std::tanh (in * driveGain) / std::tanh (driveGain);

    // Tone tilt around the one-pole low-pass.
    const float lp = toneFilter.processLowpass (shaped);
    const float toned = tone < 0.5f
                          ? lerp (lp, shaped, tone * 2.0f)              // darken
                          : shaped + (tone * 2.0f - 1.0f) * (shaped - lp) * 0.8f; // brighten

    return lerp (in, toned, mix);
}

void ColorStage::process (float* left, float* right, int numSamples)
{
    const float driveGain = map::colorDriveGain (drive_.skip (numSamples));
    const float tone      = tone_.skip (numSamples);
    toneL_.setCutoffHz (map::toneCutoffHz (0.5f));
    toneR_.setCutoffHz (map::toneCutoffHz (0.5f));

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mix_.next();
        left[i]  = processSample (toneL_, left[i],  driveGain, tone, mix);
        right[i] = processSample (toneR_, right[i], driveGain, tone, mix);
    }
}

} // namespace sculpt
