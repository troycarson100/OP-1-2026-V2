#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PageModel.h"
#include "ui/juce/InstrumentPanel.h"
#include "ui/juce/ModPagePanel.h"
#include "ui/juce/SampleBrowser.h"

// Temporary debug UI only. Reads the portable ScreenModel for display and
// attaches sliders to host parameters. No DSP, no engine logic, no core state.
class SculptSamplerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          public juce::FileDragAndDropTarget
{
public:
    explicit SculptSamplerAudioProcessorEditor (SculptSamplerAudioProcessor&);
    ~SculptSamplerAudioProcessorEditor() override;

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
    juce::TextButton   metroButton_ { "METRO" }; // metronome click toggle (by the BPM, in the header)

    // SELECT = main output level (always; same role as S-4 select on MIX).
    juce::Label       selectLabel_;
    juce::Slider      selectSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> selectAttachment_;

    // Tracks
    std::array<juce::TextButton, sculpt::kNumTracks> trackButtons_;
    // Mute mode: when armed (reddish), clicking a track toggles its mute instead of selecting it.
    juce::TextButton muteButton_ { "MUTE" };
    bool muteMode_ = false;

    // Step sequencer row: master transport, per-track machine toggle, step-page toggle,
    // and 16 step buttons (showing 1-16 or 17-32 of the selected track's lane).
    juce::TextButton seqPlayButton_ { "PLAY ALL" };
    juce::TextButton machineButton_ { "MACH" };
    juce::TextButton stepPageButton_ { "1-16" };
    std::array<juce::TextButton, sculpt::kStepsPerPage> stepButtons_;
    int stepPage_ = 0;   // 0 = steps 0..15, 1 = steps 16..31
    void toggleStepPage();
    void toggleSelectedMachine();

    // P-lock editing (latched, works with one mouse): arm P-LOCK, click a step to target it; the
    // page knobs then detach from the live params and edit THAT step's locks directly (turn = set
    // the lock, no effect on the live value). Un-arm to rebind the knobs to the live params.
    juce::TextButton plockButton_ { "P-LOCK" };
    bool   plockMode_ = false;
    int    plockStep_ = -1;   // targeted step for lock editing
    void   enterLockEdit();   // detach page knobs and load the targeted step's lock values

    // During playback the page knobs follow the selected track's effective (post-lock) values so
    // locks are visible jumping per step. True while that follow is active (resync to base on stop).
    bool   knobsFollowing_ = false;

    // Pages (device row + MIX)
    std::array<juce::TextButton, static_cast<size_t> (sculpt::Page::Count)> pageButtons_;
    // Granular only: switch between encoder bank 1 (core sound) and 2 (rhythm + pitch + pattern).
    std::array<juce::TextButton, 2> granularEncoderPageButtons_;
    sculpt::Page currentPage_ = sculpt::Page::Granular;

    // Page parameter controls (rebuilt on track/page change)
    struct PageControl
    {
        sculpt::ParameterId id = sculpt::ParameterId::Count; // which parameter this control edits
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::ToggleButton> tapeSnapToggle; // Material page + Tape Speed only
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tapeSnapAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        // Space page: Freeze is a bool host parameter (toggle, not a rotary).
        std::unique_ptr<juce::ToggleButton> spaceFreezeToggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spaceFreezeAttachment;
        // Material page: small "Follow" toggle in the Loop Start cell (LoopStartFollow bool).
        std::unique_ptr<juce::ToggleButton> followToggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    };
    std::vector<PageControl> pageControls_;
    int lastBuiltTrack_ = -1;

    // Space page: keep the Time rotary's text box in sync with the Time Mode knob.
    juce::Slider* spaceTimeSlider_ = nullptr;
    int           spaceTimeModeIdx_ = 3;

    // Material page: Root BPM knob edits the active sample's per-sample tempo (not a track param).
    juce::Slider* rootBpmSlider_ = nullptr;

    std::unique_ptr<ModPagePanel> modPagePanel_;
    juce::Viewport                  modPageViewport_;

    juce::TextButton loadSampleButton_ { "LOAD" };
    juce::TextButton bankButton_ { "BANK" };
    juce::TextButton saveButton_ { "SAVE" };
    juce::TextButton openButton_ { "OPEN" };
    juce::TextButton spaceClearButton_ { "CLR SPACE" };
    juce::Label      helpLabel_;

    std::unique_ptr<juce::FileChooser> sampleChooser_;

    // Whole-project save/load: writes (and reads back) everything the plugin state holds — params,
    // sequences, scenes, and the loaded samples — to a single .sculpt file the user picks.
    std::unique_ptr<juce::FileChooser> projectChooser_;
    juce::File                         lastProjectDir_;
    void saveProject();
    void openProject();

    // Custom multi-select sample browser (Elektron-style), shown in its own window.
    void openSampleBrowser();
    void closeSampleBrowser();
    juce::Component::SafePointer<juce::DialogWindow> browserWindow_;
    juce::File lastBrowseDir_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptSamplerAudioProcessorEditor)
};
