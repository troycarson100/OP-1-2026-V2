#pragma once

#include <array>
#include "PageModel.h"
#include "../core/GrainDisplaySlot.h"
#include "../core/ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

// Mod page LCD: one-cycle preview + depth layer + scanner (audio thread writes).
struct ModLcdSnapshot
{
    static constexpr int kBins = kMaterialWaveformBins;

    std::array<float, kBins> carrier01 {};   // full-scale shape outline 0..1
    std::array<float, kBins> effective01 {}; // shape * amount (modulation layer)
    float scannerPhase01 = 0.0f;             // vertical playhead 0..1 through cycle
    float valueBipolar   = 0.0f;             // current source output (with offset)
    float amount01       = 1.0f;             // wave amount or unity for other kinds
    uint8_t kind         = 0;                 // ModulatorKind as uint8
    bool    active        = false;
};

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

    // Global tempo from portable Clock (host BPM when available). LCD top-right.
    float  displayBpm = 120.0f;
    bool   bpmValid   = true;

    // Selected track tape loop region (normalized, same as tape uses) for Material LCD.
    float materialLoopStart01 = 0.0f;
    float materialLoopEnd01   = 1.0f;

    // Material waveform: total length (seconds) and horizontal view window across the buffer.
    float materialDurationSec = 0.0f;
    float materialViewStart01 = 0.0f;
    float materialViewEnd01   = 1.0f;

    std::array<float, kNumMacros> macroValues {};

    // Parameter readout for the selected page/track.
    std::array<const char*, kMaxParamsPerPage> paramNames {};
    std::array<float, kMaxParamsPerPage> paramValues {};
    std::array<float, kMaxParamsPerPage> paramModOffset {}; // summed mod delta this block (for LCD hints)
    std::array<ParameterId, kMaxParamsPerPage> paramIds {};
    int numVisibleParams = 0;

    // Filter page: per-band envelope for the spectral resonator display (48 bands).
    // All zero when in LPF mode. Size matches SpectralFilterStage::kNumBands.
    static constexpr int kFilterBands = 48;
    std::array<float, kFilterBands> filterBandGains {};
    bool filterSpectralMode = false;

    // Granular page: per-grain overlay on the material waveform (pool size).
    std::array<GrainDisplaySlot, kGrainsPerTrack> grainDisplay {};
    // Knob-aligned grain window (no spray); LCD draws this for snappy feedback vs active grains.
    float grainFocusStart01 = 0.0f;
    float grainFocusLen01   = 0.0f;

    // Mod page: oscilloscope-style source preview (see ModLcdSnapshot).
    ModLcdSnapshot modLcd {};

    // Mixer page: post-mix-bus stereo peak envelope per track (same bin count as material waveform).
    std::array<std::array<float, kMaterialWaveformBins>, kNumTracks> mixBusWaveform {};
    // EQ band display 0..1 (0.5 = flat, mapped from +/-12 dB mix EQ gains).
    static constexpr int kMixEqBands = 3;
    std::array<std::array<float, kMixEqBands>, kNumTracks> mixEqBandNorm {};
    // Compressor gain-reduction meter 0..1 (smoothed, from MixBusStage).
    std::array<float, kNumTracks> mixCompReduction {};
};

} // namespace sculpt
