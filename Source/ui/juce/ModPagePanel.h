#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../modulation/ModTypes.h"
#include "../../util/Constants.h"

class SculptSamplerAudioProcessor;

// S-4-style Mod page: per-track slot, modulator kind + params, mapping grid
// (depth + uni/bi toggle per encoder for a chosen device page). JUCE only.
class ModPagePanel : public juce::Component
{
public:
    explicit ModPagePanel (SculptSamplerAudioProcessor& processor);

    void refreshFromEngine();
    void commitToEngine();

    void resized() override;

    // Minimum height for viewed component inside a Viewport (mapping + source column).
    static int recommendedScrollableHeight();

private:
    void kindChanged();
    void wireSlider (juce::Slider& s, float lo, float hi);
    void fillDivisionCombo (juce::ComboBox& box);
    void syncWaveControlsEnabled();
    void syncRandomControlsEnabled();
    void configureMappingDepthSlider (int row);
    void updatePolarButtonLabel (int row);

    SculptSamplerAudioProcessor& processor_;

    juce::Label       title_ { {}, "Modulation" };
    juce::Label       slotLabel_ { {}, "Slot" };
    juce::ComboBox    slotCombo_;
    juce::Label       kindLabel_ { {}, "Source" };
    juce::ComboBox    kindCombo_;
    juce::TextButton  trigButton_ { "ADSR Trig" };

    juce::Label       waveSection_ { {}, "Wave" };
    juce::ToggleButton waveSyncT_ { "Sync" };
    juce::ComboBox    waveDivCombo_;
    juce::Label       waveRateL_ { {}, "Rate (Hz)" };
    juce::Slider      waveRate_;
    juce::Label       waveShapeL_ { {}, "Shape" };
    juce::ComboBox    waveShapeCombo_;
    juce::Label       waveAmtL_ { {}, "Amount %" };
    juce::Slider      waveAmount_;
    juce::Label       waveBendL_ { {}, "Bend" };
    juce::Slider      waveBend_;

    juce::Label       rndSection_ { {}, "Random" };
    juce::ToggleButton rndSyncT_ { "Sync" };
    juce::ComboBox    rndDivCombo_;
    juce::Label       rndRateL_ { {}, "Rate (Hz)" };
    juce::Slider      rndRate_;
    juce::Label       rndSlewL_ { {}, "Slew" };
    juce::Slider      rndSlew_;

    juce::Label       adsrSection_ { {}, "ADSR" };
    juce::Label       adA_ { {}, "A" };
    juce::Slider      adsrA_;
    juce::Label       adD_ { {}, "D" };
    juce::Slider      adsrD_;
    juce::Label       adS_ { {}, "S" };
    juce::Slider      adsrS_;
    juce::Label       adR_ { {}, "R" };
    juce::Slider      adsrR_;

    juce::Label       mapSection_ { {}, "Mapping" };
    juce::Label       mapPageLabel_ { {}, "Target page" };
    juce::ComboBox    mapPageCombo_;
    juce::Label       mapHeaderParam_ { {}, "Param" };
    juce::Label       mapHeaderPolar_ { {}, "Mode" };
    juce::Label       mapHeaderDepth_ { {}, "Depth" };

    std::array<juce::Label,        sculpt::kMaxModMappingEncoders> mapName_;
    std::array<juce::ToggleButton, sculpt::kMaxModMappingEncoders> mapPolarT_;
    std::array<juce::Slider,       sculpt::kMaxModMappingEncoders> mapDepth_;

    bool suppressCallbacks_ = false;
};
