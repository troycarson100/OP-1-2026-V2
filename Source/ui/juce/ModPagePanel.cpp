#include "ModPagePanel.h"
#include "../../PluginProcessor.h"
#include "../../ui/PageModel.h"
#include "../../core/ParameterIds.h"
#include "../../modulation/SyncDivision.h"
#include "../../util/MathUtils.h"

namespace
{
    constexpr int kNumMapPages = sculpt::kModMapTargetPages;

    sculpt::Page pageFromMapIndex (int idx)
    {
        return static_cast<sculpt::Page> (juce::jlimit (0, kNumMapPages - 1, idx));
    }

    int readSelectedTrack (SculptSamplerAudioProcessor& proc)
    {
        if (auto* p = proc.getValueTreeState().getParameter ("selectedTrack"))
            return juce::jlimit (0, sculpt::kNumTracks - 1,
                                 juce::roundToInt (p->getValue() * (sculpt::kNumTracks - 1)));
        return 0;
    }
}

ModPagePanel::ModPagePanel (SculptSamplerAudioProcessor& processor)
    : processor_ (processor)
{
    addAndMakeVisible (title_);
    title_.setFont (juce::FontOptions (15.0f, juce::Font::bold));

    addAndMakeVisible (slotLabel_);
    slotCombo_.addItem ("1", 1);
    slotCombo_.addItem ("2", 2);
    slotCombo_.addItem ("3", 3);
    slotCombo_.addItem ("4", 4);
    slotCombo_.setSelectedId (1, juce::dontSendNotification);
    slotCombo_.onChange = [this]
    {
        if (! suppressCallbacks_)
        {
            refreshFromEngine();
            commitToEngine();
        }
    };
    addAndMakeVisible (slotCombo_);

    addAndMakeVisible (kindLabel_);
    kindCombo_.addItem ("Off", 1);
    kindCombo_.addItem ("Wave", 2);
    kindCombo_.addItem ("Random", 3);
    kindCombo_.addItem ("ADSR", 4);
    kindCombo_.addItem ("Input env", 5);
    kindCombo_.setSelectedId (1, juce::dontSendNotification);
    kindCombo_.onChange = [this]
    {
        kindChanged();
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (kindCombo_);

    trigButton_.onClick = [this]
    {
        const int trk = readSelectedTrack (processor_);
        const int slot = juce::jlimit (0, sculpt::kModSlotsPerTrack - 1, slotCombo_.getSelectedItemIndex());
        processor_.getEngine().triggerModAdsr (trk, slot);
    };
    addAndMakeVisible (trigButton_);

    for (auto* lab : { &waveSection_, &rndSection_, &adsrSection_, &mapSection_ })
    {
        lab->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        lab->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*lab);
    }

    waveSyncT_.onClick = [this]
    {
        syncWaveControlsEnabled();
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (waveSyncT_);
    fillDivisionCombo (waveDivCombo_);
    waveDivCombo_.onChange = [this]
    {
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (waveDivCombo_);

    wireSlider (waveRate_, 0.02f, 18.0f);
    wireSlider (waveAmount_, 0.0f, 100.0f);
    waveAmount_.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };
    waveAmount_.valueFromTextFunction = [] (const juce::String& t) { return static_cast<double> (t.getIntValue()); };

    waveShapeCombo_.addItem ("Sine", 1);
    waveShapeCombo_.addItem ("Triangle", 2);
    waveShapeCombo_.addItem ("Saw up", 3);
    waveShapeCombo_.addItem ("Square", 4);
    waveShapeCombo_.addItem ("Ramp down", 5);
    waveShapeCombo_.setSelectedId (1, juce::dontSendNotification);
    waveShapeCombo_.onChange = [this]
    {
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (waveShapeCombo_);

    rndSyncT_.onClick = [this]
    {
        syncRandomControlsEnabled();
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (rndSyncT_);
    fillDivisionCombo (rndDivCombo_);
    rndDivCombo_.onChange = [this]
    {
        if (! suppressCallbacks_)
            commitToEngine();
    };
    addAndMakeVisible (rndDivCombo_);

    wireSlider (rndRate_, 0.1f, 24.0f);
    wireSlider (rndSlew_, 0.0f, 1.0f);

    wireSlider (adsrA_, 0.001f, 3.0f);
    wireSlider (adsrD_, 0.001f, 3.0f);
    wireSlider (adsrS_, 0.0f, 1.0f);
    wireSlider (adsrR_, 0.001f, 4.0f);

    addAndMakeVisible (mapPageLabel_);
    const char* pageNames[] = { "Material", "Granular", "Filter", "Color", "Space", "Mixer" };
    for (int i = 0; i < kNumMapPages; ++i)
        mapPageCombo_.addItem (pageNames[i], i + 1);
    mapPageCombo_.setSelectedId (2, juce::dontSendNotification);
    mapPageCombo_.onChange = [this]
    {
        if (! suppressCallbacks_)
        {
            refreshFromEngine();
            commitToEngine();
        }
        resized();
    };
    addAndMakeVisible (mapPageCombo_);

    for (auto* h : { &mapHeaderParam_, &mapHeaderPolar_, &mapHeaderDepth_ })
    {
        h->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        h->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*h);
    }
    mapHeaderPolar_.setTooltip ("Tap: Uni (one-sided) vs Bi (signed) modulation for this row.");
    mapHeaderDepth_.setTooltip ("Modulation amount for this destination.");

    waveSyncT_.setClickingTogglesState (true);
    rndSyncT_.setClickingTogglesState (true);

    for (auto& l : { &waveRateL_, &waveShapeL_, &waveAmtL_, &rndRateL_, &rndSlewL_, &adA_, &adD_, &adS_, &adR_ })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }

    for (int i = 0; i < sculpt::kMaxModMappingEncoders; ++i)
    {
        auto& tb = mapPolarT_[static_cast<size_t> (i)];
        tb.setClickingTogglesState (true);
        tb.setTooltip ("Off = Uni (0..1 shaped). On = Bi (signed).");
        tb.onClick = [this, i]
        {
            updatePolarButtonLabel (i);
            configureMappingDepthSlider (i);
            if (! suppressCallbacks_)
                commitToEngine();
        };
        addAndMakeVisible (tb);
        updatePolarButtonLabel (i);

        auto& s = mapDepth_[static_cast<size_t> (i)];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        s.setRange (0.0, 1.0, 0.0001);
        s.onValueChange = [this]
        {
            if (! suppressCallbacks_)
                commitToEngine();
        };
        addAndMakeVisible (s);
    }

    for (auto& s : { &waveRate_, &waveAmount_, &rndRate_, &rndSlew_, &adsrA_, &adsrD_, &adsrS_, &adsrR_ })
        addAndMakeVisible (*s);

    for (auto& l : mapName_)
        addAndMakeVisible (l);

    addAndMakeVisible (waveShapeCombo_);

    kindChanged();
    refreshFromEngine();
}

void ModPagePanel::updatePolarButtonLabel (int row)
{
    if (row < 0 || row >= sculpt::kMaxModMappingEncoders)
        return;
    auto& tb = mapPolarT_[static_cast<size_t> (row)];
    tb.setButtonText (tb.getToggleState() ? "Bi" : "Uni");
}

void ModPagePanel::configureMappingDepthSlider (int row)
{
    if (row < 0 || row >= sculpt::kMaxModMappingEncoders)
        return;
    auto& s = mapDepth_[static_cast<size_t> (row)];
    const bool bi = mapPolarT_[static_cast<size_t> (row)].getToggleState();
    const double v = s.getValue();
    suppressCallbacks_ = true;
    if (bi)
    {
        s.setRange (-1.0, 1.0, 0.001);
        s.setValue (juce::jlimit (-1.0, 1.0, v), juce::dontSendNotification);
    }
    else
    {
        s.setRange (0.0, 1.0, 0.0001);
        s.setValue (juce::jlimit (0.0, 1.0, v), juce::dontSendNotification);
    }
    suppressCallbacks_ = false;
}

void ModPagePanel::fillDivisionCombo (juce::ComboBox& box)
{
    box.clear (juce::dontSendNotification);
    for (int i = 0; i < sculpt::kNumSyncDivisions; ++i)
        box.addItem (sculpt::syncDivisionLabel (i), i + 1);
    box.setSelectedId (9, juce::dontSendNotification);
}

void ModPagePanel::syncWaveControlsEnabled()
{
    const bool sync = waveSyncT_.getToggleState();
    waveDivCombo_.setEnabled (sync);
    waveRate_.setEnabled (! sync);
    waveRateL_.setEnabled (! sync);
}

void ModPagePanel::syncRandomControlsEnabled()
{
    const bool sync = rndSyncT_.getToggleState();
    rndDivCombo_.setEnabled (sync);
    rndRate_.setEnabled (! sync);
    rndRateL_.setEnabled (! sync);
}

void ModPagePanel::wireSlider (juce::Slider& s, float lo, float hi)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    s.setRange (lo, hi, lo < 0.0f ? 0.01 : (hi - lo) * 0.0001);
    s.onValueChange = [this]
    {
        if (! suppressCallbacks_)
            commitToEngine();
    };
}

