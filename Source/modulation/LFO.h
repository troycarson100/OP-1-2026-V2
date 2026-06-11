#pragma once

#include "Modulator.h"

namespace sculpt
{

// Block-rate LFO. Bipolar output [-1, 1]. Free mode advances phase each update;
// sync mode uses external beat phase via valueForShape().
class LFO : public Modulator
{
public:
    enum class Shape : uint8_t
    {
        Sine = 0,
        Triangle,
        SawUp,
        Square,
        RampDown,
        Count
    };

    static float valueForShape (Shape s, double phase01);

    void prepare (double sampleRate) override;
    void reset() override;
    void update (int numSamples) override;
    float getValue() const override { return value_; }

    void setRateHz (float hz)   { rateHz_ = hz > 0.0f ? hz : 0.01f; }
    void setShape (Shape s)     { shape_ = (s < Shape::Count ? s : Shape::Sine); }
    void setPhase (float phase01);

private:
    double sampleRate_ = 44100.0;
    double phase_      = 0.0;     // 0..1
    float  rateHz_     = 0.5f;
    float  value_      = 0.0f;
    Shape  shape_      = Shape::Sine;
};

} // namespace sculpt
