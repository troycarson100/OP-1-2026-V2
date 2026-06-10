#pragma once

#include <array>
#include "ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

class ParameterState;

// One performance snapshot. Plain portable data - this is core product
// behavior, not a plugin preset. Serialization (in the JUCE wrapper or a
// future hardware filesystem) reads/writes these fields but never defines them.
struct Scene
{
    bool used = false;

    std::array<std::array<float, kNumParameters>, kNumTracks> trackParams {};
    std::array<float, kNumMacros> macros {};

    int selectedTrack = 0;
    int selectedPage  = 0;

    void captureFrom (const ParameterState& state, int currentTrack, int currentPage);
    void applyTo (ParameterState& state) const;

    // Linear blend of two scenes' values into the live state.
    static void morphInto (const Scene& a, const Scene& b, float amount, ParameterState& state);
};

} // namespace sculpt
