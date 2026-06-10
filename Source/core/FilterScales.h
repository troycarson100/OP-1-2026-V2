#pragma once

#include <cmath>

namespace sculpt
{

enum class FilterScale : int
{
    Free = 0,
    Chromatic,
    Major,
    Minor,
    Pentatonic,
    Blues,
    Dorian,
    Lydian,
    WholeTone,
    Fifths,
    Octaves,
    Count
};

inline const char* filterScaleName (FilterScale s)
{
    switch (s)
    {
        case FilterScale::Free:       return "Free";
        case FilterScale::Chromatic:  return "Chromatic";
        case FilterScale::Major:      return "Major";
        case FilterScale::Minor:      return "Minor";
        case FilterScale::Pentatonic: return "Penta";
        case FilterScale::Blues:      return "Blues";
        case FilterScale::Dorian:     return "Dorian";
        case FilterScale::Lydian:     return "Lydian";
        case FilterScale::WholeTone:  return "Whole";
        case FilterScale::Fifths:     return "Fifths";
        case FilterScale::Octaves:    return "Octaves";
        default:                      return "?";
    }
}

// Returns semitone offsets within one octave for the given scale.
inline const int* scaleIntervals (FilterScale s, int& outCount)
{
    static constexpr int chromatic[]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr int major[]      = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minor[]      = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int pentatonic[] = { 0, 3, 5, 7, 10 };
    static constexpr int blues[]      = { 0, 3, 5, 6, 7, 10 };
    static constexpr int dorian[]     = { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr int lydian[]     = { 0, 2, 4, 6, 7, 9, 11 };
    static constexpr int wholeTone[]  = { 0, 2, 4, 6, 8, 10 };
    static constexpr int fifths[]     = { 0, 7 };
    static constexpr int octaves[]    = { 0 };

    switch (s)
    {
        case FilterScale::Free:
        case FilterScale::Chromatic:  outCount = 12; return chromatic;
        case FilterScale::Major:      outCount = 7;  return major;
        case FilterScale::Minor:      outCount = 7;  return minor;
        case FilterScale::Pentatonic: outCount = 5;  return pentatonic;
        case FilterScale::Blues:      outCount = 6;  return blues;
        case FilterScale::Dorian:     outCount = 7;  return dorian;
        case FilterScale::Lydian:     outCount = 7;  return lydian;
        case FilterScale::WholeTone:  outCount = 6;  return wholeTone;
        case FilterScale::Fifths:     outCount = 2;  return fifths;
        case FilterScale::Octaves:    outCount = 1;  return octaves;
        default:                      outCount = 12; return chromatic;
    }
}

// Maps a normalized 0..1 value to a FilterScale enum.
inline FilterScale normalizedToFilterScale (float n)
{
    const int count = static_cast<int> (FilterScale::Count);
    const int idx   = static_cast<int> (n * static_cast<float> (count - 1) + 0.5f);
    return static_cast<FilterScale> (idx < 0 ? 0 : (idx >= count ? count - 1 : idx));
}

// Snaps a MIDI note (float) to the nearest scale degree relative to rootMidi.
inline float snapToScaleMidi (float midiNote, int rootMidi, FilterScale s)
{
    if (s == FilterScale::Free)
        return midiNote;

    int count = 0;
    const int* intervals = scaleIntervals (s, count);

    const float rel            = midiNote - static_cast<float> (rootMidi);
    const int   octave         = static_cast<int> (std::floor (rel / 12.0f));
    const float semInOctave    = rel - static_cast<float> (octave * 12);

    float best     = static_cast<float> (intervals[0]);
    float bestDist = std::fabs (semInOctave - best);
    for (int i = 1; i < count; ++i)
    {
        const float dist = std::fabs (semInOctave - static_cast<float> (intervals[i]));
        if (dist < bestDist)
        {
            bestDist = dist;
            best     = static_cast<float> (intervals[i]);
        }
    }
    return static_cast<float> (rootMidi) + static_cast<float> (octave * 12) + best;
}

// Snaps a frequency in Hz to the nearest scale note, with rootHz as the tonic.
inline float snapToScale (float freqHz, float rootHz, FilterScale s)
{
    if (s == FilterScale::Free || freqHz <= 0.0f || rootHz <= 0.0f)
        return freqHz;

    const float midiFreq   = 69.0f + 12.0f * std::log2 (freqHz / 440.0f);
    const float midiRoot   = 69.0f + 12.0f * std::log2 (rootHz / 440.0f);
    const int   rootMidi   = static_cast<int> (std::round (midiRoot));
    const float snappedMidi = snapToScaleMidi (midiFreq, rootMidi, s);
    return 440.0f * std::pow (2.0f, (snappedMidi - 69.0f) / 12.0f);
}

} // namespace sculpt
