#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "MathUtils.h"

namespace sculpt
{

// Scale tables: semitone offsets within one octave [0, 12), first element 0 (root).
constexpr std::array<int, 3>  kGrainScaleOctavesFifths { 0, 5, 7 };
constexpr std::array<int, 5>  kGrainScaleMajorPent     { 0, 2, 4, 7, 9 };
constexpr std::array<int, 5>  kGrainScaleMinorPent     { 0, 3, 5, 7, 10 };
constexpr std::array<int, 7>  kGrainScaleMajor         { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array<int, 7>  kGrainScaleNatMinor      { 0, 2, 3, 5, 7, 8, 10 };
constexpr std::array<int, 7>  kGrainScaleDorian        { 0, 2, 3, 5, 7, 9, 10 };
constexpr std::array<int, 12> kGrainScaleChromatic     { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

// Normalized GrainPitchQuant: 0 = off. Stepped indices 1..7 map to scale tables.
// 0=Off, 1=Fifths, 2=MajPent, 3=MinPent, 4=Major, 5=NatMinor, 6=Dorian, 7=Chromatic
inline int grainPitchQuantScaleIndex (float n01)
{
    return std::clamp (static_cast<int> (std::lround (clamp01 (n01) * 7.0f)), 0, 7);
}

// Snap `semis` (semitone offset, any range) to nearest scale degree; scaleIndex 0 = no change.
inline float quantizeGrainPitchSemitones (float semis, int scaleIndex)
{
    if (scaleIndex <= 0)
        return semis;

    const float octave = std::floor (semis / 12.0f) * 12.0f;
    float       phase  = semis - octave; // [0, 12)
    if (phase < 0.0f)
        phase += 12.0f;
    if (phase >= 12.0f)
        phase -= 12.0f;

    auto nearestInList = [] (float ph, const int* data, int count) {
        float best = static_cast<float> (data[0]);
        float bestD = std::fabs (ph - best);
        for (int i = 1; i < count; ++i)
        {
            const float v = static_cast<float> (data[i]);
            const float d = std::fabs (ph - v);
            if (d < bestD)
            {
                bestD = d;
                best  = v;
            }
        }
        // Also consider octave +12 for wrap (e.g. phase near 11 snap to 12 as 0 next octave - skip)
        return best;
    };

    float snapped = 0.0f;
    switch (scaleIndex)
    {
        case 1:
            snapped = nearestInList (phase, kGrainScaleOctavesFifths.data (),
                                     static_cast<int> (kGrainScaleOctavesFifths.size ()));
            break;
        case 2:
            snapped = nearestInList (phase, kGrainScaleMajorPent.data (),
                                     static_cast<int> (kGrainScaleMajorPent.size ()));
            break;
        case 3:
            snapped = nearestInList (phase, kGrainScaleMinorPent.data (),
                                     static_cast<int> (kGrainScaleMinorPent.size ()));
            break;
        case 4:
            snapped = nearestInList (phase, kGrainScaleMajor.data (),
                                     static_cast<int> (kGrainScaleMajor.size ()));
            break;
        case 5:
            snapped = nearestInList (phase, kGrainScaleNatMinor.data (),
                                     static_cast<int> (kGrainScaleNatMinor.size ()));
            break;
        case 6:
            snapped = nearestInList (phase, kGrainScaleDorian.data (),
                                     static_cast<int> (kGrainScaleDorian.size ()));
            break;
        case 7:
            snapped = nearestInList (phase, kGrainScaleChromatic.data (),
                                     static_cast<int> (kGrainScaleChromatic.size ()));
            break;
        default:
            return semis;
    }

    return octave + snapped;
}

} // namespace sculpt
