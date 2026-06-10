#include "InstrumentPanel.h"
#include "EditorColours.h"
#include "../PageModel.h"
#include "../../core/FilterScales.h"

namespace
{
    using namespace sculpt_editor;

    // Fixed height reserved at the bottom of the LCD for the 2x4 VALUE readout grid.
    // Two rows, each with a name label and a numeric value line.
    constexpr int kValueGridH = 90;
    // Waveform height: fraction of what remains above the reserved value grid.
    constexpr float kWaveformFraction = 0.45f;

    void drawLoopRegion (juce::Graphics& g, juce::Rectangle<float> rect, float lo01, float hi01)
    {
        float lo = juce::jmin (lo01, hi01);
        float hi = juce::jmax (lo01, hi01);
        lo = juce::jlimit (0.0f, 1.0f, lo);
        hi = juce::jlimit (0.0f, 1.0f, hi);
        if (hi - lo < 0.002f)
            return;
        auto band = rect.withX (rect.getX() + lo * rect.getWidth())
                        .withWidth (juce::jmax (1.0f, (hi - lo) * rect.getWidth()));
        g.setColour (kWaveformLoopShade);
        g.fillRect (band);
    }

    void drawSymmetricEnvelope (juce::Graphics& g, juce::Rectangle<float> rect,
                                const std::array<float, sculpt::kMaterialWaveformBins>& peaks)
    {
        constexpr int n = sculpt::kMaterialWaveformBins;
        const float midY = rect.getCentreY();
        const float halfH = rect.getHeight() * 0.42f;

        juce::Path path;
        for (int i = 0; i < n; ++i)
        {
            const float x = rect.getX() + (static_cast<float> (i) + 0.5f) * rect.getWidth() / static_cast<float> (n);
            const float pk = juce::jlimit (0.0f, 1.0f, peaks[static_cast<size_t> (i)]);
            const float yTop = midY - pk * halfH;
            if (i == 0)
                path.startNewSubPath (x, yTop);
            else
                path.lineTo (x, yTop);
        }
        for (int i = n - 1; i >= 0; --i)
        {
            const float x = rect.getX() + (static_cast<float> (i) + 0.5f) * rect.getWidth() / static_cast<float> (n);
            const float pk = juce::jlimit (0.0f, 1.0f, peaks[static_cast<size_t> (i)]);
            const float yBot = midY + pk * halfH;
            path.lineTo (x, yBot);
        }
        path.closeSubPath();

        g.setColour (kWaveformFill.withAlpha (0.28f));
        g.fillPath (path);
        g.setColour (kWaveformStroke.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    void drawSecondaryStrip (juce::Graphics& g, juce::Rectangle<float> rect,
                             const std::array<float, sculpt::kMaterialWaveformBins>& peaks)
    {
        constexpr int n = sculpt::kMaterialWaveformBins;
        g.setColour (kLcdWaveformBg);
        g.fillRect (rect);

        const float h = rect.getHeight();
        const float baseY = rect.getBottom();
        for (int i = 0; i < n; ++i)
        {
            const float x0 = rect.getX() + static_cast<float> (i) * rect.getWidth() / static_cast<float> (n);
            const float x1 = rect.getX() + static_cast<float> (i + 1) * rect.getWidth() / static_cast<float> (n);
            const float pk = juce::jlimit (0.0f, 1.0f, peaks[static_cast<size_t> (i)]);
            const float barH = juce::jmax (1.0f, pk * h * 0.85f);
            juce::Rectangle<float> bar (x0 + 0.5f, baseY - barH, juce::jmax (1.0f, x1 - x0 - 1.0f), barH);
            g.setColour (kWaveformFill.withAlpha (0.45f + 0.35f * pk));
            g.fillRect (bar);
        }
    }

    void drawPlayhead (juce::Graphics& g, juce::Rectangle<float> rect, float pos01)
    {
        const float x = rect.getX() + juce::jlimit (0.0f, 1.0f, pos01) * rect.getWidth();
        g.setColour (kWaveformPlayhead);
        g.drawVerticalLine (juce::roundToInt (x), rect.getY(), rect.getBottom());
    }

    void drawOneTrackMeter (juce::Graphics& g, juce::Rectangle<int> row, const sculpt::ScreenModel& screen, int trackIndex)
    {
        const auto ts = static_cast<size_t> (trackIndex);
        auto r = row.reduced (0, 4);

        g.setColour (kText.withAlpha (0.75f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::String (trackIndex + 1), r.removeFromLeft (16), juce::Justification::centredLeft);

        auto barArea = r.removeFromLeft (juce::jmax (48, r.getWidth() - 56));
        g.setColour (kBackground);
        g.fillRect (barArea);
        g.setColour (screen.trackPlaying[ts] ? kMeter : kMeter.withAlpha (0.35f));
        g.fillRect (barArea.withWidth (juce::roundToInt (barArea.getWidth() * screen.trackMeter[ts])));

        g.setColour (kAccent.withAlpha (0.3f + 0.7f * screen.grainActivity[ts]));
        g.fillEllipse (r.removeFromLeft (18).reduced (3).toFloat());

        g.setColour (kText.withAlpha (0.45f));
        const float playheadX = static_cast<float> (barArea.getX())
                              + static_cast<float> (barArea.getWidth()) * screen.tapePosition[ts];
        g.drawVerticalLine (juce::roundToInt (playheadX), static_cast<float> (barArea.getY()),
                            static_cast<float> (barArea.getBottom()));
    }

    void drawMasterMeter (juce::Graphics& g, juce::Rectangle<int> row, const sculpt::ScreenModel& screen)
    {
        auto r = row.reduced (0, 4);
        g.setColour (kText.withAlpha (0.75f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("M", r.removeFromLeft (16), juce::Justification::centredLeft);
        auto masterBar = r;
        g.setColour (kBackground);
        g.fillRect (masterBar);
        g.setColour (kAccent);
        const int half = masterBar.getHeight() / 2;
        g.fillRect (masterBar.withHeight (half - 1)
                        .withWidth (juce::roundToInt (masterBar.getWidth() * screen.masterMeterL)));
        g.fillRect (masterBar.withTrimmedTop (half + 1)
                        .withWidth (juce::roundToInt (masterBar.getWidth() * screen.masterMeterR)));
    }

    void drawAllTrackMeters (juce::Graphics& g, juce::Rectangle<int> meterArea, const sculpt::ScreenModel& screen)
    {
        const int rowHeight = meterArea.getHeight() / (sculpt::kNumTracks + 1);
        for (int t = 0; t < sculpt::kNumTracks; ++t)
            drawOneTrackMeter (g, meterArea.removeFromTop (rowHeight), screen, t);
        drawMasterMeter (g, meterArea.removeFromTop (rowHeight), screen);
    }
    void drawFilterBands (juce::Graphics& g, juce::Rectangle<int> area,
                          const sculpt::ScreenModel& screen)
    {
        if (area.isEmpty())
            return;

        const int n = sculpt::ScreenModel::kFilterBands;
        const float barW = static_cast<float> (area.getWidth()) / static_cast<float> (n);
        const float maxH = static_cast<float> (area.getHeight());
        const float baseY = static_cast<float> (area.getBottom());

        // Background
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (area.toFloat(), 3.0f);

        for (int b = 0; b < n; ++b)
        {
            const float env  = juce::jlimit (0.0f, 1.0f, screen.filterBandGains[static_cast<size_t> (b)]);
            const float barH = juce::jmax (1.5f, env * maxH * 0.92f);
            const float x0   = static_cast<float> (area.getX()) + static_cast<float> (b) * barW;
            juce::Rectangle<float> bar (x0 + 0.5f, baseY - barH,
                                        juce::jmax (1.0f, barW - 1.0f), barH);
            // Colour intensity follows envelope level
            g.setColour (kAccent.withAlpha (0.3f + 0.7f * env));
            g.fillRoundedRectangle (bar, 1.0f);
        }

        // Idle state: show scale pitch markers when no audio
        if (! screen.filterSpectralMode)
        {
            g.setColour (kMeter.withAlpha (0.5f));
            g.drawText ("LPF / BP / HP", area.toFloat(), juce::Justification::centred, false);
        }
    }
} // namespace

void InstrumentPanel::setWaveformEnvelope (const float* data, int numBins)
{
    if (data == nullptr || numBins <= 0)
    {
        clearWaveformEnvelope();
        return;
    }

    const int nCopy = juce::jmin (numBins, sculpt::kMaterialWaveformBins);
    for (int i = 0; i < nCopy; ++i)
        waveformPeaks_[static_cast<size_t> (i)] = data[i];
    for (int i = nCopy; i < sculpt::kMaterialWaveformBins; ++i)
        waveformPeaks_[static_cast<size_t> (i)] = 0.0f;
}

void InstrumentPanel::clearWaveformEnvelope()
{
    waveformPeaks_.fill (0.0f);
}

void InstrumentPanel::paint (juce::Graphics& g)
{
    using namespace sculpt_editor;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (kLcdBezel);
    g.fillRoundedRectangle (bounds, 8.0f);

    if (! screenProvider_)
        return;

    const auto& screen = screenProvider_();
    auto inner = getLocalBounds().reduced (6).toFloat();

    g.setColour (kPanel);
    g.fillRoundedRectangle (inner, 5.0f);

    auto area = getLocalBounds().reduced (12);

    g.setColour (kText);
    g.setFont (juce::FontOptions (12.0f));

    const juce::String header = "TRK " + juce::String (screen.selectedTrack + 1)
                               + "   SCENE " + juce::String::charToString (juce::juce_wchar ('A' + screen.currentScene))
                               + "   " + juce::String (sculpt::PageModel::pageName (screen.selectedPage)).toUpperCase();
    g.drawText (header, area.removeFromTop (18), juce::Justification::centredLeft);

    const bool materialPage = (screen.selectedPage == sculpt::Page::Material);
    const bool filterPage   = (screen.selectedPage == sculpt::Page::Filter);

    if (materialPage)
    {
        // Reserve the value grid from the bottom first so it is always visible.
        const int valueH = juce::jmin (kValueGridH, area.getHeight());
        auto valueArea = area.removeFromBottom (valueH);

        // Draw the 2x4 VALUE readout cells in the reserved bottom strip.
        {
            const int cellW = valueArea.getWidth() / 4;
            const int cellH = valueArea.getHeight() / 2;
            for (int slot = 0; slot < 8; ++slot)
            {
                const int col = slot % 4;
                const int row = slot / 4;
                auto cell = juce::Rectangle<int> (valueArea.getX() + col * cellW,
                                                  valueArea.getY() + row * cellH,
                                                  cellW, cellH).reduced (3, 2);
                g.setColour (kBackground);
                g.fillRoundedRectangle (cell.toFloat(), 3.0f);

                if (slot < screen.numVisibleParams)
                {
                    const auto ps = static_cast<size_t> (slot);
                    const char* name = screen.paramNames[ps];
                    const float v = juce::jlimit (0.0f, 1.0f, screen.paramValues[ps]);
                    g.setColour (kText.withAlpha (0.85f));
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (name != nullptr ? name : "?",
                                cell.removeFromTop (cell.getHeight() / 2),
                                juce::Justification::centred, true);
                    g.setColour (kAccent);
                    g.setFont (juce::FontOptions (12.0f));
                    g.drawText (juce::String (v, 3), cell, juce::Justification::centred, true);
                }
                else
                {
                    g.setColour (kText.withAlpha (0.35f));
                    g.drawText ("-", cell, juce::Justification::centred, true);
                }
            }
        }

        // Split what remains above into: waveform, strip, meters.
        const int stripH = 24;
        const int rowH   = 28;
        const int meterH = 4 + rowH + rowH + 4; // gap + 2 rows + trailing gap
        const int topBudget = area.getHeight();

        // Waveform gets a fraction of what's left after strip and meters are reserved.
        const int fixedAbove = stripH + meterH;
        const int wfMax      = juce::jmax (0, topBudget - fixedAbove);
        const int wfH        = juce::jlimit (0, wfMax,
                                  juce::roundToInt (static_cast<float> (topBudget) * kWaveformFraction));

        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (wfArea, 3.0f);
        drawLoopRegion (g, wfArea, screen.materialLoopStart01, screen.materialLoopEnd01);
        drawSymmetricEnvelope (g, wfArea, waveformPeaks_);

        auto strip = area.removeFromTop (stripH).toFloat().reduced (1.0f, 1.0f);
        drawSecondaryStrip (g, strip, waveformPeaks_);

        const juce::Rectangle<float> playRect (wfArea.getX(), wfArea.getY(), wfArea.getWidth(),
                                               wfArea.getHeight() + strip.getHeight());
        const int st = juce::jlimit (0, sculpt::kNumTracks - 1, screen.selectedTrack);
        drawPlayhead (g, playRect, screen.tapePosition[static_cast<size_t> (st)]);

        area.removeFromTop (4);
        drawOneTrackMeter (g, area.removeFromTop (rowH), screen, st);
        drawMasterMeter (g, area.removeFromTop (rowH), screen);
    }
    else if (filterPage)
    {
        // Filter page: band spectrum display (top half) + value grid (bottom half).
        const int bandDisplayH = juce::jlimit (50, 160, juce::roundToInt (area.getHeight() * 0.45f));
        drawFilterBands (g, area.removeFromTop (bandDisplayH), screen);
        area.removeFromTop (6);
    }
    else
    {
        const int meterBlockH = juce::jmin (140, juce::jmax (80, area.getHeight() / 2));
        auto meterArea = area.removeFromTop (meterBlockH);
        drawAllTrackMeters (g, meterArea, screen);
        area.removeFromTop (6);
    }

    // 2x4 VALUE readouts for non-Material pages (Material draws its own grid above).
    if (! materialPage)
    {
        g.setFont (juce::FontOptions (11.0f));
        const int cellW = area.getWidth() / 4;
        const int cellH = juce::jmax (36, area.getHeight() / 2);

        for (int slot = 0; slot < 8; ++slot)
        {
            const int col = slot % 4;
            const int row = slot / 4;
            juce::Rectangle<int> cell (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH);
            cell = cell.reduced (3, 2);

            g.setColour (kBackground);
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);

            if (slot < screen.numVisibleParams)
            {
                const auto ps = static_cast<size_t> (slot);
                const char* name = screen.paramNames[ps];
                const float v = juce::jlimit (0.0f, 1.0f, screen.paramValues[ps]);
                g.setColour (kText.withAlpha (0.85f));
                g.setFont (juce::FontOptions (10.0f));
                g.drawText (name != nullptr ? name : "?", cell.removeFromTop (cell.getHeight() / 2),
                            juce::Justification::centred, true);
                g.setColour (kAccent);
                g.setFont (juce::FontOptions (12.0f));
                g.drawText (juce::String (v, 3), cell, juce::Justification::centred, true);
            }
            else
            {
                g.setColour (kText.withAlpha (0.35f));
                g.drawText ("-", cell, juce::Justification::centred, true);
            }
        }
    }
}