void ModPagePanel::kindChanged()
{
    const int k = kindCombo_.getSelectedItemIndex();
    const bool wave = (k == 1);
    const bool rnd  = (k == 2);
    const bool adsr = (k == 3);

    waveSection_.setVisible (wave);
    waveSyncT_.setVisible (wave);
    waveDivCombo_.setVisible (wave);
    waveRateL_.setVisible (wave);
    waveRate_.setVisible (wave);
    waveShapeL_.setVisible (wave);
    waveShapeCombo_.setVisible (wave);
    waveAmtL_.setVisible (wave);
    waveAmount_.setVisible (wave);
    if (wave)
        syncWaveControlsEnabled();

    rndSection_.setVisible (rnd);
    rndSyncT_.setVisible (rnd);
    rndDivCombo_.setVisible (rnd);
    rndRateL_.setVisible (rnd);
    rndRate_.setVisible (rnd);
    rndSlewL_.setVisible (rnd);
    rndSlew_.setVisible (rnd);
    if (rnd)
        syncRandomControlsEnabled();

    adsrSection_.setVisible (adsr);
    adA_.setVisible (adsr);
    adsrA_.setVisible (adsr);
    adD_.setVisible (adsr);
    adsrD_.setVisible (adsr);
    adS_.setVisible (adsr);
    adsrS_.setVisible (adsr);
    adR_.setVisible (adsr);
    adsrR_.setVisible (adsr);
    trigButton_.setVisible (adsr);

    resized();
}

