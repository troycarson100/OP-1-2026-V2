#pragma once

#include <array>
#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../ScreenModel.h"
#include "../../util/Constants.h"

// JUCE-only faux LCD: meters, Material waveform, and 2x4 parameter readouts.
class InstrumentPanel : public juce::Component
{
public:
    using ScreenProvider = std::function<const sculpt::ScreenModel&()>;

    InstrumentPanel() = default;

    void setScreenProvider (ScreenProvider fn) { screenProvider_ = std::move (fn); }

    void setWaveformEnvelope (const float* data, int numBins);
    void clearWaveformEnvelope();

    void paint (juce::Graphics& g) override;

private:
    ScreenProvider screenProvider_;
    std::array<float, sculpt::kMaterialWaveformBins> waveformPeaks_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentPanel)
};
