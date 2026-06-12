#include "PageModel.h"

namespace sculpt
{

namespace
{
    constexpr ParameterId kEmpty = ParameterId::Count;

    // Granular: two hardware pages of 8 encoders each (UI sub-page selector).
    constexpr ParameterId kGranularEncoderPage[2][8] = {
        { ParameterId::GrainPosition, ParameterId::GrainSize, ParameterId::GrainDensity,
          ParameterId::GrainPitch, ParameterId::GrainSpray, ParameterId::GrainTexture,
          ParameterId::GrainSpread, ParameterId::GrainMix },
        { ParameterId::GrainSync, ParameterId::GrainSteps, ParameterId::GrainPulses,
          ParameterId::GrainRotate, ParameterId::GrainPitchQuant, kEmpty, kEmpty, kEmpty },
    };

    // Every row must list all kMaxParamsPerPage slots: omitted trailing elements value-init to
    // ParameterId{} == OutputGain (enum starts at 0), which breaks UI loops that stop on Count.
    constexpr ParameterId kPageParams[static_cast<int> (Page::Count)][kMaxParamsPerPage] =
    {
        // Material: TimeMode + RootBpm replace Wave Zoom on the LCD row (zoom remains in APVTS / editor).
        { ParameterId::MaterialLevel, ParameterId::TapeSpeed, ParameterId::MaterialTimeMode,
          ParameterId::SampleRootBpm, ParameterId::LoopStart, ParameterId::LoopEnd,
          ParameterId::CaptureArm, ParameterId::MaterialPlayhead,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Granular row unused for slots — use kGranularEncoderPage + granularEncoderPage in parameterForSlot.
        { kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty },
        // Filter (8 encoder slots)
        { ParameterId::FilterCutoff,    ParameterId::FilterResonance,
          ParameterId::FilterDecay,     ParameterId::FilterPitch,
          ParameterId::FilterScale,     ParameterId::FilterMode,
          ParameterId::FilterKey,       ParameterId::FilterMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Color
        { ParameterId::ColorDrive, ParameterId::ColorTone, ParameterId::ColorMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Space
        { ParameterId::SpaceAmount, ParameterId::SpaceFeedback, ParameterId::SpaceMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Mixer: level/pan, master out, 3-band mix EQ, compressor (stereo-linked in MixBusStage).
        { ParameterId::TrackLevel, ParameterId::TrackPan, ParameterId::OutputGain,
          ParameterId::MixEqLowGain, ParameterId::MixEqMidGain, ParameterId::MixEqHighGain,
          ParameterId::MixCompThreshold, ParameterId::MixCompMakeup,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Mod (encoder mapping lives on Mod page UI; no APVTS row here)
        { kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
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

ParameterId PageModel::parameterForSlot (Page page, int slot, int granularEncoderPage)
{
    const int p = static_cast<int> (page);
    if (p < 0 || p >= static_cast<int> (Page::Count) || slot < 0 || slot >= kMaxParamsPerPage)
        return kEmpty;

    if (page == Page::Granular)
    {
        if (slot >= 8)
            return kEmpty;
        const int g = (granularEncoderPage > 0) ? 1 : 0;
        return kGranularEncoderPage[g][static_cast<size_t> (slot)];
    }

    return kPageParams[p][slot];
}

int PageModel::parameterCount (Page page, int granularEncoderPage)
{
    int count = 0;
    for (int i = 0; i < kMaxParamsPerPage; ++i)
        if (parameterForSlot (page, i, granularEncoderPage) != kEmpty)
            ++count;
    return count;
}

} // namespace sculpt