void ModPagePanel::refreshFromEngine()
{
    suppressCallbacks_ = true;
    const sculpt::ModPatch& p = processor_.getModPatch();
    const int trk = readSelectedTrack (processor_);
    const int slot = juce::jlimit (0, sculpt::kModSlotsPerTrack - 1, slotCombo_.getSelectedItemIndex());
    const auto& sp = p.slots[static_cast<size_t> (trk)][static_cast<size_t> (slot)];

    slotCombo_.setSelectedItemIndex (slot, juce::dontSendNotification);
    kindCombo_.setSelectedItemIndex (static_cast<int> (sp.kind), juce::dontSendNotification);

    waveSyncT_.setToggleState (sp.waveSync != 0, juce::dontSendNotification);
    waveDivCombo_.setSelectedId (
        juce::jlimit (1, sculpt::kNumSyncDivisions, static_cast<int> (sp.waveDivision) + 1),
        juce::dontSendNotification);
    waveRate_.setValue (sp.waveRateHz, juce::dontSendNotification);
    waveAmount_.setValue (static_cast<double> (sculpt::clamp01 (sp.waveAmount)) * 100.0, juce::dontSendNotification);
    waveShapeCombo_.setSelectedId (juce::jlimit (1, 5, static_cast<int> (sp.waveShape) + 1), juce::dontSendNotification);

    rndSyncT_.setToggleState (sp.randomSync != 0, juce::dontSendNotification);
    rndDivCombo_.setSelectedId (
        juce::jlimit (1, sculpt::kNumSyncDivisions, static_cast<int> (sp.randomDivision) + 1),
        juce::dontSendNotification);
    rndRate_.setValue (sp.randomRateHz, juce::dontSendNotification);
    rndSlew_.setValue (sp.randomSlew01, juce::dontSendNotification);

    adsrA_.setValue (sp.adsrAttackSec, juce::dontSendNotification);
    adsrD_.setValue (sp.adsrDecaySec, juce::dontSendNotification);
    adsrS_.setValue (sp.adsrSustain01, juce::dontSendNotification);
    adsrR_.setValue (sp.adsrReleaseSec, juce::dontSendNotification);

    const int pg = juce::jlimit (0, kNumMapPages - 1, mapPageCombo_.getSelectedItemIndex());
    mapPageCombo_.setSelectedItemIndex (pg, juce::dontSendNotification);
    const auto page = pageFromMapIndex (pg);

    for (int i = 0; i < sculpt::kMaxModMappingEncoders; ++i)
    {
        const sculpt::ParameterId id = sculpt::PageModel::parameterForSlot (page, i);
        const bool active = (id != sculpt::ParameterId::Count);
        mapName_[static_cast<size_t> (i)].setVisible (active);
        mapPolarT_[static_cast<size_t> (i)].setVisible (active);
        mapDepth_[static_cast<size_t> (i)].setVisible (active);

        if (! active)
            continue;

        mapName_[static_cast<size_t> (i)].setText (juce::String (sculpt::parameterName (id)),
                                                   juce::dontSendNotification);
        const auto& d = p.maps[static_cast<size_t> (trk)][static_cast<size_t> (slot)][static_cast<size_t> (pg)][static_cast<size_t> (i)];
        mapPolarT_[static_cast<size_t> (i)].setToggleState (d.bipolar != 0, juce::dontSendNotification);
        updatePolarButtonLabel (i);
        configureMappingDepthSlider (i);
        mapDepth_[static_cast<size_t> (i)].setValue (static_cast<double> (d.depth), juce::dontSendNotification);
    }

    kindChanged();
    syncWaveControlsEnabled();
    syncRandomControlsEnabled();
    suppressCallbacks_ = false;
}

