#include "PageModel.h"

namespace sculpt
{

namespace
{
    constexpr ParameterId kEmpty = ParameterId::Count;

    constexpr ParameterId kPageParams[static_cast<int> (Page::Count)][kMaxParamsPerPage] =
    {
        // Material
        { ParameterId::MaterialLevel, ParameterId::TapeSpeed, ParameterId::LoopStart,
          ParameterId::LoopEnd, ParameterId::CaptureArm, kEmpty, kEmpty, kEmpty },
        // Granular
        { ParameterId::GrainPosition, ParameterId::GrainSize, ParameterId::GrainDensity,
          ParameterId::GrainPitch, ParameterId::GrainSpray, ParameterId::GrainTexture,
          ParameterId::GrainSpread, ParameterId::GrainMix },
        // Filter (8 encoder slots)
        // Slots 0-3: Cutoff, Resonance, Decay/Slope, Pitch
        // Slots 4-7: Scale, Mode, Key, Mix
        { ParameterId::FilterCutoff,    ParameterId::FilterResonance,
          ParameterId::FilterDecay,     ParameterId::FilterPitch,
          ParameterId::FilterScale,     ParameterId::FilterMode,
          ParameterId::FilterKey,       ParameterId::FilterMix },
        // Color
        { ParameterId::ColorDrive, ParameterId::ColorTone, ParameterId::ColorMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Space
        { ParameterId::SpaceAmount, ParameterId::SpaceFeedback, ParameterId::SpaceMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Mixer
        { ParameterId::TrackLevel, ParameterId::TrackPan, ParameterId::OutputGain,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Mod (encoder mapping lives on Mod page UI; no APVTS row here)
        { kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
    };
}

const char* PageModel::pageName (Page page)
{
    switch (page)
    {
        case Page::Material: return "Material";
        case Page::Granular: return "Granular";
        case Page::Filter:   return "Filter";
        case Page::Color:    return "Color";
        case Page::Space:    return "Space";
        case Page::Mixer:    return "Mixer";
        case Page::Mod:     return "Mod";
        default:             return "?";
    }
}

ParameterId PageModel::parameterForSlot (Page page, int slot)
{
    const int p = static_cast<int> (page);
    if (p < 0 || p >= static_cast<int> (Page::Count) || slot < 0 || slot >= kMaxParamsPerPage)
        return kEmpty;
    return kPageParams[p][slot];
}

int PageModel::parameterCount (Page page)
{
    int count = 0;
    for (int i = 0; i < kMaxParamsPerPage; ++i)
        if (parameterForSlot (page, i) != kEmpty)
            ++count;
    return count;
}

} // namespace sculpt
