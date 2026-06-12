#pragma once

#include <algorithm>
#include <cmath>
#include "../util/MathUtils.h"

namespace sculpt
{

// Every engine parameter. Values are normalized 0..1 everywhere in the engine.
// Parameters at or after TrackLevel are per-track; everything before is global.
enum class ParameterId : int
{
    // ---- Global ----
    OutputGain = 0,
    SelectedTrack,
    Macro1,
    Macro2,
    Macro3,
    Macro4,

    // ---- Per-track ----
    TrackLevel,
    TrackPan,

    MaterialLevel,

    TapeSpeed,
    LoopStart,
    LoopEnd,

    GrainPosition,
    GrainSize,
    GrainDensity,
    GrainPitch,
    GrainSpray,
    GrainTexture,
    GrainSpread,
    GrainMix,

    FilterCutoff,
    FilterResonance,
    FilterMix,
    FilterMode,    // 0 = LPF/BP/HP (SVF), 1 = Spectral resonator bank
    FilterDecay,   // Spectral: pole radius / ring time. LPF: SVF type (0=LP, 0.5=BP, 1=HP)
    FilterPitch,   // Spectral: semitone transpose of resonator bank (0.5 = 0 semitones)
    FilterScale,   // Spectral: scale quantization (normalized index into FilterScale enum)

    ColorDrive,
    ColorTone,
    ColorMix,

    SpaceAmount,
    SpaceFeedback,
    SpaceMix,

    // > 0.5 = arm live input capture into this track's material buffer (plugin input bus).
    CaptureArm,

    // Spectral: tonic pitch class 0=C .. 11=B (normalized steps; use snap in Track).
    FilterKey,

    // Mix bus (after Space, before fader): 3-band tone EQ + stereo-linked compressor.
    MixEqLowGain,
    MixEqMidGain,
    MixEqHighGain,
    MixCompThreshold,
    MixCompMakeup,

    // Material LCD: horizontal zoom for waveform (0 = full buffer, 1 = max zoom).
    MaterialWaveZoom,
    // Material: manual tape playhead 0..1 (loop-clamped). Applied while stopped; zoom centers on this when not playing.
    MaterialPlayhead,

    // <=0.5 = tape-style speed (TapeSpeed knob); >0.5 = warp to host BPM using SampleRootBpm.
    MaterialTimeMode,
    // Musical BPM of the loaded loop before tempo warp (used in Warp mode only).
    SampleRootBpm,

    // When on, Tape Speed knob is quantized to 0, 0.25, 0.5, 0.75, 1 (normalized) for that track.
    TapeSpeedSnap,

    Count
};

constexpr int kNumParameters      = static_cast<int> (ParameterId::Count);
constexpr int kFirstTrackParam    = static_cast<int> (ParameterId::TrackLevel);
constexpr int kNumTrackParams     = kNumParameters - kFirstTrackParam;

constexpr bool isTrackParameter (ParameterId id)
{
    return static_cast<int> (id) >= kFirstTrackParam;
}

inline float parameterDefault (ParameterId id)
{
    switch (id)
    {
        case ParameterId::OutputGain:      return 0.8f;
        case ParameterId::SelectedTrack:   return 0.0f;
        case ParameterId::Macro1:
        case ParameterId::Macro2:
        case ParameterId::Macro3:
        case ParameterId::Macro4:          return 0.0f;
        case ParameterId::TrackLevel:      return 0.8f;
        case ParameterId::TrackPan:        return 0.5f;
        case ParameterId::MaterialLevel:   return 0.8f;
        case ParameterId::TapeSpeed:       return 0.5f;   // stopped (display 0, ratio 0)
        case ParameterId::LoopStart:       return 0.0f;
        case ParameterId::LoopEnd:         return 1.0f;
        case ParameterId::GrainPosition:   return 0.30f;
        case ParameterId::GrainSize:       return 0.40f;
        case ParameterId::GrainDensity:    return 0.45f;
        case ParameterId::GrainPitch:      return 0.5f;   // -> ratio 1.0
        case ParameterId::GrainSpray:      return 0.20f;
        case ParameterId::GrainTexture:    return 0.20f;
        case ParameterId::GrainSpread:     return 0.50f;
        case ParameterId::GrainMix:        return 0.50f;
        case ParameterId::FilterCutoff:    return 0.80f;
        case ParameterId::FilterResonance: return 0.20f;
        case ParameterId::FilterMix:       return 1.0f;
        case ParameterId::FilterMode:      return 0.0f;   // LPF by default
        case ParameterId::FilterDecay:     return 0.35f;  // moderate ring / LP type
        case ParameterId::FilterPitch:     return 0.5f;   // 0 semitones
        case ParameterId::FilterScale:     return 0.0f;   // Free
        case ParameterId::ColorDrive:      return 0.15f;
        case ParameterId::ColorTone:       return 0.5f;
        case ParameterId::ColorMix:        return 0.30f;
        case ParameterId::SpaceAmount:     return 0.25f;
        case ParameterId::SpaceFeedback:   return 0.35f;
        case ParameterId::SpaceMix:        return 0.25f;
        case ParameterId::CaptureArm:      return 0.0f;
        case ParameterId::FilterKey:       return 0.0f;   // C
        case ParameterId::MixEqLowGain:
        case ParameterId::MixEqMidGain:
        case ParameterId::MixEqHighGain:   return 0.5f;   // 0 dB (bipolar center)
        case ParameterId::MixCompThreshold: return 0.82f; // light compression by default
        case ParameterId::MixCompMakeup:    return 0.0f;  // unity makeup until dialed in
        case ParameterId::MaterialWaveZoom: return 0.0f;   // full waveform
        case ParameterId::MaterialPlayhead: return 0.0f;   // start of material
        case ParameterId::MaterialTimeMode: return 0.0f;   // tape mode
        case ParameterId::SampleRootBpm:   return 0.4f;    // ~120 BPM (40 + 0.4*200)
        case ParameterId::TapeSpeedSnap:   return 0.0f;   // off
        default:                           return 0.0f;
    }
}

// The single place that maps normalized 0..1 values to real units.
// Do not duplicate these conversions anywhere else.
namespace map
{
    inline float outputGain (float n)      { return n * n * 1.5f; }
    inline float trackGain (float n)       { return n * n * 1.5f; }
    inline float pan (float n)             { return n * 2.0f - 1.0f; }