void ModPagePanel::commitToEngine()
{
    sculpt::ModPatch p = processor_.getModPatch();
    const int trk = readSelectedTrack (processor_);
    const int slot = juce::jlimit (0, sculpt::kModSlotsPerTrack - 1, slotCombo_.getSelectedItemIndex());
    auto& sp = p.slots[static_cast<size_t> (trk)][static_cast<size_t> (slot)];

    sp.kind = static_cast<sculpt::ModulatorKind> (juce::jlimit (0, 4, kindCombo_.getSelectedItemIndex()));
    sp.waveSync = waveSyncT_.getToggleState() ? 1u : 0u;
    sp.waveDivision = static_cast<uint8_t> (juce::jlimit (0, sculpt::kNumSyncDivisions - 1, waveDivCombo_.getSelectedItemIndex()));
    sp.waveRateHz   = static_cast<float> (waveRate_.getValue());
    sp.waveAmount   = sculpt::clamp01 (static_cast<float> (waveAmount_.getValue() / 100.0));
    sp.waveShape    = static_cast<uint8_t> (juce::jlimit (0, 4, waveShapeCombo_.getSelectedItemIndex()));

    sp.randomSync = rndSyncT_.getToggleState() ? 1u : 0u;
    sp.randomDivision = static_cast<uint8_t> (juce::jlimit (0, sculpt::kNumSyncDivisions - 1, rndDivCombo_.getSelectedItemIndex()));
    sp.randomRateHz = static_cast<float> (rndRate_.getValue());
    sp.randomSlew01 = static_cast<float> (rndSlew_.getValue());

    sp.adsrAttackSec  = static_cast<float> (adsrA_.getValue());
    sp.adsrDecaySec   = static_cast<float> (adsrD_.getValue());
    sp.adsrSustain01  = static_cast<float> (adsrS_.getValue());
    sp.adsrReleaseSec = static_cast<float> (adsrR_.getValue());

    const int pg = juce::jlimit (0, kNumMapPages - 1, mapPageCombo_.getSelectedItemIndex());
    const auto page = pageFromMapIndex (pg);
    for (int i = 0; i < sculpt::kMaxModMappingEncoders; ++i)
    {
        auto& cell = p.maps[static_cast<size_t> (trk)][static_cast<size_t> (slot)][static_cast<size_t> (pg)][static_cast<size_t> (i)];
        if (sculpt::PageModel::parameterForSlot (page, i) == sculpt::ParameterId::Count)
        {
            cell.depth   = 0.0f;
            cell.bipolar = 0;
            continue;
        }
        cell.depth   = static_cast<float> (mapDepth_[static_cast<size_t> (i)].getValue());
        cell.bipolar = mapPolarT_[static_cast<size_t> (i)].getToggleState() ? 1u : 0u;
    }

    processor_.setModPatch (p);
}

