#include "PluginEditor.h"
#include "ui/juce/EditorColours.h"
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
        select.onClick = [this, t] { selectTrack (t); };
        addAndMakeVisible (select);

        auto& play = playButtons_[static_cast<size_t> (t)];
        play.setButtonText ("PLAY");
        play.onClick = [this, t]
        {
            auto& engine = processor_.getEngine();
            if (engine.getScreenModel().trackPlaying[static_cast<size_t> (t)])
                engine.stopTrack (t);
            else
                engine.triggerTrack (t);
        };
        addAndMakeVisible (play);
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
        b.setTooltip (g == 0 ? "Granular encoders: position, size, density, pitch, spray, texture, spread, mix."
                              : "Granular encoders: sync, Euclidean steps / pulses / rotate, pitch quantize.");
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
        b.setVisible (currentPage_ == sculpt::Page::Granular);
    instrumentPanel_.setUiPage (currentPage_);
    // 60 Hz: LCD (mod hints, Mod oscilloscope, meters) tracks audio-thread ScreenModel with less lag than 30 Hz.
    startTimerHz (60);
    setSize (980, 840);
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

void SculptSamplerAudioProcessorEditor::selectPage (sculpt::Page page)
{
    currentPage_ = page;
    processor_.getEngine().setSelectedPage (page);
    instrumentPanel_.setUiPage (currentPage_);
    for (auto& b : granularEncoderPageButtons_)
        b.setVisible (currentPage_ == sculpt::Page::Granular);
    rebuildPageControls();
    resized();
}

void SculptSamplerAudioProcessorEditor::rebuildPageControls()
{
    using namespace sculpt_editor;

    pageControls_.clear();
    auto& apvts = processor_.getValueTreeState();
    const int track = getSelectedTrackFromParameter();
    lastBuiltTrack_ = track;

    if (currentPage_ == sculpt::Page::Mod)
    {
        modPagePanel_->refreshFromEngine();
        return;
    }

    const int granularEncPage = (currentPage_ == sculpt::Page::Granular)
                                    ? processor_.getEngine().getGranularEncoderPage()
                                    : 0;

    for (int slot = 0; slot < sculpt::kMaxParamsPerPage; ++slot)
    {
        const auto id = sculpt::PageModel::parameterForSlot (currentPage_, slot, granularEncPage);
        if (id == sculpt::ParameterId::Count)
            break;

        PageControl control;
        control.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                         juce::Slider::NoTextBox);
        control.slider->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
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

        if (id == sculpt::ParameterId::TapeSpeed && currentPage_ == sculpt::Page::Material)
        {
            control.tapeSnapToggle = std::make_unique<juce::ToggleButton>();
            control.tapeSnapToggle->setButtonText ({});
            control.tapeSnapToggle->setClickingTogglesState (true);
            control.tapeSnapToggle->setTooltip (
                "Tape mode: snap knob to 0/0.25/0.5/0.75/1. Warp mode: snap varispeed to 0.25x steps (1.0x, 1.25x, …).");
            control.tapeSnapToggle->setColour (juce::ToggleButton::tickColourId, sculpt_editor::kAccent);
            control.tapeSnapToggle->setColour (juce::ToggleButton::tickDisabledColourId,
                                               sculpt_editor::kText.withAlpha (0.4f));
            addAndMakeVisible (*control.tapeSnapToggle);
            const auto snapPid = bridge::paramIdString (track, sculpt::ParameterId::TapeSpeedSnap);
            if (apvts.getParameter (snapPid) != nullptr)
                control.tapeSnapAttachment =
                    std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                        apvts, snapPid, *control.tapeSnapToggle);
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
    for (int t = 0; t < sculpt::kNumTracks; ++t)
    {
        const auto ts = static_cast<size_t> (t);
        trackButtons_[ts].setToggleState (t == track, juce::dontSendNotification);
        playButtons_[ts].setButtonText (screen.trackPlaying[ts] ? "STOP" : "PLAY");
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

    auto actionRow = area.removeFromBottom (actionH);
    auto trackRow = area.removeFromBottom (trackH);
    auto deviceRow = area.removeFromBottom (deviceH);

    area.removeFromBottom (8);

    const int pageBtnW = deviceRow.getWidth() / static_cast<int> (sculpt::Page::Count);
    for (int pg = 0; pg < static_cast<int> (sculpt::Page::Count); ++pg)
        pageButtons_[static_cast<size_t> (pg)].setBounds (deviceRow.removeFromLeft (pageBtnW).reduced (3, 4));

    const int trackCell = trackRow.getWidth() / sculpt::kNumTracks;
    for (int t = 0; t < sculpt::kNumTracks; ++t)
    {
        auto cell = trackRow.removeFromLeft (trackCell).reduced (4);
        trackButtons_[static_cast<size_t> (t)].setBounds (cell.removeFromTop (34));
        playButtons_[static_cast<size_t> (t)].setBounds (cell.reduced (6, 2));
    }

    loadSampleButton_.setBounds (actionRow.removeFromLeft (88).reduced (0, 6));
    helpLabel_.setBounds (actionRow.reduced (8, 6));

    // Remaining `area`: LCD + VALUE encoders. Split SELECT (left) vs instrument column.
    const int selectColW = 56;
    auto selectCol = area.removeFromLeft (selectColW);
    auto lcdKnobStack = area;

    selectLabel_.setBounds (selectCol.removeFromTop (20));
    selectSlider_.setBounds (selectCol.reduced (4, 4));

    const int knobMinH = 160;
    const int lcdMinH = 350;
    const int available = juce::jmax (lcdMinH + knobMinH, lcdKnobStack.getHeight());
    const int lcdH = juce::jlimit (lcdMinH, 440, juce::roundToInt (available * 0.65f));

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

    if (currentPage_ == sculpt::Page::Granular)
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
    const int cellHeight = juce::jmax (72, knobArea.getHeight() / numRows);
    for (size_t i = 0; i < pageControls_.size(); ++i)
    {
        const int col = static_cast<int> (i) % columns;
        const int row = static_cast<int> (i) / columns;
        juce::Rectangle<int> cell (knobArea.getX() + col * cellWidth,
                                   knobArea.getY() + row * cellHeight,
                                   cellWidth, cellHeight);
        cell = cell.reduced (8);
        pageControls_[i].label->setBounds (cell.removeFromBottom (16));
        pageControls_[i].slider->setBounds (cell);
        if (pageControls_[i].tapeSnapToggle != nullptr)
        {
            constexpr int sq        = 12;
            constexpr int marginR   = 10; // keep inside cell; column edge was clipping the right side
            constexpr int marginTop = 3;
            const int     x         = cell.getRight() - sq - marginR;
            pageControls_[i].tapeSnapToggle->setBounds (x, cell.getY() + marginTop, sq, sq);
            pageControls_[i].tapeSnapToggle->toFront (false);
        }
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
