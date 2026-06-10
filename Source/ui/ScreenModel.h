#pragma once

#include <array>
#include "PageModel.h"
#include "../util/Constants.h"

namespace sculpt
{

// Abstract display state for the future hardware screen.
// The engine writes it once per block; any frontend (JUCE debug editor now,
// an embedded display later) only reads it. No drawing code lives here.
//
// Note: fields are plain floats written on the audio thread and read on a
// UI thread. Individual float tears are harmless for display purposes.
struct ScreenModel
{
    int selectedTrack = 0;
    int currentScene  = 0;
    Page selectedPage = Page::Granular;

    std::array<bool,  kNumTracks> trackPlaying {};
    std::array<bool,  kNumTracks> trackRecording {};
    std::array<float, kNumTracks> trackMeter {};      // 0..1 peak
    std::array<float, kNumTracks> grainActivity {};   // 0..1 pool usage
    std::array<float, kNumTracks> tapePosition {};    // 0..1 playhead

    float masterMeterL = 0.0f;
    float masterMeterR = 0.0f;

    // Selected track tape loop region (normalized, same as tape uses) for Material LCD.
    float materialLoopStart01 = 0.0f;
    float materialLoopEnd01   = 1.0f;

    std::array<float, kNumMacros> macroValues {};

    // Parameter readout for the selected page/track.
    std::array<const char*, kMaxParamsPerPage> paramNames {};
    std::array<float, kMaxParamsPerPage> paramValues {};
    int numVisibleParams = 0;
};

} // namespace sculpt
