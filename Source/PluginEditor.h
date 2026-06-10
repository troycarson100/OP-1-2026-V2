#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PageModel.h"
#include "ui/juce/InstrumentPanel.h"

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
    sculpt::Page currentPage_ = sculpt::Page::Granular;

    // Page parameter controls (rebuilt on track/page change)
    struct PageControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<PageControl> pageControls_;
    int lastBuiltTrack_ = -1;

    // Macros
    std::array<juce::Slider, sculpt::kNumMacros> macroSliders_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, sculpt::kNumMacros> macroAttachments_;

    juce::TextButton modButton_ { "MOD" };
    juce::TextButton loadSampleButton_ { "LOAD" };
    juce::Label      helpLabel_;

    std::unique_ptr<juce::FileChooser> sampleChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptSamplerAudioProcessorEditor)
};
