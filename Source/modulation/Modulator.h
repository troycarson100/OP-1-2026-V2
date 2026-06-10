#pragma once

namespace sculpt
{

// Base class for block-rate modulation sources.
// Sources update once per processed chunk and expose a bipolar value
// in [-1, 1] (unipolar sources simply stay in [0, 1]).
// Virtual dispatch happens only at block rate, never per sample.
class Modulator
{
public:
    virtual ~Modulator() = default;

    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;

    // Advance by numSamples and cache the new value.
    virtual void update (int numSamples) = 0;

    virtual float getValue() const = 0;
};

} // namespace sculpt
