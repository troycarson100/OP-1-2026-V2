#include "PageModel.h"

namespace sculpt
{

namespace
{
    constexpr ParameterId kEmpty = ParameterId::Count;

    // Material: two 8-encoder banks (S-4-style split). p1 = loop + pitch/scale performance,
    // p2 = mode / capture / utility. Indexed by the shared encoder sub-page.
    constexpr ParameterId kMaterialEncoderPage[2][8] = {
        // Page 1: Level, Loop Start, Loop End, Playhead, Pitch, Scale, Key, Cross Fade
        { ParameterId::MaterialLevel, ParameterId::LoopStart, ParameterId::LoopEnd,
          ParameterId::MaterialPlayhead, ParameterId::TapeSpeed, ParameterId::MaterialPitchScale,
          ParameterId::MaterialPitchKey, ParameterId::MaterialLoopXfade },
        // Page 2: Grid Snap, Grid Div, Time Mode, Root BPM, Capture Arm, Wave Zoom, Sample Mode, Sample Slot
        { ParameterId::LoopSnapGrid, ParameterId::MaterialGridDivision, ParameterId::MaterialTimeMode,
          ParameterId::SampleRootBpm, ParameterId::CaptureArm, ParameterId::MaterialWaveZoom,
          ParameterId::MaterialSampleMode, ParameterId::MaterialSampleSlot },
    };

    // Granular: two 8-encoder banks.
    // p1=core sound / p2=rhythm+pitch+pattern (S-4-style split).
    constexpr ParameterId kGranularEncoderPage[2][8] = {
        // Page 1: core grain controls — Contour replaces Texture for direct S-4 musicality
        { ParameterId::GrainPosition, ParameterId::GrainSize, ParameterId::GrainDensity,
          ParameterId::GrainPitch, ParameterId::GrainSpray, ParameterId::GrainContour,
          ParameterId::GrainSpread, ParameterId::GrainMix },
        // Page 2: rhythm + pitch + choreography. Follow (playhead tracking) takes the slot the
        // now-superseded Pitch Quant held — grain pitch follows the Material scale/key directly.
        { ParameterId::GrainSync, ParameterId::GrainSteps, ParameterId::GrainPulses,
          ParameterId::GrainRotate, ParameterId::GrainFollow, ParameterId::GrainPattern,
          ParameterId::GrainPatternAmount, ParameterId::GrainRandRev },
    };

    // Every row must list all kMaxParamsPerPage slots: omitted trailing elements value-init to
    // ParameterId{} == OutputGain (enum starts at 0), which breaks UI loops that stop on Count.
    constexpr ParameterId kPageParams[static_cast<int> (Page::Count)][kMaxParamsPerPage] =
    {
        // Material row unused for slots — uses kMaterialEncoderPage + encoder sub-page in parameterForSlot.
        { kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
          kEmpty, kEmpty, kEmpty },
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
        // Space: GLOBAL reverb/delay character (one shared FX; tracks feed it via the Mixer-page
        // Rev/Dly Send knobs). Delay first, then reverb, then shared damp/spread/freeze.
        { ParameterId::GlobalDelayTime,     ParameterId::GlobalDelayFeedback, ParameterId::GlobalDelayTimeMode,
          ParameterId::GlobalReverbSize,    ParameterId::GlobalReverbDecay,
          ParameterId::GlobalSpaceDamp,     ParameterId::GlobalSpaceSpread,   ParameterId::GlobalSpaceFreeze,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
        // Mixer: level/pan, master out, 3-band mix EQ, compressor, + per-track Reverb/Delay sends.
        { ParameterId::TrackLevel, ParameterId::TrackPan, ParameterId::OutputGain,
          ParameterId::MixEqLowGain, ParameterId::MixEqMidGain, ParameterId::MixEqHighGain,
          ParameterId::MixCompThreshold, ParameterId::MixCompMakeup,
          ParameterId::SpaceReverbAmount, ParameterId::SpaceDelayAmount,
          kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty },
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

    if (page == Page::Material)
    {
        if (slot >= 8)
            return kEmpty;
        const int g = std::clamp (granularEncoderPage, 0, 1);
        return kMaterialEncoderPage[static_cast<size_t> (g)][static_cast<size_t> (slot)];
    }

    return kPageParams[p][slot];
}

ParameterId PageModel::modDestinationForSlot (Page page, int enc)
{
    if (enc < 0 || enc >= kMaxModMappingEncoders)
        return kEmpty;

    if (page == Page::Material)
    {
        // Curated Material modulation destinations across both encoder sub-pages: the musical ones
        // plus the new Sample Slot (random/LFO sample selection) and Sample Mode (playback dir).
        static constexpr ParameterId kMaterialModDest[kMaxModMappingEncoders] = {
            ParameterId::MaterialLevel, ParameterId::LoopStart, ParameterId::LoopEnd,
            ParameterId::MaterialPlayhead, ParameterId::TapeSpeed, ParameterId::MaterialSampleSlot,
            ParameterId::MaterialSampleMode, ParameterId::MaterialLoopXfade,
        };
        return kMaterialModDest[enc];
    }

    return parameterForSlot (page, enc);
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
