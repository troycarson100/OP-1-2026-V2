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

    // Metronome toggle, up by the BPM in the header.
    metroButton_.setClickingTogglesState (true);
    metroButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    metroButton_.setTooltip ("Metronome: click on every beat, accented on the bar downbeat.");
    metroButton_.onClick = [this] { processor_.getEngine().setMetronomeEnabled (metroButton_.getToggleState()); };
    addAndMakeVisible (metroButton_);

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
            const int step = stepPage_ * sculpt::kStepsPerPage + i;
            if (plockMode_)
            {
                plockStep_ = step;   // target this step; knobs now edit its locks
                enterLockEdit();
            }
            else
                processor_.getEngine().toggleStepTrig (getSelectedTrackFromParameter(), step);
        };
        addAndMakeVisible (b);
    }

    // P-LOCK arm: when armed, click a step to target it, then turn page knobs to lock that step's
    // parameters (knobs detach from the live values while editing).
    plockButton_.setClickingTogglesState (true);
    plockButton_.setColour (juce::TextButton::buttonOnColourId, kParamModDot);
    plockButton_.setTooltip ("Arm parameter-lock editing: pick a step, then turn a page knob to lock that "
                             "parameter for that step. Un-arm to return the knobs to the live values.");
    plockButton_.onClick = [this]
    {
        plockMode_ = plockButton_.getToggleState();
        if (plockMode_)
        {
            if (plockStep_ >= 0)
                enterLockEdit();
        }
        else
        {
            plockStep_ = -1;
            rebuildPageControls(); // re-attach knobs to the live params
            resized();
        }
    };
    addAndMakeVisible (plockButton_);

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

    addAndMakeVisible (bankButton_);
    bankButton_.setTooltip ("Open the project sample bank browser: mark multiple files, load them all at once.");
    bankButton_.onClick = [this] { openSampleBrowser(); };

    addAndMakeVisible (spaceClearButton_);
    spaceClearButton_.setTooltip ("Clears delay and reverb buffers on the selected track (next audio block).");
    spaceClearButton_.onClick = [this]
    { processor_.requestSpaceClear (getSelectedTrackFromParameter()); };

    addAndMakeVisible (helpLabel_);
    helpLabel_.setJustificationType (juce::Justification::centredLeft);
    helpLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.88f));
    helpLabel_.setMinimumHorizontalScale (1.0f);
    helpLabel_.setFont (juce::FontOptions (11.0f));
    helpLabel_.setText ("Drag WAV/AIFF/FLAC/OGG or use LOAD to add to the project sample bank (A1, A2...). "
                        "Turn the Sample knob to pick a bank slot; p-lock it per step. "
                        "Scenes A-D: enable SAVE then tap a letter to store.",
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

SculptSamplerAudioProcessorEditor::~SculptSamplerAudioProcessorEditor()
{
    // The browser window's callbacks capture `this`; delete it synchronously before we go away.
    if (browserWindow_ != nullptr)
        delete browserWindow_.getComponent();
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
        control.id = id;
        if (id == sculpt::ParameterId::GlobalSpaceFreeze)
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

            // P-lock editing: while a step is targeted in P-LOCK mode the knob is detached from the
            // live param (see enterLockEdit), so turning it writes/updates this step's lock only.
            // Locks store the parameter's normalized 0..1 value (convertTo0to1 handles choice/stepped
            // params whose slider range isn't 0..1).
            {
                auto* sliderPtr = control.slider.get();
                control.slider->onValueChange = [this, id, sliderPtr]
                {
                    if (! (plockMode_ && plockStep_ >= 0))
                        return;
                    if (! sculpt::isTrackParameter (id))   // global FX params aren't per-step-lockable
                        return;
                    const int track = getSelectedTrackFromParameter();
                    float norm = static_cast<float> (sliderPtr->getValue());
                    if (auto* p = processor_.getValueTreeState().getParameter (bridge::paramIdString (track, id)))
                        norm = p->convertTo0to1 (static_cast<float> (sliderPtr->getValue()));
                    processor_.getEngine().setStepLock (track, plockStep_, id, norm);
                };
            }

            // Delay Time: show the musical/free value (matching the LCD + Time Mode),
            // not the raw normalized number. Set after the attachment so it wins.
            if (id == sculpt::ParameterId::GlobalDelayTime)
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

    // If we rebuilt while editing locks (e.g. track changed), re-detach and reload the step's locks.
    if (plockMode_ && plockStep_ >= 0)
        enterLockEdit();
}

void SculptSamplerAudioProcessorEditor::enterLockEdit()
{
    const int track = getSelectedTrackFromParameter();
    auto& apvts = processor_.getValueTreeState();
    auto& engine = processor_.getEngine();

    for (auto& pc : pageControls_)
    {
        if (pc.slider == nullptr)
            continue;

        // Detach from the live parameter so edits only touch the step's lock, not the live value.
        pc.attachment.reset();

        auto* p = apvts.getParameter (bridge::paramIdString (track, pc.id));
        float lockNorm = 0.0f;
        if (engine.getStepLock (track, plockStep_, pc.id, lockNorm))
            // Show the existing lock (convert normalized -> the slider's natural range).
            pc.slider->setValue (p != nullptr ? p->convertFrom0to1 (lockNorm) : lockNorm,
                                 juce::dontSendNotification);
        else if (p != nullptr)
            // Seed from the current live value so a new lock starts where the knob is.
            pc.slider->setValue (p->convertFrom0to1 (p->getValue()), juce::dontSendNotification);
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
        auto& engine2 = processor_.getEngine();
        b.setToggleState (engine2.getStepTrig (track, step), juce::dontSendNotification);

        const bool current  = screen.seqPlaying && screen.seqCurrentStep == step;
        const bool targeted = plockMode_ && plockStep_ == step;
        const bool hasLocks = engine2.stepLockCount (track, step) > 0;

        juce::Colour bg = sculpt_editor::kBackground.brighter (0.15f);
        if (targeted)      bg = sculpt_editor::kText.withAlpha (0.55f);  // selected for lock editing
        else if (current)  bg = sculpt_editor::kMeter;                   // playhead
        else if (hasLocks) bg = sculpt_editor::kParamModDot;             // has p-locks
        b.setColour (juce::TextButton::buttonColourId, bg);

        // Mark locked steps with a trailing dot so locks are visible even on trig (orange) steps.
        b.setButtonText (juce::String (step + 1) + (hasLocks ? " *" : ""));
    }

    // Knob following: during playback (when not editing a step's locks), the page knobs reflect the
    // selected track's effective values, so p-locks are visible jumping per step. dontSendNotification
    // moves the knob visually without writing back to the live (base) parameter.
    {
        auto& apvts3 = processor_.getValueTreeState();
        const bool lockEditing = plockMode_ && plockStep_ >= 0;
        const bool following   = screen.seqPlaying && ! lockEditing;
        if (following)
        {
            for (auto& pc : pageControls_)
            {
                if (pc.slider == nullptr || pc.slider->isMouseButtonDown())
                    continue;
                // Global params (e.g. the Space-page global FX, Mixer OutputGain) aren't per-track and
                // have no per-step effective value to follow; leave their knobs on the live value.
                if (! sculpt::isTrackParameter (pc.id))
                    continue;
                const float eff = processor_.getEngine().getEffectiveTrackParameter (track, pc.id);
                float natural = eff;
                if (auto* p = apvts3.getParameter (bridge::paramIdString (track, pc.id)))
                    natural = p->convertFrom0to1 (eff);
                pc.slider->setValue (natural, juce::dontSendNotification);
            }
            knobsFollowing_ = true;
        }
        else if (knobsFollowing_)
        {
            // Follow ended (playback stopped): resync knobs to their live values, unless lock-editing
            // (enterLockEdit already loaded the targeted step's values).
            knobsFollowing_ = false;
            if (! lockEditing)
                for (auto& pc : pageControls_)
                    if (pc.slider != nullptr)
                        if (auto* p = apvts3.getParameter (bridge::paramIdString (track, pc.id)))
                            pc.slider->setValue (p->convertFrom0to1 (p->getValue()), juce::dontSendNotification);
        }
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
    metroButton_.setBounds (header.removeFromRight (62).reduced (2, 2));
    titleLabel_.setBounds (header);

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
    plockButton_.setBounds (seqRow.removeFromLeft (64).reduced (2, 3));
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
    loadSampleButton_.setBounds (actionRow.removeFromLeft (72).reduced (0, 6));
    actionRow.removeFromLeft (4);
    bankButton_.setBounds (actionRow.removeFromLeft (72).reduced (0, 6));
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

void SculptSamplerAudioProcessorEditor::openSampleBrowser()
{
    if (browserWindow_ != nullptr)
    {
        browserWindow_->toFront (true);
        return;
    }

    juce::File start = lastBrowseDir_;
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    auto* browser = new SampleBrowser (start);
    const int track = getSelectedTrackFromParameter();

    browser->onLoad = [this, track] (const juce::Array<juce::File>& files)
    {
        if (! files.isEmpty())
        {
            lastBrowseDir_ = files.getFirst().getParentDirectory();
            processor_.loadAudioFilesIntoBank (files, track);
        }
        closeSampleBrowser();
    };
    browser->onCancel = [this] { closeSampleBrowser(); };

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (browser);
    o.dialogTitle = "Sample Bank";
    o.dialogBackgroundColour = juce::Colour (0xff232327);
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;

    browserWindow_ = o.launchAsync();
    if (browserWindow_ != nullptr)
        browser->grabKeyboardFocus();
}

void SculptSamplerAudioProcessorEditor::closeSampleBrowser()
{
    // Defer the window delete: this is invoked from a button callback inside the window's content.
    juce::Component::SafePointer<juce::DialogWindow> w = browserWindow_;
    browserWindow_ = nullptr;
    juce::MessageManager::callAsync ([w]() mutable
    {
        if (w != nullptr)
            delete w.getComponent();
    });
}
