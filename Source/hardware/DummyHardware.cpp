#include "DummyHardware.h"
#include "../ui/ScreenModel.h"

namespace sculpt
{

float DummyHardware::readEncoderDelta (int encoderIndex)
{
    (void) encoderIndex;
    return 0.0f;
}

bool DummyHardware::isButtonDown (int buttonIndex)
{
    (void) buttonIndex;
    return false;
}

bool DummyHardware::wasButtonPressed (int buttonIndex)
{
    (void) buttonIndex;
    return false;
}

void DummyHardware::setLed (int ledIndex, float brightness)
{
    if (ledIndex >= 0 && ledIndex < hw::kNumLeds)
        leds_[static_cast<size_t> (ledIndex)] = brightness;
}

void DummyHardware::updateScreen (const ScreenModel& screen)
{
    (void) screen;   // The JUCE editor reads the ScreenModel directly instead.
}

float DummyHardware::getLed (int ledIndex) const
{
    return (ledIndex >= 0 && ledIndex < hw::kNumLeds) ? leds_[static_cast<size_t> (ledIndex)] : 0.0f;
}

} // namespace sculpt
