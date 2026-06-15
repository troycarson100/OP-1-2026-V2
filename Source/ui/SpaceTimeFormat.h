#pragma once

#include <cstdio>
#include <string>
#include "../core/ParameterIds.h"

namespace sculpt
{

// Single source of truth for the Space delay-time readout, shared by the LCD value
// grid and the rotary text box so they always agree with the Time Mode.
// modeIdx: 0 Straight, 1 Dotted, 2 Triplet, 3 Free.
inline std::string formatSpaceDelayTime (float timeKnob01, int modeIdx)
{
    if (modeIdx >= 3) // Free: real time
    {
        const float sec = map::spaceDelayTimeFreeSeconds (timeKnob01);
        char buf[16];
        if (sec < 1.0f)
            std::snprintf (buf, sizeof buf, "%d ms", static_cast<int> (sec * 1000.0f + 0.5f));
        else
            std::snprintf (buf, sizeof buf, "%.2f s", static_cast<double> (sec));
        return std::string (buf);
    }

    std::string s = map::spaceDelayDivisionLabel (map::spaceDelayDivisionIndex (timeKnob01));
    if (modeIdx == 1)
        s += ".";   // dotted
    else if (modeIdx == 2)
        s += "T";   // triplet
    return s;
}

} // namespace sculpt
