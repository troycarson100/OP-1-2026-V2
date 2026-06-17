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
    float randomAmount  = 1.0f;   // 0..1 depth; UI shows 0..100%
    uint8_t randomSync   = 0;
    uint8_t randomDivision = 8;

    float adsrAttackSec  = 0.02f;
    float adsrDecaySec   = 0.08f;
    float adsrSustain01  = 0.65f;
    float adsrReleaseSec = 0.12f;
};

// Modulatable parameters of a mod slot, exposed as the encoders of the "Mod" target page so a
// slot can modulate another slot. Encoder layout: slot = enc/2, target = enc%2.
enum class ModSlotTarget : uint8_t { Amount = 0, Rate = 1 };

inline bool modSlotTargetForEncoder (int encoder, int& slot, ModSlotTarget& target)
{
    if (encoder < 0 || encoder >= 2 * kModSlotsPerTrack)
        return false;
    slot   = encoder / 2;
    target = (encoder % 2 == 0) ? ModSlotTarget::Amount : ModSlotTarget::Rate;
    return true;
}

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
