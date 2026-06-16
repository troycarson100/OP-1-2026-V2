#pragma once

#include <algorithm>
#include <cmath>
#include "../util/Constants.h"
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

    // >0.5 = synced (musical division + Euclidean); <=0.5 = free Hz cloud (default).
    GrainSync,
    GrainSteps,
    GrainPulses,
    GrainRotate,
    // 0 = continuous pitch; stepped scale indices for synced quantization.
    GrainPitchQuant,
    // Stepped preset bank index (see map::grainPatternIndex).
    GrainPattern,
    // 0 = pattern bypass (Phase-2-equivalent offsets); 1 = full relative pattern offsets.
    GrainPatternAmount,

    // S-4-inspired grain timbre controls.
    // Envelope shape: 0 = Hann bell (smooth), 1 = percussive (fast attack, exp decay).
    GrainContour,
    // Per-grain reverse probability: 0 = never reversed, 1 = always reversed.
    GrainRandRev,
    // Per-grain amplitude randomization depth: 0 = flat, 1 = up to -75% random reduction.
    GrainRandAmp,

    FilterCutoff,
    FilterResonance,
    FilterMix,
    FilterMode,    // 0 = LPF/BP/HP (SVF), 1 = Spectral resonator bank
    FilterDecay,   // Spectral: pole radius / ring time. LPF: SVF type (0=LP, 0.5=BP, 1=HP)
    FilterPitch,   // Spectral: semitone transpose of resonator bank (0.5 = 0 semitones)
    FilterScale,   // Spectral: scale quantization (normalized index into FilterScale enum)

    ColorDrive,
    ColorCrush,
    ColorTilt,
    ColorCompress,
    ColorNoise,
    ColorNoiseDecay,
    ColorNoiseTone,
    ColorWet,

    // Space (Vast-style): delay + hall reverb. Time mode: Straight / Dotted / Triplet / Free (choice).
    SpaceDelayAmount,
    SpaceDelayTime,
    SpaceReverbAmount,
    SpaceReverbSize,
    SpaceDelayFeedback,
    SpaceSpread,
    SpaceDamp,
    SpaceReverbDecay,
    SpaceDelayTimeMode,
    // > 0.5: hold delay+reverb buffers in self-sustain (no new dry input).
    SpaceFreeze,

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
    // When on, Loop Start / Loop End snap to a fixed grid (see map::snapLoopPair01).
    LoopSnapGrid,

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
        case ParameterId::GrainSpray:      return 0.16f;
        case ParameterId::GrainTexture:    return 0.16f;
        case ParameterId::GrainSpread:     return 0.50f;
        case ParameterId::GrainMix:        return 0.50f;
        case ParameterId::GrainSync:       return 0.0f;   // Free
        case ParameterId::GrainSteps:      return 0.5f;   // ~8 steps
        case ParameterId::GrainPulses:     return 0.5f;
        case ParameterId::GrainRotate:     return 0.0f;
        case ParameterId::GrainPitchQuant: return 0.0f;   // off
        case ParameterId::GrainPattern:     return 0.0f;   // Flat
        case ParameterId::GrainPatternAmount: return 0.0f; // bypass
        case ParameterId::GrainContour:    return 0.0f;   // Hann (smooth)
        case ParameterId::GrainRandRev:    return 0.0f;   // no reversals
        case ParameterId::GrainRandAmp:    return 0.0f;   // flat amplitude
        case ParameterId::FilterCutoff:    return 0.80f;
        case ParameterId::FilterResonance: return 0.20f;
        case ParameterId::FilterMix:       return 1.0f;
        case ParameterId::FilterMode:      return 0.0f;   // LPF by default
        case ParameterId::FilterDecay:     return 0.35f;  // moderate ring / LP type
        case ParameterId::FilterPitch:     return 0.5f;   // 0 semitones
        case ParameterId::FilterScale:     return 0.0f;   // Free
        case ParameterId::ColorDrive:      return 0.15f;
        case ParameterId::ColorCrush:      return 0.0f;
        case ParameterId::ColorTilt:       return 0.5f;   // center = equal low/high bands
        case ParameterId::ColorCompress:   return 0.0f;
        case ParameterId::ColorNoise:      return 0.0f;
        case ParameterId::ColorNoiseDecay: return 0.35f;  // ~300ms decay
        case ParameterId::ColorNoiseTone:  return 0.5f;   // ~1kHz
        case ParameterId::ColorWet:        return 0.30f;
        case ParameterId::SpaceDelayAmount:   return 0.25f;
        case ParameterId::SpaceDelayTime:     return 0.35f;
        case ParameterId::SpaceReverbAmount:  return 0.20f;
        case ParameterId::SpaceReverbSize:    return 0.45f;
        case ParameterId::SpaceDelayFeedback: return 0.30f;
        case ParameterId::SpaceSpread:        return 0.50f;
        case ParameterId::SpaceDamp:          return 0.40f;
        case ParameterId::SpaceReverbDecay:   return 0.45f;
        case ParameterId::SpaceDelayTimeMode: return 1.0f; // Free (last choice index)
        case ParameterId::SpaceFreeze:        return 0.0f;
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
        case ParameterId::SampleRootBpm:   return 0.4f;    // 120 BPM (40 + 0.4*200)
        case ParameterId::TapeSpeedSnap:   return 0.0f;   // off
        case ParameterId::LoopSnapGrid:    return 0.0f;   // off
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

    // Warp mode: sample's original BPM (40 .. 240), whole numbers only.
    inline float sampleRootBpm (float n)
    {
        const float x = 40.0f + clamp01 (n) * 200.0f;
        const int   b = std::clamp (static_cast<int> (std::lround (static_cast<double> (x))), 40, 240);
        return static_cast<float> (b);
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

    // Material loop in/out: snap to 1/64 grid in normalized 0..1 (64 steps including endpoints).
    inline float snapLoopToGrid01 (float n01)
    {
        constexpr int kSteps = 64;
        return std::round (clamp01 (n01) * static_cast<float> (kSteps)) / static_cast<float> (kSteps);
    }

    inline void snapLoopPair01 (float& loopStart01, float& loopEnd01)
    {
        loopStart01 = snapLoopToGrid01 (loopStart01);
        loopEnd01   = snapLoopToGrid01 (loopEnd01);
        if (loopEnd01 < loopStart01 + 0.01f)
            loopEnd01 = clampf (loopStart01 + 0.01f, 0.0f, 1.0f);
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

    // Synced mode: beats between grain hits (musical interval). Table order: whole .. 1/32 with dotted/triplets.
    inline int grainRateDivisionTableIndex (float n01)
    {
        constexpr int kTableLen = 14;
        const float   n       = clamp01 (n01);
        const int     idx     = static_cast<int> (std::lround (n * static_cast<float> (kTableLen - 1)));
        return std::clamp (idx, 0, kTableLen - 1);
    }

    // Beats per spawn period (larger = slower).
    inline double grainRateDivisionBeats (float n01)
    {
        // Order: 4, 2, 1, 3/4, 1/2, 3/8, 1/3, 1/4, 3/16, 1/6, 1/8, 1/12, 1/16, 1/32
        static constexpr double kDivBeats[14] = {
            4.0, 2.0, 1.0, 0.75, 0.5, 0.375, 1.0 / 3.0, 0.25, 0.1875, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625, 1.0 / 32.0
        };
        return kDivBeats[static_cast<size_t> (grainRateDivisionTableIndex (n01))];
    }

    inline int grainSteps (float n01)
    {
        return 1 + static_cast<int> (std::lround (clamp01 (n01) * 15.0f));
    }

    inline int grainPulses (float n01, int steps)
    {
        steps = std::clamp (steps, 1, 16);
        const int span = steps - 1;
        if (span <= 0)
            return 1;
        return 1 + static_cast<int> (std::lround (clamp01 (n01) * static_cast<float> (span)));
    }

    inline int grainRotate (float n01, int steps)
    {
        steps = std::clamp (steps, 1, 16);
        if (steps <= 1)
            return 0;
        const int span = steps - 1;
        return static_cast<int> (std::lround (clamp01 (n01) * static_cast<float> (span)));
    }

    // Max humanization offset in samples for synced texture (full knob = map constant in seconds).
    inline float grainSyncHumanizeMaxSamples (double sampleRate)
    {
        const double sr = sampleRate > 1.0e-6 ? sampleRate : 44100.0;
        return static_cast<float> (0.005 * sr);
    }

    // 16 curated grain choreography presets (see GrainPatternBank.h).
    inline int grainPatternIndex (float n01)
    {
        return std::clamp (static_cast<int> (std::lround (clamp01 (n01) * 15.0f)), 0, 15);
    }

    // Pattern per-step gain accent scaled by GrainPatternAmount (linear dB contribution).
    inline float grainPatternGainDbEffective (float gainDbStep, float amount01)
    {
        return gainDbStep * clamp01 (amount01);
    }

    // Eased 0..1 depth for pattern choreography: more musical motion in the first half of the knob.
    inline float grainPatternDepth01 (float n01)
    {
        return std::sqrt (clamp01 (n01));
    }

    inline float filterCutoffHz (float n)    { return 40.0f * std::pow (2.0f, n * 8.3f); }        // ~40Hz .. ~12.6kHz
    inline float filterResonance (float n)   { return 0.5f + n * 9.0f; }                          // SVF Q
    // Spectral: pole radius r in [0.92, 0.9998] — controls ring/decay time.
    // r = 0.92 → ~3ms ring at 44100; r = 0.9998 → ~1.1s ring.
    inline float spectralPoleRadius (float n) { return 0.92f + n * 0.0798f; }
    // Spectral: semitone transpose [-24, +24].
    inline float filterPitchSemitones (float n) { return (n - 0.5f) * 48.0f; }

    // Drive: 1x to 16x gain for tanh waveshaper.
    inline float colorDriveGain (float n)    { return 1.0f + n * 15.0f; }

    // Tilt: per-band gain for the dual-band crossover (crossover at ~650 Hz).
    // n < 0.5 emphasises the high band; n > 0.5 emphasises the low band.
    inline float colorTiltLowGain (float n)  { return n < 0.5f ? 0.2f + n * 1.6f : 1.0f; }
    inline float colorTiltHighGain (float n) { return n > 0.5f ? 0.2f + (1.0f - n) * 1.6f : 1.0f; }

    // Noise envelope decay: 5 ms .. 2000 ms.
    inline float colorNoiseDecayMs (float n) { return 5.0f + n * n * 1995.0f; }

    // Noise bandpass centre frequency: 100 Hz .. 8000 Hz.
    inline float colorNoiseCutoffHz (float n) { return 100.0f * std::pow (80.0f, clamp01 (n)); }

    // Delay feedback gain 0 .. ~0.92 (stability margin).
    inline float spaceDelayFeedbackGain (float n) { return clamp01 (n) * 0.92f; }

    // Free-mode delay time (seconds), ~2 ms .. kMaxSpaceDelaySeconds.
    inline float spaceDelayTimeFreeSeconds (float n)
    {
        const float mn = 0.002f;
        const float mx = kMaxSpaceDelaySeconds;
        const float t  = clamp01 (n);
        return mn * std::pow (mx / mn, t);
    }

    // Reverb RT60 (seconds), ~0.3 .. 30 s from decay knob (long, lush tails).
    inline float spaceReverbRt60Seconds (float n)
    {
        return 0.3f + std::pow (clamp01 (n), 2.0f) * 29.7f;
    }

    // Damping: one-pole cutoff Hz on feedback paths (delay + reverb).
    inline float spaceDampCutoffHz (float n)
    {
        return 900.0f + clamp01 (n) * clamp01 (n) * 15000.0f;
    }

    // Reverb size: scales plate delay lengths (0.5 .. 2.2 multiplier) for a much bigger space.
    inline float spaceReverbSizeLineScale (float n) { return 0.5f + clamp01 (n) * 1.7f; }

    // SpaceDelayTimeMode: 0 Straight, 1 Dotted, 2 Triplet, 3 Free (choice index).
    inline int spaceDelayTimeModeIndex (float n01)
    {
        return std::clamp (static_cast<int> (std::lround (clamp01 (n01) * 3.0f)), 0, 3);
    }

    // Delay synced time uses a clean straight base division; the Time Mode knob then
    // applies straight / dotted / triplet. Keeps the readout unambiguous (no stray "T").
    inline int spaceDelayDivisionIndex (float n01)
    {
        constexpr int kN = 6;
        return std::clamp (static_cast<int> (std::lround (clamp01 (n01) * static_cast<float> (kN - 1))), 0, kN - 1);
    }

    inline double spaceDelayDivisionBeats (int idx)
    {
        static constexpr double kBeats[6] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 }; // 1/1 .. 1/32
        return kBeats[static_cast<size_t> (std::clamp (idx, 0, 5))];
    }

    inline const char* spaceDelayDivisionLabel (int idx)
    {
        static const char* kLabel[6] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        return kLabel[static_cast<size_t> (std::clamp (idx, 0, 5))];
    }

    // Musical delay: straight base division scaled by the time mode (dotted / triplet).
    inline double spaceSyncedDelayBeats (float timeKnob01, int modeIdx)
    {
        double b = spaceDelayDivisionBeats (spaceDelayDivisionIndex (timeKnob01));
        if (modeIdx == 1) // Dotted
            b *= 1.5;
        else if (modeIdx == 2) // Triplet
            b *= (2.0 / 3.0);
        return std::max (1.0 / 128.0, b);
    }
    // Mix EQ: +/- 12 dB per band; 0.5 = flat.
    inline float mixEqBandGainDb (float n)   { return (clamp01 (n) - 0.5f) * 24.0f; }

    // Mix compressor threshold (dBFS, peak detector). Higher = less compression.
    // Maps ~-40 .. +6 dBFS so the knob stays in a useful range for normalized / post-EQ peaks;
    // fully clockwise (~+6 dB) is effectively "off" with the compOff bypass rule (thrDb >= 1.5).
    inline float mixCompThresholdDb (float n) { return -40.0f + clamp01 (n) * 46.0f; }

    // Makeup gain after compression (0 .. +14 dB).
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
        case ParameterId::GrainSync:       return "Grain Sync";
        case ParameterId::GrainSteps:      return "Grain Steps";
        case ParameterId::GrainPulses:     return "Grain Pulses";
        case ParameterId::GrainRotate:     return "Grain Rotate";
        case ParameterId::GrainPitchQuant: return "Grain Pitch Q";
        case ParameterId::GrainPattern:     return "Grain Pattern";
        case ParameterId::GrainPatternAmount: return "Pattern Amt";
        case ParameterId::GrainContour:    return "Contour";
        case ParameterId::GrainRandRev:    return "Rand Rev";
        case ParameterId::GrainRandAmp:    return "Rand Amp";
        case ParameterId::FilterCutoff:    return "Filter Cutoff";
        case ParameterId::FilterResonance: return "Resonance";
        case ParameterId::FilterMix:       return "Filter Mix";
        case ParameterId::FilterMode:      return "Filter Mode";
        case ParameterId::FilterDecay:     return "Decay/Slope";
        case ParameterId::FilterPitch:     return "Pitch";
        case ParameterId::FilterScale:     return "Scale";
        case ParameterId::ColorDrive:      return "Drive";
        case ParameterId::ColorCrush:      return "Crush";
        case ParameterId::ColorTilt:       return "Tilt";
        case ParameterId::ColorCompress:   return "Compress";
        case ParameterId::ColorNoise:      return "Noise";
        case ParameterId::ColorNoiseDecay: return "Noise Decay";
        case ParameterId::ColorNoiseTone:  return "Noise Tone";
        case ParameterId::ColorWet:        return "Wet";
        case ParameterId::SpaceDelayAmount:   return "Delay";
        case ParameterId::SpaceDelayTime:     return "Time";
        case ParameterId::SpaceReverbAmount:  return "Reverb";
        case ParameterId::SpaceReverbSize:    return "Size";
        case ParameterId::SpaceDelayFeedback: return "Fdbk";
        case ParameterId::SpaceSpread:        return "Spread";
        case ParameterId::SpaceDamp:          return "Damp";
        case ParameterId::SpaceReverbDecay: return "Decay";
        case ParameterId::SpaceDelayTimeMode: return "Time Mode";
        case ParameterId::SpaceFreeze:        return "Freeze";
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
        case ParameterId::LoopSnapGrid:    return "Loop Snap";
        default:                           return "Unknown";
    }
}

} // namespace sculpt