int ModPagePanel::recommendedScrollableHeight()
{
    return 380;
}

void ModPagePanel::resized()
{
    auto placeLabeledSlider = [] (juce::Rectangle<int> row, juce::Label& lab, juce::Slider& s, int labelW)
    {
        lab.setBounds (row.removeFromLeft (labelW));
        s.setBounds (row);
    };

    auto zeroBounds = [] (juce::Component& c) { c.setBounds (0, 0, 0, 0); };

    auto area = getLocalBounds().reduced (8);
    title_.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);

    auto topRow = area.removeFromTop (30);
    slotLabel_.setBounds (topRow.removeFromLeft (36));
    slotCombo_.setBounds (topRow.removeFromLeft (56));
    topRow.removeFromLeft (10);
    kindLabel_.setBounds (topRow.removeFromLeft (44));
    kindCombo_.setBounds (topRow.removeFromLeft (130));
    topRow.removeFromLeft (8);
    trigButton_.setBounds (topRow.removeFromLeft (100));

    area.removeFromTop (8);

    const int midGap   = 10;
    const int minRight = 200;
    int         leftW  = juce::roundToInt (area.getWidth() * 0.42f);
    leftW              = juce::jlimit (160, juce::jmax (160, area.getWidth() - minRight - midGap), leftW);
    auto        leftCol = area.removeFromLeft (leftW);
    area.removeFromLeft (midGap);
    auto rightCol = area;

    constexpr int mapRowH = 24;
    const int     pg    = juce::jlimit (0, kNumMapPages - 1, mapPageCombo_.getSelectedItemIndex());
    const auto    page  = pageFromMapIndex (pg);

    // Left column: mapping (only rows with a real parameter on this page)
    {
        auto L = leftCol;
        mapSection_.setBounds (L.removeFromTop (16));
        auto mpRow = L.removeFromTop (28);
        mapPageLabel_.setBounds (mpRow.removeFromLeft (88));
        mapPageCombo_.setBounds (mpRow.removeFromLeft (juce::jmin (200, juce::jmax (120, mpRow.getWidth()))));
        L.removeFromTop (4);

        bool anyRow = false;
        for (int i = 0; i < sculpt::kMaxModMappingEncoders; ++i)
        {
            if (sculpt::PageModel::parameterForSlot (page, i) != sculpt::ParameterId::Count)
            {
                anyRow = true;
                break;
            }
        }

        if (anyRow)
        {
            auto hdrRow = L.removeFromTop (16);
            mapHeaderParam_.setBounds (hdrRow.removeFromLeft (100));
            mapHeaderPolar_.setBounds (hdrRow.removeFromLeft (44));
            mapHeaderDepth_.setBounds (hdrRow);
        }
        else
        {
            zeroBounds (mapHeaderParam_);
            zeroBounds (mapHeaderPolar_);
            zeroBounds (mapHeaderDepth_);
        }

        for (int i = 0; i < sculpt::kMaxModMappingEncoders; ++i)
        {
            if (sculpt::PageModel::parameterForSlot (page, i) == sculpt::ParameterId::Count)
            {
                zeroBounds (mapName_[static_cast<size_t> (i)]);
                zeroBounds (mapPolarT_[static_cast<size_t> (i)]);
                zeroBounds (mapDepth_[static_cast<size_t> (i)]);
                continue;
            }
            auto row = L.removeFromTop (mapRowH);
            mapName_[static_cast<size_t> (i)].setBounds (row.removeFromLeft (100));
            mapPolarT_[static_cast<size_t> (i)].setBounds (row.removeFromLeft (44));
            mapDepth_[static_cast<size_t> (i)].setBounds (row);
        }
    }

    const int k = kindCombo_.getSelectedItemIndex();
    const bool wave = (k == 1);
    const bool rnd  = (k == 2);
    const bool adsr = (k == 3);

    auto R = rightCol;
    if (wave)
    {
        waveSection_.setBounds (R.removeFromTop (16));
        auto r1 = R.removeFromTop (26);
        waveSyncT_.setBounds (r1.removeFromLeft (72));
        r1.removeFromLeft (8);
        waveDivCombo_.setBounds (r1);
        auto r2 = R.removeFromTop (26);
        waveRateL_.setBounds (r2.removeFromLeft (72));
        r2.removeFromLeft (6);
        waveRate_.setBounds (r2);
        auto r3 = R.removeFromTop (26);
        waveShapeL_.setBounds (r3.removeFromLeft (52));
        r3.removeFromLeft (6);
        waveShapeCombo_.setBounds (r3);
        auto r4 = R.removeFromTop (26);
        placeLabeledSlider (r4, waveAmtL_, waveAmount_, 72);
    }
    else
    {
        zeroBounds (waveSection_);
        zeroBounds (waveSyncT_);
        zeroBounds (waveDivCombo_);
        zeroBounds (waveRateL_);
        zeroBounds (waveRate_);
        zeroBounds (waveShapeL_);
        zeroBounds (waveShapeCombo_);
        zeroBounds (waveAmtL_);
        zeroBounds (waveAmount_);
    }

    if (rnd)
    {
        rndSection_.setBounds (R.removeFromTop (16));
        auto n1 = R.removeFromTop (26);
        rndSyncT_.setBounds (n1.removeFromLeft (72));
        n1.removeFromLeft (8);
        rndDivCombo_.setBounds (n1);
        auto n2 = R.removeFromTop (26);
        rndRateL_.setBounds (n2.removeFromLeft (72));
        n2.removeFromLeft (6);
        rndRate_.setBounds (n2);
        auto n3 = R.removeFromTop (26);
        placeLabeledSlider (n3, rndSlewL_, rndSlew_, 44);
    }
    else
    {
        zeroBounds (rndSection_);
        zeroBounds (rndSyncT_);
        zeroBounds (rndDivCombo_);
        zeroBounds (rndRateL_);
        zeroBounds (rndRate_);
        zeroBounds (rndSlewL_);
        zeroBounds (rndSlew_);
    }

    if (adsr)
    {
        adsrSection_.setBounds (R.removeFromTop (16));
        auto adRow = R.removeFromTop (30);
        const int aw = 22;
        const int sw = juce::jmax (48, (adRow.getWidth() - 4 * aw - 12) / 4);
        adA_.setBounds (adRow.removeFromLeft (aw));
        adsrA_.setBounds (adRow.removeFromLeft (sw));
        adD_.setBounds (adRow.removeFromLeft (aw));
        adsrD_.setBounds (adRow.removeFromLeft (sw));
        adS_.setBounds (adRow.removeFromLeft (aw));
        adsrS_.setBounds (adRow.removeFromLeft (sw));
        adR_.setBounds (adRow.removeFromLeft (aw));
        adsrR_.setBounds (adRow);
    }
    else
    {
        zeroBounds (adsrSection_);
        zeroBounds (adA_);
        zeroBounds (adsrA_);
        zeroBounds (adD_);
        zeroBounds (adsrD_);
        zeroBounds (adS_);
        zeroBounds (adsrS_);
        zeroBounds (adR_);
        zeroBounds (adsrR_);
    }
}
