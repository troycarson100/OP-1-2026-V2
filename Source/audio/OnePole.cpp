#include <cmath>
#include "OnePole.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void OnePole::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void OnePole::setCutoffHz (float hz)
{
    const float limited = clampf (hz, 5.0f, static_cast<float> (sampleRate_) * 0.45f);
    coeff_ = 1.0f - std::exp (-kTwoPi * limited / static_cast<float> (sampleRate_));
}

} // namespace sculpt
