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
    using BpmDragHandler = std::function<void (double bpm)>;

    InstrumentPanel() = default;

    void setScreenProvider (ScreenProvider fn) { screenProvider_ = std::move (fn); }
    void setBpmDragHandler (BpmDragHandler fn) { bpmDragHandler_ = std::move (fn); }

    // Which device page the editor is showing (message thread). LCD layout follows this so the
    // MIX view is not stuck on the meter-only branch while waiting for the next audio callback.
    void setUiPage (sculpt::Page page) { uiPage_ = page; }

    void setWaveformEnvelope (const float* data, int numBins);
    void clearWaveformEnvelope();

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<int> bpmInteractionBounds() const;

    ScreenProvider   screenProvider_;
    BpmDragHandler   bpmDragHandler_;
    sculpt::Page     uiPage_ = sculpt::Page::Granular;
    std::array<float, sculpt::kMaterialWaveformBins> waveformPeaks_{};

    bool   bpmDragging_   = false;
    float  bpmDragStartX_ = 0.0f;
    double bpmAtDragStart_ = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentPanel)
};
