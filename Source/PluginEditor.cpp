#include "PluginEditor.h"
#include "ui/juce/EditorColours.h"
#include "ui/SpaceTimeFormat.h"
#include "util/Constants.h"

#include <array>

SculptSamplerAudioProcessorEditor::SculptSamplerAudioProcessorEditor (SculptSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    using namespace sculpt_editor;

    titleLabel_.setText ("SculptSampler", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, kText);
    titleLabel_.setFont (juce::FontOptions (15.0f));
    addAndMakeVisible (titleLabel_);

    for (int s = 0; s < 4; ++s)
    {
        auto& b = sceneButtons_[static_cast<size_t> (s)];
        b.setButtonText (juce::String::charToString (juce::juce_wchar ('A' + s)));
        b.onClick = [this, s]
        {
            auto& engine = processor_.getEngine();
            if (sceneSaveMode_.getToggleState())
            {
                engine.saveCurrentScene (s);
                sceneSaveMode_.setToggleState (false, juce::dontSendNotification);
            }
            else
            {
                engine.recallScene (s);
            }
        };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (sceneSaveMode_);
    sceneSaveMode_.setColour (juce::ToggleButton::textColourId, kText);

    selectLabel_.setText ("SELECT", juce::dontSendNotification);
    selectLabel_.setJustificationType (juce::Justification::centred);
    selectLabel_.setColour (juce::Label::textColourId, kText);
    selectLabel_.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (selectLabel_);

    selectSlider_.setSliderStyle (juce::Slider::LinearVertical);
    selectSlider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    selectSlider_.setColour (juce::Slider::trackColourId, kMeter);
    selectSlider_.setColour (juce::Slider::thumbColourId, kAccent);
    selectSlider_.setTooltip ("Main output level (host parameter: output gain).");
    addAndMakeVisible (selectSlider_);
    selectAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor_.getValueTreeState(), "outputGain", selectSlider_);

    for (int t = 0; t < sculpt::kNumTracks; ++t)
    {
        auto& select = trackButtons_[static_cast<size_t> (t)];
        select.setButtonText ("TRK " + juce::String (t + 1));
        select.setClickingTogglesState (false);
        select.setColour (juce::TextButton::buttonOnColourId, kAccent);
        select.onClick = [this, t]
        {
            // In mute mode the track buttons toggle mute; otherwise they select the track.
            if (muteMode_)
                processor_.getEngine().toggleTrackMuted (t);
            else
                selectTrack (t);
        };
        addAndMakeVisible (select);
    }

    // MUTE arm button (reddish). Arm it, then click tracks to toggle their mute.
    muteButton_.setClickingTogglesState (true);
    muteButton_.setColour (juce::TextButton::buttonOnColourId, kMute);
    muteButton_.setTooltip ("Arm mute, then click tracks to mute/unmute them (muted tracks show red).");
    muteButton_.onClick = [this] { muteMode_ = muteButton_.getToggleState(); };
    addAndMakeVisible (muteButton_);

    // --- Step sequencer row ---
    seqPlayButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    seqPlayButton_.setTooltip ("Master transport: starts the sequencer and launches all Torso tracks at once; "
                               "stop halts the sequencer and all tracks. Per-track PLAY still works individually.");
    seqPlayButton_.onClick = [this]
    {
        auto& engine = processor_.getEngine();
        if (engine.isSequencerPlaying())
            engine.masterStop();
        else
            engine.masterPlay();
    };
    addAndMakeVisible (seqPlayButton_);

    machineButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    machineButton_.setTooltip ("Selected track machine: TORSO (free-run tape/granular) or SAMP (sequencer-driven sampler).");
    machineButton_.onClick = [this] { toggleSelectedMachine(); };
    addAndMakeVisible (machineButton_);

    stepPageButton_.setTooltip ("Toggle the step buttons between steps 1-16 and 17-32.");
    stepPageButton_.onClick = [this] { toggleStepPage(); };
    addAndMakeVisible (stepPageButton_);

    for (int i = 0; i < sculpt::kStepsPerPage; ++i)
    {
        auto& b = stepButtons_[static_cast<size_t> (i)];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonOnColourId, kAccent);
        b.onClick = [this, i]
        {
            const int step  = stepPage_ * sculpt::kStepsPerPage + i;
            const int track = getSelectedTrackFromParameter();
            processor_.getEngine().toggleStepTrig (track, step);
        };
        addAndMakeVisible (b);
    }

    for (int pg = 0; pg < static_cast<int> (sculpt::Page::Count); ++pg)
    {
        auto& b = pageButtons_[static_cast<size_t> (pg)];
        const auto page = static_cast<sculpt::Page> (pg);
        if (page == sculpt::Page::Mixer)
            b.setButtonText ("MIX");
        else if (page == sculpt::Page::Mod)
            b.setButtonText ("MOD");
        else
            b.setButtonText (juce::String (sculpt::PageModel::pageName (page)).toUpperCase());
        b.setColour (juce::TextButton::buttonOnColourId, kAccent);
        b.onClick = [this, pg] { selectPage (static_cast<sculpt::Page> (pg)); };
        addAndMakeVisible (b);
    }

    for (int g = 0; g < 2; ++g)
    {
        auto& b = granularEncoderPageButtons_[static_cast<size_t> (g)];
        b.setButtonText (juce::String (g + 1));
        b.setTooltip (g == 0 ? "Granular page 1: position, size, density, pitch, spray, contour, spread, mix."
                             : "Granular page 2: sync, Euclidean steps/pulses/rotate, pitch quantize, "
                               "grain pattern + pattern amount, rand rev.");
        b.setColour (juce::TextButton::buttonOnColourId, kAccent);
        b.onClick = [this, g]
        {
            processor_.getEngine().setGranularEncoderPage (g);
            rebuildPageControls();
            resized();
        };
        b.setVisible (false);
        addAndMakeVisible (b);
    }

    auto& apvts = processor_.getValueTreeState();

    for (int m = 0; m < sculpt::kNumMacros; ++m)
    {
        const auto ms = static_cast<size_t> (m);
        auto& slider = macroSliders_[ms];
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::trackColourId, kAccent);
        addAndMakeVisible (slider);
        macroAttachments_[ms] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "macro" + juce::String (m + 1), slider);
    }

    addAndMakeVisible (loadSampleButton_);
    loadSampleButton_.onClick = [this]
    {
        if (sampleChooser_ != nullptr)
            return;

        sampleChooser_ = std::make_unique<juce::FileChooser> (
            "Load sample into selected track",
            juce::File {},
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        constexpr auto browserFlags = juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles;

        sampleChooser_->launchAsync (browserFlags, [this] (const juce::FileChooser& fc)
        {
            const juce::File f (fc.getResult());
            if (f.existsAsFile())
                processor_.loadAudioFileIntoTrack (getSelectedTrackFromParameter(), f);

            sampleChooser_.reset();
        });
    };

    addAndMakeVisible (spaceClearButton_);
    spaceClearButton_.setTooltip ("Clears delay and reverb buffers on the selected track (next audio block).");
    spaceClearButton_.onClick = [this]
    { processor_.requestSpaceClear (getSelectedTrackFromParameter()); };

    addAndMakeVisible (helpLabel_);
    helpLabel_.setJustificationType (juce::Justification::centredLeft);
    helpLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.88f));
    helpLabel_.setMinimumHorizontalScale (1.0f);
    helpLabel_.setFont (juce::FontOptions (11.0f));
    helpLabel_.setText ("Drag WAV/AIFF/FLAC/OGG here or use LOAD. Scenes A-D: enable SAVE then tap a letter to store.",
                        juce::dontSendNotification);
    helpLabel_.setTooltip (
        "Material page: push Input Capture past halfway to arm recording from the plugin input while the track plays "
        "(circular buffer, about 8 seconds).");

    instrumentPanel_.setScreenProvider ([this]() -> const sculpt::ScreenModel& {
        return processor_.getEngine().getScreenModel();
    });
    instrumentPanel_.setBpmDragHandler ([this] (double bpm) { processor_.applyUserBpm (bpm); });
    addAndMakeVisible (instrumentPanel_);

    modPagePanel_ = std::make_unique<ModPagePanel> (processor_);
    modPageViewport_.setScrollBarsShown (true, false);
    modPageViewport_.setScrollBarThickness (10);
    modPageViewport_.setViewedComponent (modPagePanel_.get(), false);
    addAndMakeVisible (modPageViewport_);
    modPageViewport_.setVisible (false);

    rebuildPageControls();
    for (auto& b : granularEncoderPageButtons_)
        b.setVisible (currentPage_ == sculpt::Page::Granular || currentPage_ == sculpt::Page::Material);
    instrumentPanel_.setUiPage (currentPage_);
    // 60 Hz: LCD (mod hints, Mod oscilloscope, meters) tracks audio-thread ScreenModel with less lag than 30 Hz.
    startTimerHz (60);
    // Resizable so the full knob grid (3 rows on Material/Space) fits on shorter laptop
    // screens; layout in resized() is proportional. Default trimmed from 900 -> 820 tall.
    setResizable (true, true);
    setResizeLimits (900, 700, 1600, 1200);
    setSize (980, 880);
}

