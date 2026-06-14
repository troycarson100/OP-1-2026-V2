#pragma once

#include <array>
#include <cstdint>

#include "../core/ParameterIds.h"
#include "../util/Constants.h"

namespace sculpt
{

constexpr int kModSlotsPerTrack = 4;

// Stored in plugin state / edited on Mod page. Plain POD.
enum class ModulatorKind : uint8_t
{
    Off = 0,
    Wave,
    Random,
    ADSR,
    InputEnvelope
};

// Input-envelope routing (ModPatchSlotParams::inputEnvSource).
constexpr uint8_t kInputEnvRouteMainInput     = 4;  // plugin main input bus
constexpr uint8_t kInputEnvRouteHostSidechain = 5;  // host sidechain bus (when connected)
constexpr uint8_t kInputEnvRouteTrackBase    = 10; // follow track t: value == 10 + t (t in 0..kNumTracks-1)

inline float inputEnvelopeLevelForRoute (uint8_t route, float main01, float side01,
                                         const std::array<float, kNumTracks>& track01)
{
    if (route == kInputEnvRouteMainInput)
        return main01;
    if (route == kInputEnvRouteHostSidechain)
        return side01;
    if (route >= kInputEnvRouteTrackBase && route < kInputEnvRouteTrackBase + static_cast<uint8_t> (kNumTracks))
        return track01[static_cast<size_t> (route - kInputEnvRouteTrackBase)];
    if (route < 2)
        return (route == 0 ? main01 : side01);
    return main01;
}

struct ModPatchSlotParams
{
    ModulatorKind kind = ModulatorKind::Off;

    float waveRateHz    = 0.35f;
    float waveAmount    = 1.0f;   // 0..1 depth; UI shows 0..100%
    // 0..1 phase shaping: 0.5 = linear (no bend), <0.5 skews earlier in cycle, >0.5 skews later.
    float waveBend01    = 0.5f;
    float wavePhase01  = 0.0f;
    float waveOffset01  = 0.5f;
    uint8_t waveShape   = 0;      // LFO::Shape index (see LFO.h)
    uint8_t waveSync    = 0;      // 0 = Free Hz, 1 = sync to division
    uint8_t waveDivision = 8;     // index into sync table (default 1/4)

    float randomRateHz  = 2.5f;
    float randomSlew01  = 0.5f;
    uint8_t randomSync   = 0;
    uint8_t randomDivision = 8;

    float adsrAttackSec  = 0.02f;
    float adsrDecaySec   = 0.08f;
    float adsrSustain01  = 0.65f;
    float adsrReleaseSec = 0.12f;

    // When kind == InputEnvelope: which signal the follower tracks (kInputEnvRoute*).
    // Legacy blobs used 0/1 as sidechain toggle; migrate on load (PluginProcessor).
    uint8_t inputEnvSource = kInputEnvRouteMainInput;
    uint8_t padInputEnv[3] = {};
};

struct ModMappingDepth
{
    float    depth   = 0.0f;    // amount; uni uses 0..1, bi uses -1..1 in UI
    uint8_t  bipolar = 0;       // 0 = unipolar (0..1 shaped source), 1 = bipolar (signed source)
    uint8_t  pad[3]  = { 0, 0, 0 };
};

// Full modulation configuration: per-track slot sources + per-target-page mapping grid.
struct ModPatch
{
    std::array<std::array<ModPatchSlotParams, kModSlotsPerTrack>, kNumTracks> slots {};

    // [track][modSlot][devicePage 0..kModMapTargetPages-1][encoder 0..7]
    std::array<std::array<std::array<std::array<ModMappingDepth, kMaxModMappingEncoders>, kModMapTargetPages>, kModSlotsPerTrack>, kNumTracks> maps {};
};

} // namespace sculpt
