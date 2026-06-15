#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PageModel.h"
#include "ui/juce/InstrumentPanel.h"
#include "ui/juce/ModPagePanel.h"

// Temporary debug UI only. Reads the portable ScreenModel for display and
// attaches sliders to host parameters. No DSP, no engine logic, no core state.
class SculptSamplerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          public juce::FileDragAndDropTarget
{
public:
    explicit SculptSamplerAudioProcessorEditor (SculptSamplerAudioProcessor&);
    ~SculptSamplerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;

    void selectTrack (int trackIndex);
    void selectPage (sculpt::Page page);
    void rebuildPageControls();

    int getSelectedTrackFromParameter() const;

    SculptSamplerAudioProcessor& processor_;

    InstrumentPanel instrumentPanel_;

    // Header / scenes
    juce::Label titleLabel_;
    std::array<juce::TextButton, 4> sceneButtons_;
    juce::ToggleButton sceneSaveMode_ { "SAVE" };

    // SELECT = main output level (always; same role as S-4 select on MIX).
    juce::Label       selectLabel_;
    juce::Slider      selectSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> selectAttachment_;

    // Tracks
    std::array<juce::TextButton, sculpt::kNumTracks> trackButtons_;
    std::array<juce::TextButton, sculpt::kNumTracks> playButtons_;

    // Pages (device row + MIX)
    std::array<juce::TextButton, static_cast<size_t> (sculpt::Page::Count)> pageButtons_;
    // Granular only: switch between core grain encoders (1) and sync / Euclidean / pitch-quant (2).
    std::array<juce::TextButton, 2> granularEncoderPageButtons_;
    sculpt::Page currentPage_ = sculpt::Page::Granular;

    // Page parameter controls (rebuilt on track/page change)
    struct PageControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::ToggleButton> tapeSnapToggle; // Material page + Tape Speed only
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tapeSnapAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        // Space page: Freeze is a bool host parameter (toggle, not a rotary).
        std::unique_ptr<juce::ToggleButton> spaceFreezeToggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spaceFreezeAttachment;
        // Material page: snap loop in/out to grid (bool).
        std::unique_ptr<juce::ToggleButton> loopSnapToggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopSnapAttachment;
    };
    std::vector<PageControl> pageControls_;
    int lastBuiltTrack_ = -1;

    // Space page: keep the Time rotary's text box in sync with the Time Mode knob.
    juce::Slider* spaceTimeSlider_ = nullptr;
    int           spaceTimeModeIdx_ = 3;

    // Macros
    std::array<juce::Slider, sculpt::kNumMacros> macroSliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, sculpt::kNumMacros> macroAttachments_;

    std::unique_ptr<ModPagePanel> modPagePanel_;
    juce::Viewport                  modPageViewport_;

    juce::TextButton loadSampleButton_ { "LOAD" };
    juce::TextButton spaceClearButton_ { "CLR SPACE" };
    juce::Label      helpLabel_;

    std::unique_ptr<juce::FileChooser> sampleChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptSamplerAudioProcessorEditor)
};
