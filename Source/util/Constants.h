#pragma once

// Portable engine constants. No JUCE. No allocation. C++17 only.
namespace sculpt
{

constexpr int    kNumTracks            = 4;
constexpr int    kNumScenes            = 8;
constexpr int    kNumMacros            = 4;
constexpr int    kNumPages             = 7;

// Encoder slots on device pages (Material..Mixer) used for mod mapping grid.
constexpr int    kMaxModMappingEncoders  = 8;

// Device pages that appear in the modulation mapping grid (Material..Mixer).
// Keep in sync with Page enum order: Mod must be immediately after Mixer.
constexpr int    kModMapTargetPages    = 6;
// this are split by the Engine. All scratch buffers are sized from this.
constexpr int    kMaxBlockSize         = 2048;

// Fixed grain pool size per track. Never allocated during processing.
constexpr int    kGrainsPerTrack       = 24;

// Placeholder material length per track, in seconds.
constexpr float  kPlaceholderSeconds   = 2.0f;

// Maximum delay time held by the Space stage, in seconds.
constexpr float  kMaxSpaceSeconds      = 0.6f;

// Maximum capture buffer length per track, in seconds.
constexpr float  kMaxCaptureSeconds    = 8.0f;

constexpr double kDefaultSampleRate    = 44100.0;

// UI / message-thread waveform preview bins (Material page LCD). Not used on audio thread.
constexpr int    kMaterialWaveformBins = 256;

} // namespace sculpt