int SculptSamplerAudioProcessorEditor::getSelectedTrackFromParameter() const
{
    if (auto* p = processor_.getValueTreeState().getParameter ("selectedTrack"))
        return juce::jlimit (0, sculpt::kNumTracks - 1,
                             juce::roundToInt (p->getValue() * (sculpt::kNumTracks - 1)));
    return 0;
}

void SculptSamplerAudioProcessorEditor::selectTrack (int trackIndex)
{
    if (auto* p = processor_.getValueTreeState().getParameter ("selectedTrack"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (static_cast<float> (trackIndex) / static_cast<float> (sculpt::kNumTracks - 1));
        p->endChangeGesture();
    }
}

void SculptSamplerAudioProcessorEditor::toggleStepPage()
{
    stepPage_ = stepPage_ == 0 ? 1 : 0;
    stepPageButton_.setButtonText (stepPage_ == 0 ? "1-16" : "17-32");
    for (int i = 0; i < sculpt::kStepsPerPage; ++i)
        stepButtons_[static_cast<size_t> (i)].setButtonText (
            juce::String (stepPage_ * sculpt::kStepsPerPage + i + 1));
}

void SculptSamplerAudioProcessorEditor::toggleSelectedMachine()
{
    const int track = getSelectedTrackFromParameter();
    const auto pid  = bridge::paramIdString (track, sculpt::ParameterId::MaterialMachine);
    auto* p = processor_.getValueTreeState().getParameter (pid);
    if (p == nullptr)
        return;

    const bool nowSampler = ! (p->getValue() > 0.5f);
    p->beginChangeGesture();
    p->setValueNotifyingHost (nowSampler ? 1.0f : 0.0f);
    p->endChangeGesture();

    // Make the change audible immediately: a Sampler track is silent until the sequencer trigs it;
    // switching back to Torso restores the free-running voice.
    auto& engine = processor_.getEngine();
    if (nowSampler)
        engine.stopTrack (track);
    else
        engine.triggerTrack (track);
}

void SculptSamplerAudioProcessorEditor::selectPage (sculpt::Page page)
{
    currentPage_ = page;
    processor_.getEngine().setSelectedPage (page);
    instrumentPanel_.setUiPage (currentPage_);
    for (auto& b : granularEncoderPageButtons_)
        b.setVisible (currentPage_ == sculpt::Page::Granular || currentPage_ == sculpt::Page::Material);
    rebuildPageControls();
    resized();
}

void SculptSamplerAudioProcessorEditor::rebuildPageControls()
{
    using namespace sculpt_editor;

    pageControls_.clear();
    spaceTimeSlider_ = nullptr;
    auto& apvts = processor_.getValueTreeState();
    const int track = getSelectedTrackFromParameter();
    lastBuiltTrack_ = track;

    if (currentPage_ == sculpt::Page::Mod)
    {
        modPagePanel_->refreshFromEngine();
        return;
    }

    const bool usesEncoderPages = (currentPage_ == sculpt::Page::Granular
                                    || currentPage_ == sculpt::Page::Material);
    const int granularEncPage = usesEncoderPages
                                    ? processor_.getEngine().getGranularEncoderPage()
                                    : 0;

    for (int slot = 0; slot < sculpt::kMaxParamsPerPage; ++slot)
    {
        const auto id = sculpt::PageModel::parameterForSlot (currentPage_, slot, granularEncPage);
        if (id == sculpt::ParameterId::Count)
            break;

        PageControl control;
        if (id == sculpt::ParameterId::SpaceFreeze)
        {
            control.spaceFreezeToggle = std::make_unique<juce::ToggleButton> ("Freeze");
            control.spaceFreezeToggle->setClickingTogglesState (true);
            control.spaceFreezeToggle->setColour (juce::ToggleButton::textColourId, kText);
            control.spaceFreezeToggle->setColour (juce::ToggleButton::tickColourId, kAccent);
            addAndMakeVisible (*control.spaceFreezeToggle);

            control.label = std::make_unique<juce::Label>();
            control.label->setText (sculpt::parameterName (id), juce::dontSendNotification);
            control.label->setJustificationType (juce::Justification::centred);
            control.label->setColour (juce::Label::textColourId, kText);
            control.label->setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (*control.label);

            const auto paramId = bridge::paramIdString (track, id);
            if (apvts.getParameter (paramId) != nullptr)
                control.spaceFreezeAttachment =
                    std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                        apvts, paramId, *control.spaceFreezeToggle);
        }
        else
        {
            control.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                             juce::Slider::NoTextBox);
            control.slider->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
            control.slider->setColour (juce::Slider::textBoxTextColourId, kText);
            control.slider->setColour (juce::Slider::textBoxOutlineColourId, kText.withAlpha (0.25f));
            control.slider->setColour (juce::Slider::textBoxBackgroundColourId, kBackground);
            control.slider->setTextBoxIsEditable (false);
            control.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 14);
            control.slider->setNumDecimalPlacesToDisplay (2);
            addAndMakeVisible (*control.slider);

            control.label = std::make_unique<juce::Label>();
            control.label->setText (sculpt::parameterName (id), juce::dontSendNotification);
            control.label->setJustificationType (juce::Justification::centred);
            control.label->setColour (juce::Label::textColourId, kText);
            control.label->setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (*control.label);

            const auto paramId = bridge::paramIdString (track, id);
            if (apvts.getParameter (paramId) != nullptr)
                control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, paramId, *control.slider);

            // Delay Time: show the musical/free value (matching the LCD + Time Mode),
            // not the raw normalized number. Set after the attachment so it wins.
            if (id == sculpt::ParameterId::SpaceDelayTime)
            {
                control.slider->textFromValueFunction = [this] (double value)
                {
                    return juce::String (sculpt::formatSpaceDelayTime (static_cast<float> (value), spaceTimeModeIdx_));
                };
                control.slider->updateText();
                spaceTimeSlider_ = control.slider.get();
            }
            // Sampler controls: show readable values (mode name / slice count) instead of 0..1.
            else if (id == sculpt::ParameterId::MaterialSampleMode)
            {
                control.slider->textFromValueFunction = [] (double value)
                {
                    return juce::String (value > 0.5 ? "Slice" : "1-Shot");
                };
                control.slider->updateText();
            }
            else if (id == sculpt::ParameterId::MaterialSliceCount)
            {
                control.slider->textFromValueFunction = [] (double value)
                {
                    return juce::String (sculpt::map::materialSliceCount (static_cast<float> (value)));
                };
                control.slider->updateText();
            }
        }

        pageControls_.push_back (std::move (control));
    }
}

