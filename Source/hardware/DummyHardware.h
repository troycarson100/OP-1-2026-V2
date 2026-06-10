#pragma once

#include <array>
#include "HardwareAbstraction.h"

namespace sculpt
{

// No-op control surface for the desktop plugin prototype.
// Stores LED state so a debug UI could visualize it; reports no input.
class DummyHardware : public HardwareAbstraction
{
public:
    float readEncoderDelta (int encoderIndex) override;
    bool  isButtonDown (int buttonIndex) override;
    bool  wasButtonPressed (int buttonIndex) override;
    void  setLed (int ledIndex, float brightness) override;
    void  updateScreen (const ScreenModel& screen) override;

    float getLed (int ledIndex) const;

private:
    std::array<float, hw::kNumLeds> leds_ {};
};

} // namespace sculpt
