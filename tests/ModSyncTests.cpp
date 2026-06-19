// Portable engine tests: modulator tempo-sync. A synced Random mod must be driven by the beat
// (changes as the beat advances, freezes when the beat freezes); the free mode runs regardless.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "modulation/ModEngine.h"
#include "modulation/ModTypes.h"
#include "core/ParameterState.h"
#include "core/ParameterIds.h"
#include "ui/PageModel.h"

using namespace sculpt;

namespace
{
constexpr double kSr  = 44100.0;
constexpr double kBpm = 120.0;
constexpr int    kBlock = 512;

// Map slot 0 -> Filter Cutoff (Page::Filter encoder 0) so we can read the source via the mod offset.
ModPatch makeRandomPatch (uint8_t sync, uint8_t division)
{
    ModPatch p {};
    auto& sp = p.slots[0][0];
    sp.kind          = ModulatorKind::Random;
    sp.randomSync    = sync;
    sp.randomDivision = division;
    sp.randomAmount  = 1.0f;
    sp.randomSlew01  = 0.0f;               // minimal slew -> snappy steps
    sp.randomRateHz  = 4.0f;               // used only in free mode
    auto& cell = p.maps[0][0][static_cast<size_t> (Page::Filter)][0];
    cell.depth   = 1.0f;
    cell.bipolar = 1;                      // pass the signed source straight through
    return p;
}

// Total variation of the cutoff mod offset over `blocks`; `advanceBeat` chooses beat-driven vs frozen.
float runTotalVariation (uint8_t sync, uint8_t division, bool advanceBeat, int blocks, int skip)
{
    ModEngine eng;
    eng.prepare (kSr);
    eng.setLivePatch (makeRandomPatch (sync, division));

    ParameterState params;
    params.resetToDefaults();

    const double beatPerBlock = static_cast<double> (kBlock) * kBpm / (60.0 * kSr);
    double beat = 0.0;
    std::vector<float> vals;
    vals.reserve (static_cast<size_t> (blocks));
    for (int i = 0; i < blocks; ++i)
    {
        params.clearModOffsets();
        eng.apply (params, 0.0f, kBlock, beat, kBpm, kSr);
        vals.push_back (params.getTrackModOffset (0, ParameterId::FilterCutoff));
        if (advanceBeat)
            beat += beatPerBlock;
    }

    float tv = 0.0f;
    for (size_t i = static_cast<size_t> (skip) + 1; i < vals.size(); ++i)
        tv += std::fabs (vals[i] - vals[i - 1]);
    return tv;
}
} // namespace

int main()
{
    // ~9 beats of blocks.
    constexpr int kBlocks = 400;
    constexpr int kSkip   = 100; // ignore initial settling

    // 1) Synced + beat advancing -> beat boundaries pick new targets, so the value moves.
    const float syncedMoving = runTotalVariation (/*sync*/ 1, /*div 1/4*/ 8, /*advance*/ true,  kBlocks, kSkip);
    assert (syncedMoving > 0.5f);

    // 2) Synced + beat FROZEN -> no boundary is ever crossed, so it must freeze (this is the bug
    //    under test: a "synced" mod that keeps moving with a frozen beat is actually free-running).
    const float syncedFrozen = runTotalVariation (/*sync*/ 1, 8, /*advance*/ false, kBlocks, kSkip);
    assert (syncedFrozen < 1.0e-4f);

    // 3) Free + beat frozen -> runs on its own clock regardless of the beat.
    const float freeFrozen = runTotalVariation (/*sync*/ 0, 8, /*advance*/ false, kBlocks, kSkip);
    assert (freeFrozen > 0.5f);

    // 4) Finer division changes faster than a coarser one over the same beat span.
    const float fast = runTotalVariation (/*sync*/ 1, /*1/16*/ 4,  true, kBlocks, kSkip); // c=4
    const float slow = runTotalVariation (/*sync*/ 1, /*1 bar*/ 12, true, kBlocks, kSkip); // c=0.25
    assert (fast > slow);

    std::printf ("ModSync tests passed (syncedMoving=%.3f syncedFrozen=%.5f freeFrozen=%.3f fast=%.3f slow=%.3f)\n",
                 syncedMoving, syncedFrozen, freeFrozen, fast, slow);
    return 0;
}
