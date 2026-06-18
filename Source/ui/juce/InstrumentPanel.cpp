#include <cmath>
#include "InstrumentPanel.h"
#include "EditorColours.h"
#include "SpaceVisual.h"
#include "../PageModel.h"
#include "../SpaceTimeFormat.h"
#include "../../core/FilterScales.h"
#include "../../core/ParameterIds.h"
#include "../../modulation/SyncDivision.h"
#include "../../util/GrainPatternBank.h"
#include "../../util/GrainPitchScales.h"

namespace
{
    using namespace sculpt_editor;

    // Fixed minimum for the bottom parameter readout; grows when >8 params (e.g. Space).
    constexpr int kValueGridMinH = 168;

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

    void drawMaterialLoopShade (juce::Graphics& g, juce::Rectangle<float> rect,
                                float lo01, float hi01, float view0, float view1)
    {
        float lo = juce::jmin (lo01, hi01);
        float hi = juce::jmax (lo01, hi01);
        lo = juce::jlimit (0.0f, 1.0f, lo);
        hi = juce::jlimit (0.0f, 1.0f, hi);
        if (hi - lo < 0.002f)
            return;
        const float span = juce::jmax (1.0e-5f, view1 - view0);
        const float x0   = rect.getX() + (lo - view0) / span * rect.getWidth();
        const float x1   = rect.getX() + (hi - view0) / span * rect.getWidth();
        auto        band = juce::Rectangle<float> (x0, rect.getY(), juce::jmax (1.0f, x1 - x0), rect.getHeight());
        band = band.getIntersection (rect);
        if (band.isEmpty())
            return;
        g.setColour (kWaveformLoopShade);
        g.fillRect (band);
    }

    juce::String formatMaterialTimeLabel (float seconds)
    {
        if (seconds < 0.0f)
            seconds = 0.0f;
        if (seconds < 60.0f)
            return juce::String (seconds, 2) + "s";
        const int   m = static_cast<int> (seconds / 60.0f);
        const float s = seconds - static_cast<float> (m) * 60.0f;
        return juce::String (m) + ":" + juce::String (s, 1);
    }

