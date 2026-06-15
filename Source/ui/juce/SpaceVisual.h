#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sculpt { struct ScreenModel; }

namespace sculpt_editor
{

// Draws the SPACE-page LCD: an animated delay echo-train (left) and a reverb
// decay bloom (right), with a freeze indicator. Reads sculpt::ScreenModel::space.
void drawSpaceVisual (juce::Graphics& g, juce::Rectangle<int> area, const sculpt::ScreenModel& screen);

} // namespace sculpt_editor
