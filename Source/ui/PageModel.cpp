#include "PageModel.h"

namespace sculpt
{

namespace
{
    constexpr ParameterId kEmpty = ParameterId::Count;

    // Granular: two 8-encoder banks.
    // p1=core sound / p2=rhythm+pitch+pattern (S-4-style split).
    constexpr ParameterId kGranularEncoderPage[2][8] = {
        // Page 1: core grain controls — Contour replaces Texture for direct S-4 musicality
        { ParameterId::GrainPosition, ParameterId::GrainSize, ParameterId::GrainDensity,
          ParameterId::GrainPitch, ParameterId::GrainSpray, ParameterId::GrainContour,
          ParameterId::GrainSpread, ParameterId::GrainMix },
        // Page 2: rhythm + pitch + choreography; RandRev fills the formerly empty 8th slot
        { ParameterId::GrainSync, ParameterId::GrainSteps, ParameterId::GrainPulses,
          ParameterId::GrainRotate, ParameterId::GrainPitchQuant, ParameterId::GrainPattern,
          ParameterId::GrainPatternAmount, ParameterId::GrainRandRev },
    };

    // Every row must list all kMaxParamsPerPage slots: omitted trailing elements value-init to
    // ParameterId{} == OutputGain (enum starts at 0), which breaks UI loops that stop on Count.
    constexpr ParameterId kPageParams[static_cast<int> (Page::Count)][kMaxParamsPerPage] =
    {
        // Material: TimeMode + RootBpm replace Wave Zoom on the LCD row (zoom remains in APVTS / editor).
        { ParameterId::MaterialLevel, ParameterId::TapeSpeed, ParameterId::MaterialTimeMode,
          ParameterId::SampleRootBpm, ParameterId::LoopStart, ParameterId::LoopEnd,
          ParameterId::CaptureArm, ParameterId::MaterialPlayhead,
          ParameterId::LoopSnapGrid, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Granular row unused for slots — use kGranularEncoderPage + granularEncoderPage in parameterForSlot.
        { kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty },
        // Filter (8 encoder slots)
        { ParameterId::FilterCutoff,    ParameterId::FilterResonance,
          ParameterId::FilterDecay,     ParameterId::FilterPitch,
          ParameterId::FilterScale,     ParameterId::FilterMode,
          ParameterId::FilterKey,       ParameterId::FilterMix,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Color (DEFORM): drive, crush, tilt, compress, noise, noise decay, noise tone, wet
        { ParameterId::ColorDrive, ParameterId::ColorCrush, ParameterId::ColorTilt,
          ParameterId::ColorCompress, ParameterId::ColorNoise, ParameterId::ColorNoiseDecay,
          ParameterId::ColorNoiseTone, ParameterId::ColorWet,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Space (Vast-style)
        { ParameterId::SpaceDelayAmount,   ParameterId::SpaceDelayTime,     ParameterId::SpaceReverbAmount,
          ParameterId::SpaceReverbSize,    ParameterId::SpaceDelayFeedback, ParameterId::SpaceSpread,
          ParameterId::SpaceDamp,            ParameterId::SpaceReverbDecay, ParameterId::SpaceDelayTimeMode,
          ParameterId::SpaceFreeze, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
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
        const int g = std::clamp (granularEncoderPage, 0, 1);
        return kGranularEncoderPage[static_cast<size_t> (g)][static_cast<size_t> (slot)];
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
