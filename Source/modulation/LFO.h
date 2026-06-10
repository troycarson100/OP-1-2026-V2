#pragma once

#include "Modulator.h"

namespace sculpt
{

// Block-rate LFO. Sine or triangle. Bipolar output [-1, 1].
class LFO : public Modulator
{
public:
    enum class Shape { Sine, Triangle };

    void prepare (double sampleRate) override;
    void reset() override;
    void update (int numSamples) override;
    float getValue() const override { return value_; }

    void setRateHz (float hz)   { rateHz_ = hz > 0.0f ? hz : 0.01f; }
    void setShape (Shape s)     { shape_ = s; }
    void setPhase (float phase01);

private:
    double sampleRate_ = 44100.0;
    double phase_      = 0.0;     // 0..1
    float  rateHz_     = 0.5f;
    float  value_      = 0.0f;
    Shape  shape_      = Shape::Sine;
};

} // namespace sculpt