    // -2x .. +2x, negative = reverse. Small dead zone around 0 avoids stalling.
    inline float tapeSpeedRatio (float n)
    {
        const float r = (n - 0.5f) * 4.0f;
        return std::fabs (r) < 0.02f ? 0.0f : r;
    }

    // LCD / readout: -100 (full reverse) .. 0 (stopped) .. +100 (full forward). Matches tapeSpeedRatio range.
    inline int tapeSpeedDisplay (float n)
    {
        const int x = static_cast<int> (std::lround ((clamp01 (n) - 0.5f) * 200.0f));
        return std::clamp (x, -100, 100);
    }

    // Warp mode: sample's original BPM (40 .. 240) from normalized knob.
    inline float sampleRootBpm (float n)
    {
        return 40.0f + clamp01 (n) * 200.0f;
    }

    // Warp mode only: varispeed multiplier around unity. n=0.5 -> 1.0; ends ~0.25x .. 4x.
    inline float warpVarispeedMultiplier (float n)
    {
        return std::pow (4.0f, 2.0f * (clamp01 (n) - 0.5f));
    }

    // Snap grid for normalized tape speed (0, 0.25, 0.5, 0.75, 1) — Tape mode only.
    inline float quantizeNormalizedQuarter (float n)
    {
        return std::round (clamp01 (n) * 4.0f) / 4.0f;
    }

    // Warp varispeed multiplier: snap to 0.25 steps (e.g. 1.0, 1.25, 1.5 …), clamp to curve range.
    inline float quantizeWarpVarispeedMultiplier (float m)
    {
        const float q = std::round (m / 0.25f) * 0.25f;
        return std::clamp (q, 0.25f, 4.0f);
    }

    // Grain length in seconds (normalized knob). Quadratic taper; max was 500 ms, now ~2 s (4x).
    inline float grainSizeSeconds (float n)  { return 0.02f + n * n * 1.92f; }   // ~20 ms .. ~2.0 s
    inline float grainDensityHz (float n)    { return 1.0f + n * n * 59.0f; }    // 1 .. 60 grains/s
    inline float grainPitchRatio (float n)   { return semitonesToRatio ((n - 0.5f) * 12.0f); } // +/- 12 semitones

    inline float filterCutoffHz (float n)    { return 40.0f * std::pow (2.0f, n * 8.3f); }        // ~40Hz .. ~12.6kHz
    inline float filterResonance (float n)   { return 0.5f + n * 9.0f; }                          // SVF Q
    // Spectral: pole radius r in [0.92, 0.9998] — controls ring/decay time.
    // r = 0.92 → ~3ms ring at 44100; r = 0.9998 → ~1.1s ring.
    inline float spectralPoleRadius (float n) { return 0.92f + n * 0.0798f; }
    // Spectral: semitone transpose [-24, +24].
    inline float filterPitchSemitones (float n) { return (n - 0.5f) * 48.0f; }

