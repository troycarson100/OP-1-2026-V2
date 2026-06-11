#include <cmath>
#include "InstrumentPanel.h"
#include "EditorColours.h"
#include "../PageModel.h"
#include "../../core/FilterScales.h"
#include "../../core/ParameterIds.h"

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

    void drawGrainOverlay (juce::Graphics& g, juce::Rectangle<float> wfArea,
                           const sculpt::ScreenModel& screen)
    {
        constexpr int nSlots = sculpt::kGrainsPerTrack;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < nSlots; ++i)
        {
            const auto& slot = screen.grainDisplay[static_cast<size_t> (i)];
            if (! slot.active)
                continue;

            const float wobble = 2.5f * std::sin (slot.phase01 * twoPi);
            const float xL = wfArea.getX() + slot.start01 * wfArea.getWidth() + wobble;
            const float spanW = juce::jmax (1.0f, slot.len01 * wfArea.getWidth());
            const float xR = xL + spanW;

            const float alpha = 0.2f + 0.55f * juce::jlimit (0.0f, 1.0f, slot.phase01);
            const float hue = static_cast<float> (i % 6) / 6.0f;
            const juce::Colour col = juce::Colour::fromHSV (hue, 0.55f, 0.92f, alpha);

            const float thick = 1.0f + 1.4f * juce::jlimit (0.0f, 1.0f, slot.phase01);
            g.setColour (col);

            const float y0 = wfArea.getY();
            const float y1 = wfArea.getBottom();
            g.drawLine (juce::jlimit (wfArea.getX(), wfArea.getRight(), xL), y0,
                        juce::jlimit (wfArea.getX(), wfArea.getRight(), xL), y1, thick);
            g.drawLine (juce::jlimit (wfArea.getX(), wfArea.getRight(), xR), y0,
                        juce::jlimit (wfArea.getX(), wfArea.getRight(), xR), y1, thick);
        }
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
    juce::String formatParamCellValue (sculpt::ParameterId id, float v)
    {
        using P = sculpt::ParameterId;
        switch (id)
        {
            case P::FilterScale:
                return sculpt::filterScaleName (sculpt::normalizedToFilterScale (v));
            case P::FilterKey:
                return sculpt::filterKeyName (sculpt::normalizedToKeyIndex (v));
            case P::FilterMode:
                return (v > 0.5f) ? "Ring" : "LP/BP/HP";
            default:
                return juce::String (v, 3);
        }
    }

    void drawValueGrid (juce::Graphics& g, juce::Rectangle<int> area,
                        const sculpt::ScreenModel& screen)
    {
        const int cellW = area.getWidth() / 4;
        const int cellH = area.getHeight() / 2;
        constexpr float kModDead = 1.0e-5f;

        for (int slot = 0; slot < 8; ++slot)
        {
            const int col = slot % 4;
            const int row = slot / 4;
            const juce::Rectangle<int> fullCell = juce::Rectangle<int> (area.getX() + col * cellW,
                                                                         area.getY() + row * cellH,
                                                                         cellW, cellH)
                                                    .reduced (3, 2);

            g.setColour (kBackground);
            g.fillRoundedRectangle (fullCell.toFloat(), 3.0f);

            if (slot >= screen.numVisibleParams)
            {
                g.setColour (kText.withAlpha (0.35f));
                g.drawText ("-", fullCell, juce::Justification::centred, true);
                continue;
            }

            const auto   ps   = static_cast<size_t> (slot);
            const char*  name = screen.paramNames[ps];
            const float  v    = juce::jlimit (0.0f, 1.0f, screen.paramValues[ps]);
            const float  mod  = screen.paramModOffset[ps];
            const bool   showMod = std::fabs (mod) > kModDead;

            juce::Rectangle<int> labelArea = fullCell.withHeight (fullCell.getHeight() / 2);
            juce::Rectangle<int> valueArea = fullCell.withTrimmedTop (fullCell.getHeight() / 2);
            if (showMod)
                valueArea = valueArea.withTrimmedBottom (8);

            g.setColour (kText.withAlpha (0.85f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (name != nullptr ? name : "?", labelArea, juce::Justification::centred, true);
            g.setColour (kAccent);
            g.setFont (juce::FontOptions (12.0f));
            const auto pid = screen.paramIds[ps];
            g.drawText (formatParamCellValue (pid, v), valueArea, juce::Justification::centred, true);

            if (showMod)
            {
                const float dotR = 2.5f;
                const float dotX = static_cast<float> (fullCell.getRight()) - 6.0f - dotR;
                const float dotY = static_cast<float> (fullCell.getY()) + 6.0f;
                g.setColour (kParamModDot);
                g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

                const int barH = 3;
                const int padX = 5;
                auto      bar    = fullCell.withTrimmedTop (fullCell.getHeight() - barH - 4)
                                  .withTrimmedBottom (1)
                                  .withTrimmedLeft (padX)
                                  .withTrimmedRight (padX);
                g.setColour (kParamModBarBg);
                g.fillRoundedRectangle (bar.toFloat(), 1.0f);

                constexpr float thumbW = 5.0f;
                // 1:1 with summed bipolar-style offset (same scale as engine mod delta). Hard gain
                // (e.g. *6) pegged the thumb at the bar ends most of a sine cycle — avoid that here.
                const float shaped = juce::jlimit (-1.0f, 1.0f, mod);
                const float pos01  = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * shaped);
                const float     barF   = static_cast<float> (bar.getX());
                const float     avail  = juce::jmax (0.0f, static_cast<float> (bar.getWidth()) - thumbW);
                const float     thumbX = barF + pos01 * avail;

                g.setColour (kParamModThumb);
                g.fillRect (juce::Rectangle<float> (thumbX,
                                                     static_cast<float> (bar.getY()) + 0.5f,
                                                     thumbW,
                                                     static_cast<float> (juce::jmax (1, bar.getHeight() - 1))));
            }
        }
    }

    void drawFilterBands (juce::Graphics& g, juce::Rectangle<int> area,
                          const sculpt::ScreenModel& screen)
    {
        if (area.isEmpty())
            return;

        // Background
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (area.toFloat(), 3.0f);

        if (! screen.filterSpectralMode)
        {
            // LPF / BP / HP mode — show a static label
            g.setColour (kText.withAlpha (0.45f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("LP / BP / HP", area.toFloat(), juce::Justification::centred, false);
            return;
        }

        const int   n     = sculpt::ScreenModel::kFilterBands;
        const float barW  = static_cast<float> (area.getWidth()) / static_cast<float> (n);
        const float maxH  = static_cast<float> (area.getHeight());
        const float baseY = static_cast<float> (area.getBottom());
        const float tickH = juce::jmax (2.0f, maxH * 0.04f);

        for (int b = 0; b < n; ++b)
        {
            const float x0 = static_cast<float> (area.getX()) + static_cast<float> (b) * barW;
            const float bw = juce::jmax (1.0f, barW - 1.0f);

            // Idle tuning grid: faint tick at each band position
            g.setColour (kText.withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (x0 + 0.5f, baseY - tickH, bw, tickH));

            // Active bar: getBandEnvelope already returns 0..1 normalized
            const float env  = juce::jlimit (0.0f, 1.0f,
                                             screen.filterBandGains[static_cast<size_t> (b)]);
            if (env < 0.01f)
                continue;

            const float barH = env * maxH * 0.94f;
            juce::Rectangle<float> bar (x0 + 0.5f, baseY - barH, bw, barH);

            // Gradient-like colour: brighter and more accent-coloured at the top
            g.setColour (kAccent.withAlpha (0.25f + 0.75f * env));
            g.fillRoundedRectangle (bar, 1.0f);
        }
    }

    void drawSymmetricPeaksStyled (juce::Graphics& g, juce::Rectangle<float> rect,
                                   const std::array<float, sculpt::kMaterialWaveformBins>& peaks,
                                   juce::Colour fill, juce::Colour stroke, float strokeW)
    {
        constexpr int n = sculpt::kMaterialWaveformBins;
        const float   midY = rect.getCentreY();
        const float   halfH = rect.getHeight() * 0.42f;

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

        g.setColour (fill);
        g.fillPath (path);
        g.setColour (stroke);
        g.strokePath (path, juce::PathStrokeType (strokeW));
    }

    void drawModOscilloscope (juce::Graphics& g, juce::Rectangle<float> wfArea,
                              const sculpt::ModLcdSnapshot& m)
    {
        if (! m.active)
        {
            g.setColour (kText.withAlpha (0.42f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("Select a mod source (Wave, Random, ADSR, Input env).",
                        wfArea.reduced (6.0f, 2.0f), juce::Justification::centredLeft, true);
            return;
        }

        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (wfArea, 3.0f);

        drawSymmetricPeaksStyled (g, wfArea, m.carrier01,
                                  kModLcdCarrierFill.withAlpha (0.24f),
                                  kModLcdCarrierStroke.withAlpha (0.5f), 1.0f);
        drawSymmetricPeaksStyled (g, wfArea, m.effective01,
                                  kModLcdModFill,
                                  kModLcdModStroke.withAlpha (0.95f), 1.35f);

        const float scanX = wfArea.getX() + juce::jlimit (0.0f, 1.0f, m.scannerPhase01) * wfArea.getWidth();
        g.setColour (kModLcdScanner.withAlpha (0.88f));
        g.drawVerticalLine (juce::roundToInt (scanX), wfArea.getY(), wfArea.getBottom());

        const float midY = wfArea.getCentreY();
        const float halfH = wfArea.getHeight() * 0.42f;
        const float vy = juce::jlimit (wfArea.getY() + 3.0f, wfArea.getBottom() - 3.0f,
                                       midY - juce::jlimit (-1.0f, 1.0f, m.valueBipolar) * halfH);
        g.setColour (kModLcdDot.withAlpha (0.95f));
        g.fillEllipse (scanX - 4.5f, vy - 4.5f, 9.0f, 9.0f);
        g.setColour (kModLcdDot.brighter (0.2f));
        g.drawEllipse (scanX - 4.5f, vy - 4.5f, 9.0f, 9.0f, 1.1f);
    }

    // Mixer page: 3-band EQ as vertical meters + gain-reduction strip (same palette as waveform / filter).
    void drawMixEqCompressorVisual (juce::Graphics& g, juce::Rectangle<float> area,
                                    const std::array<float, sculpt::ScreenModel::kMixEqBands>& bandNorm,
                                    float compReduction01)
    {
        if (area.isEmpty())
            return;

        compReduction01 = juce::jlimit (0.0f, 1.0f, compReduction01);

        auto eqZone  = area.removeFromLeft (area.getWidth() * 0.70f);
        const float gap = 5.0f;
        const float cellW = (eqZone.getWidth() - gap * 2.0f) / 3.0f;
        const char* labels[] = { "BASS", "MID", "TREBLE" };
        const juce::Colour bandCols[] = {
            kWaveformFill.withAlpha (0.85f),
            juce::Colour (0xff8ab4e8).withAlpha (0.88f),
            kAccent.withAlpha (0.82f)
        };

        const float zx = eqZone.getX();
        const float zy = eqZone.getY();
        const float zh = eqZone.getHeight();

        for (int b = 0; b < 3; ++b)
        {
            juce::Rectangle<float> cell (zx + static_cast<float> (b) * (cellW + gap), zy, cellW, zh);

            g.setColour (kBackground);
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (kText.withAlpha (0.22f));
            g.drawRoundedRectangle (cell, 3.0f, 1.0f);

            auto meterCell = cell.reduced (2.0f, 2.0f).withTrimmedBottom (12.0f);
            const float cy   = meterCell.getCentreY();
            const float half = meterCell.getHeight() * 0.38f;
            const float t    = juce::jlimit (0.0f, 1.0f, bandNorm[static_cast<size_t> (b)]);
            const float signedExtent = (t - 0.5f) * 2.0f;

            g.setColour (kText.withAlpha (0.35f));
            g.drawHorizontalLine (juce::roundToInt (cy), meterCell.getX() + 1.0f, meterCell.getRight() - 1.0f);

            if (std::fabs (signedExtent) > 0.02f)
            {
                juce::Rectangle<float> fill;
                if (signedExtent > 0.0f)
                    fill = { meterCell.getX() + 2.0f, cy - signedExtent * half,
                             meterCell.getWidth() - 4.0f, signedExtent * half };
                else
                    fill = { meterCell.getX() + 2.0f, cy, meterCell.getWidth() - 4.0f, -signedExtent * half };

                g.setColour (bandCols[static_cast<size_t> (b)].withAlpha (0.35f + 0.45f * std::fabs (signedExtent)));
                g.fillRoundedRectangle (fill, 2.0f);
            }

            g.setColour (kText.withAlpha (0.55f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (labels[b], cell.removeFromBottom (11.0f), juce::Justification::centred, false);
        }

        area.removeFromLeft (6.0f);
        g.setColour (kText.withAlpha (0.5f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("COMP GR", area.removeFromTop (11.0f), juce::Justification::centredLeft, false);

        auto grBar = area.reduced (0.0f, 2.0f);
        g.setColour (kBackground);
        g.fillRoundedRectangle (grBar, 3.0f);
        g.setColour (kText.withAlpha (0.2f));
        g.drawRoundedRectangle (grBar, 3.0f, 1.0f);

        auto grFill = grBar.reduced (2.0f, 3.0f);
        grFill.setWidth (juce::jmax (2.0f, grFill.getWidth() * compReduction01));
        g.setColour (kAccent.withAlpha (0.25f + 0.65f * compReduction01));
        g.fillRoundedRectangle (grFill, 2.0f);
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

juce::Rectangle<int> InstrumentPanel::bpmInteractionBounds() const
{
    auto area = getLocalBounds().reduced (12);
    auto headerStrip = area.removeFromTop (18);
    constexpr int bpmW = 88;
    headerStrip.removeFromLeft (juce::jmax (0, headerStrip.getWidth() - bpmW));
    return headerStrip;
}

void InstrumentPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! bpmDragHandler_ || ! screenProvider_)
        return;

    if (bpmInteractionBounds().contains (e.getPosition()))
    {
        bpmDragging_     = true;
        bpmDragStartX_   = static_cast<float> (e.position.x);
        bpmAtDragStart_  = static_cast<double> (screenProvider_().displayBpm);
    }
}

void InstrumentPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! bpmDragging_ || ! bpmDragHandler_)
        return;

    const float dx = static_cast<float> (e.position.x) - bpmDragStartX_;
    const double sensitivity = e.mods.isShiftDown() ? 0.08 : 0.35;
    bpmDragHandler_ (bpmAtDragStart_ + static_cast<double> (dx) * sensitivity);
}

void InstrumentPanel::mouseUp (const juce::MouseEvent&)
{
    bpmDragging_ = false;
}

void InstrumentPanel::mouseMove (const juce::MouseEvent& e)
{
    if (bpmInteractionBounds().contains (e.getPosition()))
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::ParentCursor);
}

void InstrumentPanel::mouseExit (const juce::MouseEvent&)
{
    if (! bpmDragging_)
        setMouseCursor (juce::MouseCursor::ParentCursor);
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

    auto headerStrip = area.removeFromTop (18);
    const int bpmW = 88;
    auto leftHdr = headerStrip.removeFromLeft (juce::jmax (0, headerStrip.getWidth() - bpmW));
    const juce::String header = "TRK " + juce::String (screen.selectedTrack + 1)
                               + "   SCENE " + juce::String::charToString (juce::juce_wchar ('A' + screen.currentScene))
                               + "   " + juce::String (sculpt::PageModel::pageName (uiPage_)).toUpperCase();
    g.drawText (header, leftHdr, juce::Justification::centredLeft);
    g.setColour (screen.bpmValid ? kAccent : kText.withAlpha (0.55f));
    const juce::String bpmTxt = juce::String (screen.displayBpm, 1) + " BPM";
    g.drawText (bpmTxt, headerStrip, juce::Justification::centredRight);
    g.setColour (kText);

    const bool materialPage = (uiPage_ == sculpt::Page::Material);
    const bool granularPage = (uiPage_ == sculpt::Page::Granular);
    const bool filterPage   = (uiPage_ == sculpt::Page::Filter);
    const bool modPage      = (uiPage_ == sculpt::Page::Mod);
    // Prefer editor page; also treat engine-reported page as Mixer so the LCD never falls through
    // to the 4-track meter stack if message-thread vs audio-thread page state tears briefly.
    const bool mixerLcd     = (uiPage_ == sculpt::Page::Mixer)
                          || (screen.selectedPage == sculpt::Page::Mixer);
    const bool waveformPage = materialPage || granularPage;

    // Reserve the fixed value grid strip at the bottom on every page.
    const int valueH   = juce::jmin (kValueGridH, area.getHeight());
    auto valueArea     = area.removeFromBottom (valueH);

    if (waveformPage)
    {
        // Split what remains above into: waveform, strip, meters.
        const int stripH    = 24;
        const int rowH      = 28;
        const int meterH    = 4 + rowH + rowH + 4;
        const int topBudget = area.getHeight();

        const int fixedAbove = stripH + meterH;
        const int wfMax      = juce::jmax (0, topBudget - fixedAbove);
        const int wfH        = juce::jlimit (0, wfMax,
                                  juce::roundToInt (static_cast<float> (topBudget) * kWaveformFraction));

        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (wfArea, 3.0f);
        drawLoopRegion (g, wfArea, screen.materialLoopStart01, screen.materialLoopEnd01);
        drawSymmetricEnvelope (g, wfArea, waveformPeaks_);
        if (granularPage)
            drawGrainOverlay (g, wfArea, screen);

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
    else if (mixerLcd)
    {
        // Mix bus waveform + EQ/comp — no 4-track meter stack; no secondary strip (frees height for waveform).
        constexpr int mixStripH   = 48;
        const int     padBottom = 8;
        const int     topBudget = area.getHeight();

        const int reserved = mixStripH + padBottom;
        const int wfMax    = juce::jmax (0, topBudget - reserved);
        constexpr int wfMin = 56;
        const int wfWant   = juce::roundToInt (static_cast<float> (topBudget) * 0.72f);
        const int wfH      = juce::jlimit (juce::jmin (wfMin, wfMax), wfMax, wfWant);

        const int st = juce::jlimit (0, sculpt::kNumTracks - 1, screen.selectedTrack);

        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (wfArea, 3.0f);
        drawSymmetricPeaksStyled (g, wfArea, screen.mixBusWaveform[static_cast<size_t> (st)],
                                  kWaveformFill.withAlpha (0.26f),
                                  kWaveformStroke.withAlpha (0.92f), 1.2f);

        drawPlayhead (g, wfArea, screen.tapePosition[static_cast<size_t> (st)]);

        auto mixStrip = area.removeFromTop (mixStripH).toFloat().reduced (1.0f, 1.0f);
        drawMixEqCompressorVisual (g, mixStrip, screen.mixEqBandNorm[static_cast<size_t> (st)],
                                   screen.mixCompReduction[static_cast<size_t> (st)]);

        area.removeFromTop (padBottom);
    }
    else if (modPage)
    {
        const int stripH = 0;
        const int rowH   = 28;
        const int topBudget = area.getHeight();
        const int fixedBelow = rowH + rowH + 4;
        const int wfMax      = juce::jmax (0, topBudget - fixedBelow);
        const int wfH        = juce::jlimit (0, wfMax,
                                  juce::roundToInt (static_cast<float> (topBudget) * kWaveformFraction));

        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        drawModOscilloscope (g, wfArea, screen.modLcd);

        if (stripH > 0)
            area.removeFromTop (stripH);
        area.removeFromTop (4);
        const int st = juce::jlimit (0, sculpt::kNumTracks - 1, screen.selectedTrack);
        drawOneTrackMeter (g, area.removeFromTop (rowH), screen, st);
        drawMasterMeter (g, area.removeFromTop (rowH), screen);
    }
    else if (filterPage)
    {
        const int bandDisplayH = juce::jlimit (40, area.getHeight(), area.getHeight() - 4);
        drawFilterBands (g, area.removeFromTop (bandDisplayH), screen);
    }
    else
    {
        const int meterBlockH = juce::jmin (140, juce::jmax (80, area.getHeight()));
        drawAllTrackMeters (g, area.removeFromTop (meterBlockH), screen);
    }

    // 2x4 VALUE readout grid — same fixed size and position on every page.
    drawValueGrid (g, valueArea, screen);
}
