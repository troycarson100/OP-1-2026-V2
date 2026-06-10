#pragma once

namespace sculpt
{

struct ScreenModel;

// Physical control surface layout (Torso S-4-style reference).
namespace hw
{
    constexpr int kNumEncoders = 9;    // 8 page encoders + 1 nav/master
    constexpr int kNumButtons  = 21;
    constexpr int kNumLeds     = 21;

    // Button map. Indices into the button/LED arrays.
    enum Button : int
    {
        Track1 = 0, Track2, Track3, Track4,           // track select
        Play, Stop, Record,                           // transport
        SceneA, SceneB, SceneC, SceneD,               // scene recall
        SceneSave,                                    // hold + scene = save
        PageMaterial, PageGranular, PageFilter,
        PageColor, PageSpace, PageMixer,              // page select
        Shift, MacroMode, CaptureArm                  // modifiers
    };
}

// The boundary between the portable instrument and any physical (or fake)
// control surface. The desktop prototype uses DummyHardware; a real device
// implements this against its encoders, buttons, LEDs and display.
// Implementations must be real-time safe if polled from the audio thread.
class HardwareAbstraction
{
public:
    virtual ~HardwareAbstraction() = default;

    // Accumulated encoder movement since the last call (detented steps,
    // fractional for high-resolution encoders). Returns 0 when idle.
    virtual float readEncoderDelta (int encoderIndex) = 0;

    virtual bool isButtonDown (int buttonIndex) = 0;

    // True exactly once per physical press (edge detect inside the impl).
    virtual bool wasButtonPressed (int buttonIndex) = 0;

    // LED brightness 0..1.
    virtual void setLed (int ledIndex, float brightness) = 0;

    // Push the abstract display state to the physical screen.
    virtual void updateScreen (const ScreenModel& screen) = 0;
};

} // namespace sculpt