void SculptSamplerAudioProcessorEditor::timerCallback()
{
    const int track = getSelectedTrackFromParameter();
    if (track != lastBuiltTrack_)
    {
        rebuildPageControls();
        resized();
    }

    instrumentPanel_.setUiPage (currentPage_);

    const auto& screen = processor_.getEngine().getScreenModel();

    // Keep the Space Time rotary's text box in sync with the current Time Mode.
    if (spaceTimeSlider_ != nullptr && currentPage_ == sculpt::Page::Space
        && screen.space.delayTimeMode != spaceTimeModeIdx_)
    {
        spaceTimeModeIdx_ = screen.space.delayTimeMode;
        spaceTimeSlider_->updateText();
    }

    for (int t = 0; t < sculpt::kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        trackButtons_[ts].setToggleState (t == track, juce::dontSendNotification);
        // Muted tracks show red; selected (toggle-on) shows the accent and takes visual precedence.
        const bool muted = processor_.getEngine().isTrackMuted (t);
        trackButtons_[ts].setColour (juce::TextButton::buttonColourId,
                                     muted ? sculpt_editor::kMute
                                           : sculpt_editor::kBackground.brighter (0.15f));
    }

    // Step sequencer row.
    seqPlayButton_.setToggleState (screen.seqPlaying, juce::dontSendNotification);
    seqPlayButton_.setButtonText (screen.seqPlaying ? "STOP ALL" : "PLAY ALL");

    bool selSampler = false;
    if (auto* mp = processor_.getValueTreeState().getParameter (
            bridge::paramIdString (track, sculpt::ParameterId::MaterialMachine)))
        selSampler = mp->getValue() > 0.5f;
    machineButton_.setToggleState (selSampler, juce::dontSendNotification);
    machineButton_.setButtonText (selSampler ? "SAMP" : "TORSO");

    for (int i = 0; i < sculpt::kStepsPerPage; ++i)
    {
        const int step = stepPage_ * sculpt::kStepsPerPage + i;
        auto& b = stepButtons_[static_cast<size_t> (i)];
        b.setToggleState (processor_.getEngine().getStepTrig (track, step), juce::dontSendNotification);
        const bool current = screen.seqPlaying && screen.seqCurrentStep == step;
        b.setColour (juce::TextButton::buttonColourId,
                     current ? sculpt_editor::kMeter : sculpt_editor::kBackground.brighter (0.15f));
    }

    for (int pg = 0; pg < static_cast<int> (sculpt::Page::Count); ++pg)
        pageButtons_[static_cast<size_t> (pg)].setToggleState (
            static_cast<sculpt::Page> (pg) == currentPage_, juce::dontSendNotification);

    const int gEnc = processor_.getEngine().getGranularEncoderPage();
    for (int g = 0; g < 2; ++g)
        granularEncoderPageButtons_[static_cast<size_t> (g)].setToggleState (
            g == gEnc, juce::dontSendNotification);

    auto& engine = processor_.getEngine();
    if (screen.selectedPage == sculpt::Page::Material
        || screen.selectedPage == sculpt::Page::Granular)
    {
        std::array<float, sculpt::kMaterialWaveformBins> env {};
        const bool materialZoom = (currentPage_ == sculpt::Page::Material);
        engine.fillMaterialWaveformEnvelope (track, sculpt::kMaterialWaveformBins, env.data (), materialZoom);
        instrumentPanel_.setWaveformEnvelope (env.data(), sculpt::kMaterialWaveformBins);

        // Secondary strip is a full-sample overview: always un-zoomed regardless of Wave Zoom.
        std::array<float, sculpt::kMaterialWaveformBins> overview {};
        engine.fillMaterialWaveformEnvelope (track, sculpt::kMaterialWaveformBins, overview.data (), false);
        instrumentPanel_.setOverviewEnvelope (overview.data(), sculpt::kMaterialWaveformBins);
    }
    else
    {
        instrumentPanel_.clearWaveformEnvelope();
    }

    instrumentPanel_.repaint();
}

void SculptSamplerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (sculpt_editor::kBackground);
}

void SculptSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (30);
    sceneSaveMode_.setBounds (header.removeFromRight (72));
    for (int s = 3; s >= 0; --s)
        sceneButtons_[static_cast<size_t> (s)].setBounds (header.removeFromRight (36).reduced (2, 0));
    titleLabel_.setBounds (header);

    area.removeFromTop (8);

    auto macroRow = area.removeFromTop (28);
    const int macroCell = macroRow.getWidth() / sculpt::kNumMacros;
    for (int m = 0; m < sculpt::kNumMacros; ++m)
        macroSliders_[static_cast<size_t> (m)].setBounds (macroRow.removeFromLeft (macroCell).reduced (4, 2));

    area.removeFromTop (8);

    const int actionH = 44;
    const int trackH = 68;
    const int deviceH = 32;
    const int seqH = 30;

    auto actionRow = area.removeFromBottom (actionH);
    auto trackRow = area.removeFromBottom (trackH);
    auto deviceRow = area.removeFromBottom (deviceH);
    area.removeFromBottom (6);
    auto seqRow = area.removeFromBottom (seqH);

    area.removeFromBottom (8);

    // Step sequencer row: transport + machine + page toggle on the left, 16 step buttons on the right.
    seqPlayButton_.setBounds (seqRow.removeFromLeft (76).reduced (2, 3));
    machineButton_.setBounds (seqRow.removeFromLeft (74).reduced (2, 3));
    stepPageButton_.setBounds (seqRow.removeFromLeft (56).reduced (2, 3));
    seqRow.removeFromLeft (6);
    {
        const int stepCell = juce::jmax (1, seqRow.getWidth() / sculpt::kStepsPerPage);
        for (int i = 0; i < sculpt::kStepsPerPage; ++i)
            stepButtons_[static_cast<size_t> (i)].setBounds (seqRow.removeFromLeft (stepCell).reduced (1, 3));
    }

    const int pageBtnW = deviceRow.getWidth() / static_cast<int> (sculpt::Page::Count);
    for (int pg = 0; pg < static_cast<int> (sculpt::Page::Count); ++pg)
        pageButtons_[static_cast<size_t> (pg)].setBounds (deviceRow.removeFromLeft (pageBtnW).reduced (3, 4));

    const int trackCell = trackRow.getWidth() / sculpt::kNumTracks;
    for (int t = 0; t < sculpt::kNumTracks; ++t)
        trackButtons_[static_cast<size_t> (t)].setBounds (trackRow.removeFromLeft (trackCell).reduced (3));

    muteButton_.setBounds (actionRow.removeFromLeft (70).reduced (0, 6));
    actionRow.removeFromLeft (6);
    loadSampleButton_.setBounds (actionRow.removeFromLeft (88).reduced (0, 6));
    spaceClearButton_.setBounds (actionRow.removeFromLeft (100).reduced (0, 6));
    helpLabel_.setBounds (actionRow.reduced (8, 6));

    // Remaining `area`: LCD + VALUE encoders. Split SELECT (left) vs instrument column.
    const int selectColW = 56;
    auto selectCol = area.removeFromLeft (selectColW);
    auto lcdKnobStack = area;

    selectLabel_.setBounds (selectCol.removeFromTop (20));
    selectSlider_.setBounds (selectCol.reduced (4, 4));

    const int knobMinH = 200;
    const int lcdMinH = 300;
    const int available = juce::jmax (lcdMinH + knobMinH, lcdKnobStack.getHeight());
    // Many encoder rows (e.g. Material/Space, 3 rows): give the knob stack more of the column
    // so each knob stays large enough to read.
    const float lcdFrac =
        (static_cast<int> (pageControls_.size()) > 8) ? 0.44f : 0.6f;
    const int lcdH = juce::jlimit (lcdMinH, 440, juce::roundToInt (static_cast<float> (available) * lcdFrac));

    auto lcdBounds = lcdKnobStack.removeFromTop (lcdH);
    instrumentPanel_.setBounds (lcdBounds);

    lcdKnobStack.removeFromTop (6);
    auto knobArea = lcdKnobStack;

    if (currentPage_ == sculpt::Page::Mod && modPagePanel_ != nullptr)
    {
        modPageViewport_.setVisible (true);
        modPageViewport_.setBounds (knobArea);
        const int contentW = juce::jmax (200, knobArea.getWidth() - 12);
        modPagePanel_->setSize (contentW, ModPagePanel::recommendedScrollableHeight());
        return;
    }

    modPageViewport_.setVisible (false);

    if (currentPage_ == sculpt::Page::Granular || currentPage_ == sculpt::Page::Material)
    {
        auto strip = knobArea.removeFromTop (22);
        const int btnW = juce::jmin (36, strip.getWidth() / 8);
        auto right = strip.removeFromRight (btnW * 2 + 8);
        granularEncoderPageButtons_[1].setBounds (right.removeFromRight (btnW).reduced (1, 3));
        granularEncoderPageButtons_[0].setBounds (right.removeFromRight (btnW).reduced (1, 3));
    }

    const int columns = 4;
    const int cellWidth = knobArea.getWidth() / columns;
    const int numRows = juce::jmax (1, (static_cast<int> (pageControls_.size()) + columns - 1) / columns);
    // Fit rows strictly inside knobArea — forcing a min taller than knobArea/rows overlaps page/track rows.
    const int cellHeight = juce::jmax (1, knobArea.getHeight() / numRows);
    for (size_t i = 0; i < pageControls_.size(); ++i)
    {
        const int col = static_cast<int> (i) % columns;
        const int row = static_cast<int> (i) / columns;
        juce::Rectangle<int> cell (knobArea.getX() + col * cellWidth,
                                   knobArea.getY() + row * cellHeight,
                                   cellWidth, cellHeight);
        cell = cell.reduced (8);
        pageControls_[i].label->setBounds (cell.removeFromBottom (16));
        if (pageControls_[i].slider != nullptr)
            pageControls_[i].slider->setBounds (cell);
        else if (pageControls_[i].spaceFreezeToggle != nullptr)
            pageControls_[i].spaceFreezeToggle->setBounds (cell.reduced (6, 10));
        else if (pageControls_[i].loopSnapToggle != nullptr)
            pageControls_[i].loopSnapToggle->setBounds (cell.reduced (6, 10));
    }
}

bool SculptSamplerAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (juce::File (path).hasFileExtension (".wav;.aif;.aiff;.flac;.ogg"))
            return true;

    return false;
}

void SculptSamplerAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty())
        processor_.loadAudioFileIntoTrack (getSelectedTrackFromParameter(), juce::File (files[0]));
}