    // Tempo grid: lines at each 1/N note (materialGridStepSec) across the visible window, so the
    // loop-snap points are visible; label the times when the divisions are sparse enough to read.
    void drawMaterialTimeGrid (juce::Graphics& g, juce::Rectangle<float> rect,
                               const sculpt::ScreenModel& screen)
    {
        const float dur = screen.materialDurationSec;
        const float v0  = screen.materialViewStart01;
        const float v1  = screen.materialViewEnd01;
        if (dur < 1.0e-4f || v1 <= v0 + 1.0e-5f)
            return;

        const float t0   = dur * v0;
        const float t1   = dur * v1;
        const float span = juce::jmax (1.0e-4f, t1 - t0);
        const float step = screen.materialGridStepSec; // seconds per 1/N note
        if (step < 1.0e-5f || span / step > 512.0f)    // guard against absurdly dense grids
            return;

        constexpr float labelBandH = 11.0f;
        auto            plot       = rect.withTrimmedBottom (labelBandH);

        g.setColour (kText.withAlpha (0.22f));
        for (float t = std::ceil (t0 / step) * step; t <= t1 + 1.0e-4f; t += step)
        {
            const float nx = (t - t0) / span;
            const float x  = rect.getX() + nx * rect.getWidth();
            if (x < rect.getX() - 1.0f || x > rect.getRight() + 1.0f)
                continue;
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        }

        if (span / step <= 8.5f)
        {
            g.setColour (kText.withAlpha (0.48f));
            g.setFont (juce::FontOptions (8.0f));
            for (float t = std::ceil (t0 / step) * step; t <= t1 + 1.0e-4f; t += step)
            {
                const float nx = (t - t0) / span;
                const float x  = rect.getX() + nx * rect.getWidth();
                if (x < rect.getX() - 2.0f || x > rect.getRight() - 2.0f)
                    continue;
                const juce::String txt = formatMaterialTimeLabel (t);
                g.drawText (txt, juce::Rectangle<float> (x - 14.0f, plot.getBottom() - 1.0f, 28.0f, labelBandH),
                            juce::Justification::centred, true);
            }
        }
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

    // Loop crossfade overlay: a gain envelope across the loop region (rises over the attack at
    // the loop start, falls over the release at the loop end) so the blend regions are visible.
    void drawLoopFades (juce::Graphics& g, juce::Rectangle<float> wfArea,
                        const sculpt::ScreenModel& screen)
    {
        const float fi = screen.materialLoopFadeIn01;
        const float fo = screen.materialLoopFadeOut01;
        if (fi <= 0.0f && fo <= 0.0f)
            return;

        const float v0   = screen.materialViewStart01;
        const float v1   = screen.materialViewEnd01;
        const float span = std::max (1.0e-5f, v1 - v0);
        auto xFor = [&] (float b) {
            return wfArea.getX() + (b - v0) / span * wfArea.getWidth();
        };

        const float ls  = screen.materialLoopStart01;
        const float le  = screen.materialLoopEnd01;
        const float top = wfArea.getY() + 2.0f;
        const float bot = wfArea.getBottom() - 2.0f;

        const float xLs  = xFor (ls);
        const float xAtk = xFor (ls + fi);
        const float xRel = xFor (le - fo);
        const float xLe  = xFor (le);

        juce::Path env;
        env.startNewSubPath (xLs, bot);
        env.lineTo (xAtk, top);
        env.lineTo (xRel, top);
        env.lineTo (xLe, bot);

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (wfArea.toNearestInt());

        juce::Path fill = env;
        fill.closeSubPath();
        g.setColour (kAccent.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (kAccent.withAlpha (0.7f));
        g.strokePath (env, juce::PathStrokeType (1.4f));
    }

    void drawGrainOverlay (juce::Graphics& g, juce::Rectangle<float> wfArea,
                           const sculpt::ScreenModel& screen)
    {
        constexpr int nSlots = sculpt::kGrainsPerTrack;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        // Instant knob-aligned window (snappy); active grains add motion/blur underneath.
        if (screen.grainFocusLen01 > 1.0e-5f)
        {
            const float x0 = wfArea.getX() + screen.grainFocusStart01 * wfArea.getWidth();
            const float w  = juce::jmax (1.0f, screen.grainFocusLen01 * wfArea.getWidth());
            auto focusBand = juce::Rectangle<float> (x0, wfArea.getY(), w, wfArea.getHeight())
                                 .getIntersection (wfArea);
            if (! focusBand.isEmpty())
            {
                g.setColour (kWaveformFill.withAlpha (0.26f));
                g.fillRect (focusBand);
                g.setColour (kWaveformStroke.withAlpha (0.62f));
                g.drawRect (focusBand, 1.0f);
            }
        }

        for (int i = 0; i < nSlots; ++i)
        {
            const auto& slot = screen.grainDisplay[static_cast<size_t> (i)];
            if (! slot.active)
                continue;

            const float wobble = 2.5f * std::sin (slot.phase01 * twoPi);
            const float xL = wfArea.getX() + slot.start01 * wfArea.getWidth() + wobble;
            const float spanW = juce::jmax (1.0f, slot.len01 * wfArea.getWidth());
            const float xR = xL + spanW;

            // Emphasize young grains; fade tails so old windows don't read as "lagging" the knob.
            const float life = std::pow (1.0f - slot.phase01, 1.25f);
            const float alpha = (0.24f + 0.52f * life) * (0.55f + 0.45f * life);
            const float hue = static_cast<float> (i % 6) / 6.0f;
            const juce::Colour col = juce::Colour::fromHSV (hue, 0.55f, 0.92f, juce::jlimit (0.08f, 0.95f, alpha));

            const float y0 = wfArea.getY();
            const float y1 = wfArea.getBottom();
            const float xl = juce::jlimit (wfArea.getX(), wfArea.getRight(), xL);
            const float xr = juce::jlimit (wfArea.getX(), wfArea.getRight(), xR);
            const float bandL = juce::jmin (xl, xr);
            const float bandR = juce::jmax (xl, xr);
            auto band = juce::Rectangle<float> (bandL, y0, juce::jmax (1.0f, bandR - bandL), y1 - y0)
                            .getIntersection (wfArea);
            if (! band.isEmpty())
            {
                g.setColour (juce::Colour::fromHSV (hue, 0.38f, 0.9f, alpha * 0.34f));
                g.fillRect (band);
            }

            const float thick = 1.0f + 1.15f * life;
            g.setColour (col);
            g.drawLine (xl, y0, xl, y1, thick);
            g.drawLine (xr, y0, xr, y1, thick);
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

    // Vertical output meters in a fixed right-side column (identical position on every page):
    // selected track level + master L / R, filling from the bottom up.
    void drawVerticalMeters (juce::Graphics& g, juce::Rectangle<int> col, const sculpt::ScreenModel& screen)
    {
        const int  st = juce::jlimit (0, sculpt::kNumTracks - 1, screen.selectedTrack);
        const auto ts = static_cast<size_t> (st);

        auto inner    = col.reduced (3, 2);
        const int labelH = 12;
        auto labelRow = inner.removeFromBottom (labelH);
        inner.removeFromBottom (2);

        struct Bar { float level; juce::Colour colour; const char* label; };
        const Bar bars[3] = {
            { screen.trackMeter[ts], screen.trackPlaying[ts] ? kMeter : kMeter.withAlpha (0.4f), "T" },
            { screen.masterMeterL, kAccent, "L" },
            { screen.masterMeterR, kAccent, "R" },
        };
        constexpr int n   = 3;
        constexpr int gap = 4;
        const int barW = juce::jmax (3, (inner.getWidth() - gap * (n - 1)) / n);

        g.setFont (juce::FontOptions (10.0f));
        int x = inner.getX();
        for (int i = 0; i < n; ++i)
        {
            juce::Rectangle<int> barRect (x, inner.getY(), barW, inner.getHeight());
            g.setColour (kBackground);
            g.fillRect (barRect);
            const float lvl = juce::jlimit (0.0f, 1.0f, bars[i].level);
            const int   h   = juce::roundToInt (static_cast<float> (barRect.getHeight()) * lvl);
            g.setColour (bars[i].colour);
            g.fillRect (barRect.withTop (barRect.getBottom() - h));
            g.setColour (kText.withAlpha (0.6f));
            g.drawText (bars[i].label, juce::Rectangle<int> (x, labelRow.getY(), barW, labelH),
                        juce::Justification::centred);
            x += barW + gap;
        }
    }

    float spaceDelayTimeMode01FromScreen (const sculpt::ScreenModel& screen)
    {
        for (int i = 0; i < screen.numVisibleParams; ++i)
            if (screen.paramIds[static_cast<size_t> (i)] == sculpt::ParameterId::SpaceDelayTimeMode)
                return screen.paramValues[static_cast<size_t> (i)];
        return 1.0f;
    }

    juce::String formatParamCellValue (sculpt::ParameterId id, float v,
                                      const sculpt::ScreenModel& screen)
    {
        using P = sculpt::ParameterId;
        switch (id)
        {
            case P::FilterScale:
            case P::MaterialPitchScale:
                return sculpt::filterScaleName (sculpt::normalizedToFilterScale (v));
            case P::FilterKey:
            case P::MaterialPitchKey:
                return sculpt::filterKeyName (sculpt::normalizedToKeyIndex (v));
            case P::MaterialLoopXfade:
                return juce::String (juce::roundToInt (sculpt::map::loopFadeSeconds (v) * 1000.0f)) + " ms";
            case P::MaterialGridDivision:
                return "1/" + juce::String (sculpt::map::materialGridDivisions (v));
            case P::FilterMode:
                return (v > 0.5f) ? "Ring" : "LP/BP/HP";
            case P::TapeSpeed:
            {
                // Pitch: semitone transpose, snapped to the Material scale/key when set.
                const auto pitchScale =
                    sculpt::normalizedToFilterScale (screen.selectedTrackMaterialPitchScale01);
                const int  pitchKey =
                    sculpt::normalizedToKeyIndex (screen.selectedTrackMaterialPitchKey01);
                const int  semis =
                    juce::roundToInt (sculpt::materialPitchSemitones (v, pitchScale, pitchKey));
                return juce::String::formatted ("%+d st", semis);
            }
            case P::MaterialTimeMode:
                return (v > 0.5f) ? "Warp" : "Tape";
            case P::SampleRootBpm:
                return juce::String (juce::roundToInt (sculpt::map::sampleRootBpm (v)));
            case P::GrainSync:
                return (v > 0.5f) ? "Sync" : "Free";
            case P::GrainDensity:
            {
                if (screen.granularPattern.syncOn)
                    return "d" + juce::String (screen.granularPattern.divisionIndex);
                return juce::String (sculpt::map::grainDensityHz (v), 1) + " Hz";
            }
            case P::GrainSteps:
                return juce::String (sculpt::map::grainSteps (v));
            case P::GrainPulses:
                return juce::String (sculpt::map::grainPulses (v, screen.granularPattern.steps));
            case P::GrainRotate:
                return juce::String (sculpt::map::grainRotate (v, screen.granularPattern.steps));
            case P::GrainPitchQuant:
            {
                const int q = sculpt::grainPitchQuantScaleIndex (v);
                switch (q)
                {
                    case 0: return "Off";
                    case 1: return "5th/Oct";
                    case 2: return "MajPent";
                    case 3: return "MinPent";
                    case 4: return "Major";
                    case 5: return "MinNat";
                    case 6: return "Dorian";
                    case 7: return "Chrm";
                    default: return "?";
                }
            }
            case P::GrainContour:
            {
                if (v < 0.2f) return "Hann";
                if (v < 0.5f) return "Warm";
                if (v < 0.8f) return "Perc";
                return "Sharp";
            }
            case P::GrainRandRev:
                return juce::String (juce::roundToInt (v * 100.0f)) + "% Rev";
            case P::GrainRandAmp:
                return juce::String (juce::roundToInt (v * 100.0f)) + "% Amp";
            case P::GrainPattern:
            {
                const int i = sculpt::map::grainPatternIndex (v);
                return sculpt::kGrainPatternUiNames[static_cast<size_t> (i)];
            }
            case P::GrainPatternAmount:
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            case P::SpaceDelayTime:
            {
                const int modeIdx = sculpt::map::spaceDelayTimeModeIndex (spaceDelayTimeMode01FromScreen (screen));
                return juce::String (sculpt::formatSpaceDelayTime (v, modeIdx));
            }
            case P::SpaceDelayTimeMode:
            {
                const int m = sculpt::map::spaceDelayTimeModeIndex (v);
                const char* names[] = { "Straight", "Dotted", "Triplet", "Free" };
                return juce::String (names[static_cast<size_t> (m)]);
            }
            case P::SpaceFreeze:
                return (v > 0.5f) ? "ON" : "OFF";
            case P::LoopSnapGrid:
                return (v > 0.5f) ? "ON" : "OFF";
            case P::SpaceReverbDecay:
                return juce::String (sculpt::map::spaceReverbRt60Seconds (v), 1) + "s";
            case P::SpaceDamp:
                return juce::String (juce::roundToInt (sculpt::map::spaceDampCutoffHz (v))) + " Hz";
            case P::SpaceDelayFeedback:
                return juce::String (juce::roundToInt (sculpt::map::spaceDelayFeedbackGain (v) * 100.0f)) + "%";
            case P::SpaceDelayAmount:
            case P::SpaceReverbAmount:
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            case P::SpaceReverbSize:
                return juce::String (juce::roundToInt (sculpt::map::spaceReverbSizeLineScale (v) * 100.0f)) + "%";
            case P::SpaceSpread:
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            default:
                return juce::String (v, 3);
        }
    }

    void drawValueGrid (juce::Graphics& g, juce::Rectangle<int> area,
                        const sculpt::ScreenModel& screen)
    {
        constexpr int kCols = 4;
        const int     nParams = screen.numVisibleParams;
        const int kLcdParamSlots =
            juce::jmin (16, juce::jmax (8, ((nParams + kCols - 1) / kCols) * kCols));
        const int     rows  = (kLcdParamSlots + kCols - 1) / kCols;
        const int     cellW = area.getWidth() / kCols;
        const int     cellH = rows > 0 ? area.getHeight() / rows : area.getHeight();
        constexpr float kModDead = 1.0e-5f;

        for (int slot = 0; slot < kLcdParamSlots; ++slot)
        {
            const int col = slot % kCols;
            const int row = slot / kCols;
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
            const juce::String valueTxt =
                (pid == sculpt::ParameterId::SampleRootBpm)
                    ? juce::String (screen.rootBpmWhole)
                    : formatParamCellValue (pid, v, screen);
            g.drawText (valueTxt, valueArea, juce::Justification::centred, true);

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

    // ---- Color / DEFORM page visual ----
    // Two reactive orbs: left = low band (warm/orange), right = high band (cool/teal).
    // Size/glow driven by live audio levels, drive, tilt. Crush adds grid overlay; noise adds rings.
    void drawColorVisual (juce::Graphics& g, juce::Rectangle<int> areaInt,
                          const sculpt::ScreenModel& screen)
    {
        const auto& c   = screen.color;
        const auto  area = areaInt.toFloat();

        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (area, 3.0f);

        if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
            return;

        const float w  = area.getWidth();
        const float h  = area.getHeight();
        const float cx = area.getCentreX();
        const float cy = area.getCentreY();

        // Dividing line at the 650 Hz crossover.
        g.setColour (kText.withAlpha (0.12f));
        g.drawLine (cx, area.getY() + 4.0f, cx, area.getBottom() - 4.0f, 1.0f);
        g.setFont (juce::FontOptions (8.0f));
        g.setColour (kText.withAlpha (0.22f));
        g.drawText ("650Hz", juce::Rectangle<float> (cx - 16.0f, area.getBottom() - 13.0f, 32.0f, 10.0f),
                    juce::Justification::centred);

        const float baseR     = std::min (w * 0.18f, h * 0.38f);
        const float wetFade   = 0.3f + c.wet01 * 0.7f;
        const float driveBoost = 1.0f + c.drive01 * 0.5f;
        // Compress squashes both orbs vertically.
        const float squash    = 1.0f - c.compress01 * 0.30f;

        // Low-band orb (left, orange/warm).
        const float lowScale = (0.4f + c.tilt01 * 1.2f) * (0.55f + c.lowBandLevel * 0.9f) * driveBoost;
        const float lowR     = baseR * juce::jlimit (0.15f, 1.9f, lowScale) * wetFade;
        const float lowCx    = area.getX() + w * 0.28f;

        for (int layer = 3; layer >= 0; --layer)
        {
            const float lr    = lowR * (1.0f + static_cast<float> (layer) * 0.55f);
            const float alpha = (0.05f + (3 - layer) * 0.07f) * wetFade;
            g.setColour (kAccent.withAlpha (alpha));
            g.fillEllipse (lowCx - lr, cy - lr * squash, lr * 2.0f, lr * squash * 2.0f);
        }
        g.setColour (kAccent.withAlpha (0.82f * wetFade));
        g.fillEllipse (lowCx - lowR * 0.55f, cy - lowR * squash * 0.55f,
                       lowR * 1.1f, lowR * squash * 1.1f);

        // High-band orb (right, teal/cyan).
        const float highScale = (0.4f + (1.0f - c.tilt01) * 1.2f) * (0.55f + c.highBandLevel * 0.9f) * driveBoost;
        const float highR     = baseR * juce::jlimit (0.15f, 1.9f, highScale) * wetFade;
        const float highCx    = area.getX() + w * 0.72f;

        for (int layer = 3; layer >= 0; --layer)
        {
            const float lr    = highR * (1.0f + static_cast<float> (layer) * 0.55f);
            const float alpha = (0.05f + (3 - layer) * 0.07f) * wetFade;
            g.setColour (kWaveformStroke.withAlpha (alpha));
            g.fillEllipse (highCx - lr, cy - lr * squash, lr * 2.0f, lr * squash * 2.0f);
        }
        g.setColour (kWaveformStroke.withAlpha (0.82f * wetFade));
        g.fillEllipse (highCx - highR * 0.55f, cy - highR * squash * 0.55f,
                       highR * 1.1f, highR * squash * 1.1f);

        // Crush grid: draw a cell overlay whose density increases with crush amount.
        if (c.crush01 > 0.02f)
        {
            const int   gridN     = 4 + static_cast<int> (c.crush01 * 12.0f);  // 4..16 cells
            const float cellW     = w / static_cast<float> (gridN);
            const float cellH     = h / static_cast<float> (gridN);
            const float crushAlpha = c.crush01 * 0.18f;
            g.setColour (kText.withAlpha (crushAlpha));
            for (int row = 0; row < gridN; ++row)
                for (int col = 0; col < gridN; ++col)
                    g.drawRect (area.getX() + static_cast<float> (col) * cellW,
                                area.getY() + static_cast<float> (row) * cellH, cellW, cellH, 0.5f);
        }

        // Noise rings: halo around each orb; ring radius shifts with noiseTone.
        if (c.noise01 > 0.02f)
        {
            const float noiseAlpha = c.noise01 * 0.55f;
            const float ringOffset = 4.0f + c.noiseTone01 * 8.0f;

            const float nrl = lowR  + ringOffset;
            g.setColour (kAccent.withAlpha (noiseAlpha * 0.5f));
            g.drawEllipse (lowCx - nrl, cy - nrl * squash, nrl * 2.0f, nrl * squash * 2.0f, 1.5f);

            const float nrh = highR + ringOffset;
            g.setColour (kWaveformStroke.withAlpha (noiseAlpha * 0.5f));
            g.drawEllipse (highCx - nrh, cy - nrh * squash, nrh * 2.0f, nrh * squash * 2.0f, 1.5f);
        }

        // Page label.
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (kText.withAlpha (0.35f));
        g.drawText ("DEFORM", juce::Rectangle<float> (area.getX() + 4.0f, area.getY(), 60.0f, 14.0f),
                    juce::Justification::centredLeft);
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
    // Param grid uses engine `selectedPage` (audio thread). Match waveform + header to that page so
    // labels and Tape/Warp readouts line up with the visible cells.
    const sculpt::Page lcdPage = screen.selectedPage;

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
                               + "   " + juce::String (sculpt::PageModel::pageName (lcdPage)).toUpperCase();
    g.drawText (header, leftHdr, juce::Justification::centredLeft);
    g.setColour (screen.bpmValid ? kAccent : kText.withAlpha (0.55f));
    const juce::String bpmTxt = juce::String (screen.displayBpm, 1) + " BPM";
    g.drawText (bpmTxt, headerStrip, juce::Justification::centredRight);
    g.setColour (kText);

    // Output meters: fixed vertical column on the right — identical position on every page.
    // Reserved before the value grid / page visuals so both flow to the left of it.
    constexpr int meterColW = 44;
    auto meterCol = area.removeFromRight (meterColW);
    area.removeFromRight (6);
    drawVerticalMeters (g, meterCol, screen);

    const bool materialPage = (lcdPage == sculpt::Page::Material);
    const bool granularPage = (lcdPage == sculpt::Page::Granular);
    const bool filterPage   = (lcdPage == sculpt::Page::Filter);
    const bool colorPage    = (lcdPage == sculpt::Page::Color);
    const bool modPage      = (lcdPage == sculpt::Page::Mod);
    const bool spacePage    = (lcdPage == sculpt::Page::Space);
    // Prefer editor page; also treat engine-reported page as Mixer so the LCD never falls through
    // to the 4-track meter stack if message-thread vs audio-thread page state tears briefly.
    const bool mixerLcd     = (uiPage_ == sculpt::Page::Mixer)
                          || (screen.selectedPage == sculpt::Page::Mixer);
    const bool waveformPage = materialPage || granularPage;

    // Reserve the fixed value grid strip at the bottom on every page.
    constexpr int cols = 4;
    const int wantSlots =
        juce::jmin (16, juce::jmax (8, ((screen.numVisibleParams + cols - 1) / cols) * cols));
    const int lcdRows   = (wantSlots + cols - 1) / cols;
    const int valueH   = juce::jmin (area.getHeight(),
                                      juce::jmax (kValueGridMinH, lcdRows * 48 + 20));
    auto valueArea     = area.removeFromBottom (valueH);

    if (waveformPage)
    {
        // Meters now live in the right-side column, so the waveform gets the full height here
        // (minus the secondary strip below it).
        const int stripH    = 24;
        const int topBudget = area.getHeight();
        const int wfH       = juce::jmax (0, topBudget - stripH - 4);

        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        g.setColour (kLcdWaveformBg);
        g.fillRoundedRectangle (wfArea, 3.0f);
        if (materialPage)
        {
            drawMaterialLoopShade (g, wfArea, screen.materialLoopStart01, screen.materialLoopEnd01,
                                   screen.materialViewStart01, screen.materialViewEnd01);
            drawMaterialTimeGrid (g, wfArea, screen);
        }
        else
        {
            drawLoopRegion (g, wfArea, screen.materialLoopStart01, screen.materialLoopEnd01);
        }
        drawSymmetricEnvelope (g, wfArea, waveformPeaks_);
        if (materialPage)
            drawLoopFades (g, wfArea, screen);
        if (granularPage)
            drawGrainOverlay (g, wfArea, screen);

        auto strip = area.removeFromTop (stripH).toFloat().reduced (1.0f, 1.0f);
        drawSecondaryStrip (g, strip, waveformPeaks_);

        const juce::Rectangle<float> playRect (wfArea.getX(), wfArea.getY(), wfArea.getWidth(),
                                               wfArea.getHeight() + strip.getHeight());
        const int st = juce::jlimit (0, sculpt::kNumTracks - 1, screen.selectedTrack);
        float play01 = screen.tapePosition[static_cast<size_t> (st)];
        if (materialPage)
        {
            const float v0   = screen.materialViewStart01;
            const float v1   = screen.materialViewEnd01;
            const float span = juce::jmax (1.0e-5f, v1 - v0);
            play01             = (play01 - v0) / span;
            play01             = juce::jlimit (0.0f, 1.0f, play01);
        }
        drawPlayhead (g, playRect, play01);
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
        // Meters moved to the right column; oscilloscope takes the full height.
        const int wfH = juce::jmax (0, area.getHeight() - 4);
        auto wfArea = area.removeFromTop (wfH).toFloat().reduced (1.0f, 2.0f);
        drawModOscilloscope (g, wfArea, screen.modLcd);
    }
    else if (filterPage)
    {
        const int bandDisplayH = juce::jlimit (40, area.getHeight(), area.getHeight() - 4);
        drawFilterBands (g, area.removeFromTop (bandDisplayH), screen);
    }
    else if (colorPage)
    {
        const int visH = juce::jmax (60, area.getHeight() - 4);
        drawColorVisual (g, area.removeFromTop (visH), screen);
    }
    else if (spacePage)
    {
        const int visH = juce::jmax (60, area.getHeight() - 4);
        drawSpaceVisual (g, area.removeFromTop (visH), screen);
    }
    else
    {
        const int meterBlockH = juce::jmin (140, juce::jmax (80, area.getHeight()));
        drawAllTrackMeters (g, area.removeFromTop (meterBlockH), screen);
    }

    // 2x4 VALUE readout grid — same fixed size and position on every page.
    drawValueGrid (g, valueArea, screen);
}
