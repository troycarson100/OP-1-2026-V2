#include "SpaceVisual.h"
#include "EditorColours.h"
#include "../ScreenModel.h"

#include <cmath>

namespace sculpt_editor
{

static float fract (float x) { return x - std::floor (x); }

static void drawDelayHalf (juce::Graphics& g, juce::Rectangle<float> r,
                           const sculpt::SpaceVisualSnapshot& s, float tSec)
{
    g.setColour (kLcdWaveformBg);
    g.fillRoundedRectangle (r, 3.0f);

    const juce::Colour base = s.frozen ? kParamModDot : kWaveformStroke;
    const float bright = 0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, s.delayWet01);

    auto inner = r.reduced (8.0f, 6.0f);
    const float cx0 = inner.getX();
    const float cy  = inner.getCentreY();
    const float halfH = inner.getHeight() * 0.5f - 2.0f;

    // Echo spacing from delay time across a ~1.6 s window.
    constexpr float windowSec = 1.6f;
    const float spacing = juce::jlimit (6.0f, inner.getWidth() * 0.55f,
                                        (s.delaySeconds / windowSec) * inner.getWidth());

    const float ping  = s.delaySpread01 < 0.5f ? (1.0f - s.delaySpread01 * 2.0f) : 0.0f;
    const float diff  = s.delaySpread01 > 0.5f ? (s.delaySpread01 - 0.5f) * 2.0f : 0.0f;
    const float fb    = juce::jlimit (0.0f, 0.98f, s.delayFeedback01);
    const float barW  = 2.0f + diff * 6.0f;

    constexpr int maxEchoes = 14;
    for (int i = 0; i < maxEchoes; ++i)
    {
        const float x = cx0 + static_cast<float> (i) * spacing;
        if (x > inner.getRight())
            break;

        const float gain = (i == 0) ? 1.0f : (s.frozen ? 0.9f : std::pow (fb, static_cast<float> (i)));
        if (gain < 0.02f && i > 0)
            break;

        const float h = halfH * (0.18f + 0.82f * gain);
        const float yOff = (i == 0) ? 0.0f
                                    : ping * halfH * 0.5f * ((i % 2 == 0) ? -1.0f : 1.0f);

        const float a = bright * (i == 0 ? 1.0f : gain);
        g.setColour (base.withAlpha (juce::jlimit (0.05f, 1.0f, a)));
        juce::Rectangle<float> bar (x - barW * 0.5f, cy + yOff - h, barW, h * 2.0f);
        g.fillRoundedRectangle (bar, barW * 0.4f);
    }

    // Travelling head implies motion along the echo train.
    if (! s.frozen && s.delaySeconds > 1.0e-3f)
    {
        const float period = juce::jmax (0.05f, s.delaySeconds);
        const float headX  = cx0 + fract (tSec / period) * spacing;
        if (headX <= inner.getRight())
        {
            g.setColour (kWaveformPlayhead.withAlpha (0.5f + 0.5f * bright));
            g.fillEllipse (headX - 2.5f, cy - 2.5f, 5.0f, 5.0f);
        }
    }

    g.setColour (kText.withAlpha (0.7f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("DELAY", r.reduced (6.0f, 4.0f), juce::Justification::topLeft, false);

    const char* modeNames[] = { "1/4", "1/4.", "1/4T", "FREE" };
    const int   mi = juce::jlimit (0, 3, s.delayTimeMode);
    const juce::String val = juce::String (juce::roundToInt (s.delaySeconds * 1000.0f)) + " ms  "
                             + juce::String (modeNames[mi]);
    g.setColour (base.withAlpha (0.85f));
    g.drawText (val, r.reduced (6.0f, 4.0f), juce::Justification::topRight, false);

    g.setColour (kText.withAlpha (0.55f));
    g.drawText ("FB " + juce::String (juce::roundToInt (fb * 100.0f)) + "%",
                r.reduced (6.0f, 4.0f), juce::Justification::bottomLeft, false);
}

static void drawReverbHalf (juce::Graphics& g, juce::Rectangle<float> r,
                            const sculpt::SpaceVisualSnapshot& s, float tSec)
{
    g.setColour (kLcdWaveformBg);
    g.fillRoundedRectangle (r, 3.0f);

    const juce::Colour base = s.frozen ? kParamModDot : kAccent;
    const float wet = juce::jlimit (0.0f, 1.0f, s.reverbWet01);

    auto inner = r.reduced (8.0f, 6.0f);
    const float cx = inner.getCentreX();
    const float cy = inner.getCentreY();
    const float maxR = juce::jmin (inner.getWidth(), inner.getHeight()) * 0.46f;

    const float sizeF   = 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, s.reverbSize01);
    const float decayN  = juce::jlimit (0.0f, 1.0f, s.reverbDecaySeconds / 30.0f);
    const int   rings   = 4 + juce::roundToInt (decayN * 8.0f);
    const float damp    = juce::jlimit (0.0f, 1.0f, s.reverbDamp01);
    const float drift   = s.frozen ? 0.0f : fract (tSec * 0.22f);

    // Soft central cloud.
    {
        const float cr = maxR * sizeF * 0.55f;
        g.setColour (base.withAlpha (0.10f + 0.40f * wet));
        g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
    }

    for (int i = 0; i < rings; ++i)
    {
        const float frac = (static_cast<float> (i) + drift) / static_cast<float> (rings);
        if (frac >= 1.0f)
            continue;
        const float rad = frac * maxR * sizeF;
        const float a   = (1.0f - frac) * (0.20f + 0.80f * wet) * (1.0f - 0.55f * damp * frac);
        g.setColour (base.withAlpha (juce::jlimit (0.03f, 1.0f, a)));
        g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.4f);
    }

    g.setColour (kText.withAlpha (0.7f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("REVERB", r.reduced (6.0f, 4.0f), juce::Justification::topLeft, false);

    g.setColour (base.withAlpha (0.85f));
    g.drawText (juce::String (s.reverbDecaySeconds, 1) + " s",
                r.reduced (6.0f, 4.0f), juce::Justification::topRight, false);

    g.setColour (kText.withAlpha (0.55f));
    g.drawText ("SIZE " + juce::String (juce::roundToInt (s.reverbSize01 * 100.0f)) + "%",
                r.reduced (6.0f, 4.0f), juce::Justification::bottomLeft, false);
}

void drawSpaceVisual (juce::Graphics& g, juce::Rectangle<int> area, const sculpt::ScreenModel& screen)
{
    const auto& s = screen.space;
    const float tSec = static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.001);

    auto bounds = area.toFloat();
    constexpr float gap = 6.0f;
    const float halfW = (bounds.getWidth() - gap) * 0.5f;

    auto leftHalf  = bounds.removeFromLeft (halfW);
    bounds.removeFromLeft (gap);
    auto rightHalf = bounds;

    drawDelayHalf (g, leftHalf, s, tSec);
    drawReverbHalf (g, rightHalf, s, tSec);

    if (s.frozen)
    {
        g.setColour (kParamModDot.withAlpha (0.9f));
        g.drawRoundedRectangle (area.toFloat().reduced (1.0f), 3.0f, 1.5f);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("FROZEN", area, juce::Justification::centred, false);
    }
}

} // namespace sculpt_editor
