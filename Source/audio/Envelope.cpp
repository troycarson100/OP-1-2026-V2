#include "Envelope.h"

namespace sculpt
{

void Envelope::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    setTimesMs (5.0f, 30.0f);
    level_ = 0.0f;
    gate_  = false;
}

void Envelope::setTimesMs (float attackMs, float releaseMs)
{
    const float sr = static_cast<float> (sampleRate_);
    attackInc_  = 1.0f / (sr * (attackMs  > 0.1f ? attackMs  : 0.1f) * 0.001f);
    releaseInc_ = 1.0f / (sr * (releaseMs > 0.1f ? releaseMs : 0.1f) * 0.001f);
}

} // namespace sculpt