    inline float colorDriveGain (float n)    { return 1.0f + n * 9.0f; }
    inline float spaceFeedback (float n)     { return n * 0.85f; }
    inline float toneCutoffHz (float n)      { return 400.0f * std::pow (2.0f, n * 5.0f); }    // 400Hz .. ~12.8kHz

    // Mix EQ: +/- 12 dB per band; 0.5 = flat.
    inline float mixEqBandGainDb (float n)   { return (clamp01 (n) - 0.5f) * 24.0f; }

    // Mix compressor threshold (dBFS, peak detector). Higher = less compression.
    // Maps ~-45 .. +12 dBFS so fully clockwise is effectively "off" for normal program material.
    inline float mixCompThresholdDb (float n) { return -45.0f + clamp01 (n) * 57.0f; }

    // Makeup gain after compression, 0 .. +14 dB.
    inline float mixCompMakeupDb (float n)   { return clamp01 (n) * 14.0f; }

    // Material waveform: fraction of buffer width visible (1 = full, ~0.03 = max zoom).
    inline float materialWaveVisibleFraction (float zoom01)
    {
        const float z = clamp01 (zoom01);
        return std::max (0.03f, 1.0f - z * 0.97f);
    }
} // namespace map

inline const char* parameterName (ParameterId id)
{
    switch (id)
    {
        case ParameterId::OutputGain:      return "Output Gain";
        case ParameterId::SelectedTrack:   return "Selected Track";
        case ParameterId::Macro1:          return "Macro 1";
        case ParameterId::Macro2:          return "Macro 2";
        case ParameterId::Macro3:          return "Macro 3";
        case ParameterId::Macro4:          return "Macro 4";
        case ParameterId::TrackLevel:      return "Level";
        case ParameterId::TrackPan:        return "Pan";
        case ParameterId::MaterialLevel:   return "Material Level";
        case ParameterId::TapeSpeed:       return "Tape Speed";
        case ParameterId::LoopStart:       return "Loop Start";
        case ParameterId::LoopEnd:         return "Loop End";
        case ParameterId::GrainPosition:   return "Grain Position";
        case ParameterId::GrainSize:       return "Grain Size";
        case ParameterId::GrainDensity:    return "Grain Density";
        case ParameterId::GrainPitch:      return "Grain Pitch";
        case ParameterId::GrainSpray:      return "Grain Spray";
        case ParameterId::GrainTexture:    return "Grain Texture";
        case ParameterId::GrainSpread:     return "Grain Spread";
        case ParameterId::GrainMix:        return "Grain Mix";
        case ParameterId::FilterCutoff:    return "Filter Cutoff";
        case ParameterId::FilterResonance: return "Resonance";
        case ParameterId::FilterMix:       return "Filter Mix";
        case ParameterId::FilterMode:      return "Filter Mode";
        case ParameterId::FilterDecay:     return "Decay/Slope";
        case ParameterId::FilterPitch:     return "Pitch";
        case ParameterId::FilterScale:     return "Scale";
        case ParameterId::ColorDrive:      return "Color Drive";
        case ParameterId::ColorTone:       return "Color Tone";
        case ParameterId::ColorMix:        return "Color Mix";
        case ParameterId::SpaceAmount:     return "Space Amount";
        case ParameterId::SpaceFeedback:   return "Space Feedback";
        case ParameterId::SpaceMix:        return "Space Mix";
        case ParameterId::CaptureArm:      return "Input Capture";
        case ParameterId::FilterKey:       return "Key";
        case ParameterId::MixEqLowGain:    return "Mix Bass";
        case ParameterId::MixEqMidGain:    return "Mix Mid";
        case ParameterId::MixEqHighGain:   return "Mix Treble";
        case ParameterId::MixCompThreshold: return "Mix Comp Thr";
        case ParameterId::MixCompMakeup:   return "Mix Comp Gain";
        case ParameterId::MaterialWaveZoom: return "Wave Zoom";
        case ParameterId::MaterialPlayhead: return "Playhead";
        case ParameterId::MaterialTimeMode: return "Time Mode";
        case ParameterId::SampleRootBpm:   return "Root BPM";
        case ParameterId::TapeSpeedSnap:   return "Snap 1/4";
        default:                           return "Unknown";
    }
}

} // namespace sculpt
