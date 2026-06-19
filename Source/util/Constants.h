#pragma once

// Portable engine constants. No JUCE. No allocation. C++17 only.
namespace sculpt
{

constexpr int    kNumTracks            = 16;
constexpr int    kNumScenes            = 8;
constexpr int    kNumMacros            = 4;
constexpr int    kNumPages             = 7;

// Step sequencer (Digitakt-style): 32 steps shown as 16 buttons over 2 pages.
constexpr int    kNumSteps             = 32;
constexpr int    kStepsPerPage         = 16;
constexpr int    kNumSeqPatterns       = 16;   // pattern bank size

// Encoder slots on device pages (Material..Mixer) used for mod mapping grid.
constexpr int    kMaxModMappingEncoders  = 8;

// Pages that appear in the modulation mapping grid: the six device pages (Material..Mixer)
// plus the Mod page (index 6 == Page::Mod), whose encoders target other mod slots' Amount/Rate.
// Keep in sync with Page enum order: Mod is immediately after Mixer.
constexpr int    kModMapTargetPages    = 7;
// this are split by the Engine. All scratch buffers are sized from this.
constexpr int    kMaxBlockSize         = 2048;

// Fixed grain pool size per track. Never allocated during processing.
constexpr int    kGrainsPerTrack       = 24;

// Placeholder material length per track, in seconds.
constexpr float  kPlaceholderSeconds   = 2.0f;

// Space / Vast: max delay line (free + synced musical divisions at low BPM).
constexpr float  kMaxSpaceDelaySeconds = 4.0f;
// FDN delay lines sized for ~48 kHz; prepare() may scale read positions for other SR.
constexpr int    kSpaceFdnNumLines     = 4;

// Maximum capture buffer length per track, in seconds.
constexpr float  kMaxCaptureSeconds    = 8.0f;

// Maximum length when importing a file from disk (message thread). Keeps RAM bounded;
// material buffer grows to the resampled length up to this cap per track.
constexpr float  kMaxImportSeconds     = 1800.0f; // 30 minutes

constexpr double kDefaultSampleRate    = 44100.0;

// UI / message-thread waveform preview bins (Material page LCD). Not used on audio thread.
constexpr int    kMaterialWaveformBins = 256;

} // namespace sculpt
