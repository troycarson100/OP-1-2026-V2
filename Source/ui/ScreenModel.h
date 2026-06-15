#pragma once

#include <array>
#include <cstdint>
#include "PageModel.h"
#include "../core/GrainDisplaySlot.h"
#include "../core/ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

// Selected-track Euclidean / sync state for Granular LCD (filled in Engine::updateScreenModel).
struct GranularPatternSnapshot
{
    bool     syncOn        = false;
    int      divisionIndex = 0;
    int      steps         = 8;
    int      pulses        = 4;
    int      rotate        = 0;
    int      currentStep   = 0;
    uint16_t mask          = 0;
};

// Grain choreography preset (Torso-style coordinated offsets). Audio thread writes snapshot.
struct GrainPatternPresetSnapshot
{
    int                    patternActiveIndex = 0; // latched bank index 0..15
    int                    patternNameIndex   = 0; // same index for kGrainPatternUiNames
    float                  amount01           = 0.0f;
    int                    sequenceStep16     = 0; // 0..15 beat-grid step for the 16-step table
    std::array<float, 16> stepAccentNorm {};       // 0..1 LED row (from gain accent shape)
};

// Space page LCD: delay + reverb visualization state (audio thread writes each block).
struct SpaceVisualSnapshot
{
    // Delay
    float delaySeconds = 0.25f; // current delay time
    float delayFeedback01 = 0.3f;
    float delaySpread01 = 0.5f;  // <0.5 ping-pong, >0.5 diffusion
    float delayWet01 = 0.0f;     // smoothed audible level
    int   delayTimeMode = 3;     // 0 Straight,1 Dotted,2 Triplet,3 Free

    // Reverb
    float reverbSize01 = 0.5f;
    float reverbDecaySeconds = 2.0f;
    float reverbDamp01 = 0.4f;
    float reverbWet01 = 0.0f;    // smoothed audible level

    bool  frozen = false;
};

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

    // Granular: which 8-slot encoder bank the LCD mirrors (0 = core, 1 = sync/Euclidean/quant + patterns).
    uint8_t granularEncoderPage = 0;

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

    // Selected track: MaterialTimeMode 0..1 (>0.5 = Warp). Used for Tape Speed LCD readout.
    float selectedTrackMaterialTimeMode01 = 0.0f;
    // Selected track: TapeSpeedSnap (quantized tape speed knob). Not on the 8-slot LCD row.
    float selectedTrackTapeSpeedSnap01 = 0.0f;

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

    // Selected track: Root BPM as a whole number for the Material LCD cell (engine-written each block).
    int rootBpmWhole = 120;

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

    GranularPatternSnapshot granularPattern {};
    GrainPatternPresetSnapshot grainPatternPreset {};

    // Mod page: oscilloscope-style source preview (see ModLcdSnapshot).
    ModLcdSnapshot modLcd {};

    // Space page: delay + reverb visualization (see SpaceVisualSnapshot).
    SpaceVisualSnapshot space {};

    // Mixer page: post-mix-bus stereo peak envelope per track (same bin count as material waveform).
    std::array<std::array<float, kMaterialWaveformBins>, kNumTracks> mixBusWaveform {};
    // EQ band display 0..1 (0.5 = flat, mapped from +/-12 dB mix EQ gains).
    static constexpr int kMixEqBands = 3;
    std::array<std::array<float, kMixEqBands>, kNumTracks> mixEqBandNorm {};
    // Compressor gain-reduction meter 0..1 (smoothed, from MixBusStage).
    std::array<float, kNumTracks> mixCompReduction {};
};

} // namespace sculpt
