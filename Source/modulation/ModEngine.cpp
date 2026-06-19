#include <algorithm>

#include "ModEngine.h"
#include "LFO.h"
#include "SyncDivision.h"
#include "../ui/PageModel.h"
#include "../ui/ScreenModel.h"
#include "../util/Constants.h"
#include "../util/MathUtils.h"

namespace sculpt
{
namespace
{
    LFO::Shape shapeFromPatchIndex (uint8_t i)
    {
        const auto n = static_cast<uint8_t> (LFO::Shape::Count);
        return static_cast<LFO::Shape> (i < n ? i : 0);
    }

    float waveDepth01 (float stored)
    {
        return clamp01 (stored);
    }
} // namespace

void ModEngine::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            lfos_[static_cast<size_t> (t)][static_cast<size_t> (s)].prepare (sr);
            rnds_[static_cast<size_t> (t)][static_cast<size_t> (s)].prepare (sr);
            adsrs_[static_cast<size_t> (t)][static_cast<size_t> (s)].prepare (sr);
            rnds_[static_cast<size_t> (t)][static_cast<size_t> (s)].setSeed (
                0xC001u ^ static_cast<uint32_t> (t * 17 + s * 0x9E37u));
        }
    }
}

void ModEngine::reset()
{
    pendingAdsrTrig_.store (0);
    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            lfos_[static_cast<size_t> (t)][static_cast<size_t> (s)].reset();
            rnds_[static_cast<size_t> (t)][static_cast<size_t> (s)].reset();
            adsrs_[static_cast<size_t> (t)][static_cast<size_t> (s)].reset();
            prevSrc_[static_cast<size_t> (t)][static_cast<size_t> (s)] = 0.0f;
        }
    }
}

void ModEngine::triggerAdsr (int track, int slot)
{
    if (track < 0 || track >= kNumTracks || slot < 0 || slot >= kModSlotsPerTrack)
        return;
    const uint64_t bit = 1ull << static_cast<uint64_t> (track * kModSlotsPerTrack + slot);
    pendingAdsrTrig_.fetch_or (bit, std::memory_order_relaxed);
}

float ModEngine::sourceValue (int track, int slot, float inputEnv01, int numSamples,
                              double beatAtBlockStart, double beatAtBlockEnd,
                              float amountOffset, float rateOffset)
{
    const auto& sp = live_.slots[static_cast<size_t> (track)][static_cast<size_t> (slot)];
    // rateOffset (a -1..1-ish mod sum) scaled to a Hz delta added to free-rate sources.
    const float rateHzDelta = rateOffset * 8.0f;
    switch (sp.kind)
    {
        case ModulatorKind::Off:
            return 0.0f;
        case ModulatorKind::Wave:
        {
            const LFO::Shape sh = shapeFromPatchIndex (sp.waveShape);
            const float      depth = waveDepth01 (clamp01 (sp.waveAmount + amountOffset));
            float            bipolar = 0.0f;

            if (sp.waveSync != 0)
            {
                const int divIdx = static_cast<int> (sp.waveDivision);
                const double c = syncDivisionCyclesPerBeat (divIdx);
                const double phRaw = wrapPhase01 (beatAtBlockStart * c + static_cast<double> (sp.wavePhase01));
                const double ph     = LFO::bentPhase01 (phRaw, sp.waveBend01);
                bipolar = LFO::valueForShape (sh, ph) * depth;
            }
            else
            {
                auto& lfo = lfos_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
                lfo.setRateHz (std::clamp (sp.waveRateHz + rateHzDelta, 0.02f, 18.0f));
                lfo.setShape (sh);
                lfo.setBend01 (sp.waveBend01);
                lfo.update (numSamples);
                bipolar = lfo.getValue() * depth;
            }
            return bipolar + (sp.waveOffset01 - 0.5f) * 2.0f;
        }
        case ModulatorKind::Random:
        {
            auto& r = rnds_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
            if (sp.randomSync != 0)
            {
                const int divIdx = static_cast<int> (sp.randomDivision);
                const double c = syncDivisionCyclesPerBeat (divIdx);
                r.updateSync (beatAtBlockStart, beatAtBlockEnd, c, sp.randomSlew01);
            }
            else
            {
                r.setRateHz (std::clamp (sp.randomRateHz + rateHzDelta, 0.1f, 24.0f));
                r.setSlew (sp.randomSlew01);
                r.update (numSamples);
            }
            return r.getValue() * clamp01 (sp.randomAmount + amountOffset);
        }
        case ModulatorKind::ADSR:
        {
            auto& e = adsrs_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
            e.setParams (sp.adsrAttackSec, sp.adsrDecaySec, sp.adsrSustain01, sp.adsrReleaseSec);
            e.process (numSamples);
            return e.getValue() * 2.0f - 1.0f;
        }
        case ModulatorKind::InputEnvelope:
        default:
        {
            const float u = clamp01 (inputEnv01);
            return u * 2.0f - 1.0f;
        }
    }
}

void ModEngine::apply (ParameterState& params, float inputEnv01, int numSamples,
                       double beatAtBlockStart, double bpm, double sampleRate)
{
    const double beatDelta = (bpm > 0.0 && sampleRate > 0.0 && numSamples > 0)
                                 ? static_cast<double> (numSamples) * (bpm / (60.0 * sampleRate))
                                 : 0.0;
    const double beatAtBlockEnd = beatAtBlockStart + beatDelta;

    const uint64_t trig = pendingAdsrTrig_.exchange (0, std::memory_order_acq_rel);
    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            if (trig & (1ull << static_cast<uint64_t> (t * kModSlotsPerTrack + s)))
                adsrs_[static_cast<size_t> (t)][static_cast<size_t> (s)].trigger();
        }
    }

    const int modPageIdx = static_cast<int> (Page::Mod);

    for (int t = 0; t < kNumTracks; ++t)
    {
        // 1. Mod-slot targeting: accumulate each slot's Amount/Rate offset from other slots.
        //    Uses the previous block's source values so the result is independent of slot order
        //    and stable under feedback (one-block latency on slot->slot modulation).
        float amtOff[kModSlotsPerTrack]  = { 0.0f };
        float rateOff[kModSlotsPerTrack] = { 0.0f };
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            if (live_.slots[static_cast<size_t> (t)][static_cast<size_t> (s)].kind == ModulatorKind::Off)
                continue;
            const float srcPrev = prevSrc_[static_cast<size_t> (t)][static_cast<size_t> (s)];
            const float uniPrev = srcPrev * 0.5f + 0.5f;
            for (int enc = 0; enc < kMaxModMappingEncoders; ++enc)
            {
                int dstSlot = 0; ModSlotTarget tgt {};
                if (! modSlotTargetForEncoder (enc, dstSlot, tgt))
                    continue;
                const auto& d = live_.maps[static_cast<size_t> (t)][static_cast<size_t> (s)][static_cast<size_t> (modPageIdx)][static_cast<size_t> (enc)];
                if (d.depth == 0.0f)
                    continue;
                const float delta = (d.bipolar != 0) ? (srcPrev * d.depth) : (uniPrev * d.depth);
                if (tgt == ModSlotTarget::Amount)
                    amtOff[dstSlot]  += delta;
                else
                    rateOff[dstSlot] += delta;
            }
        }

        // 2. Compute each slot's source value (with its Amount/Rate offset) and route to device params.
        float curSrc[kModSlotsPerTrack] = { 0.0f };
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            if (live_.slots[static_cast<size_t> (t)][static_cast<size_t> (s)].kind == ModulatorKind::Off)
                continue;

            const float src = sourceValue (t, s, inputEnv01, numSamples,
                                           beatAtBlockStart, beatAtBlockEnd,
                                           amtOff[s], rateOff[s]);
            curSrc[s] = src;
            const float uniShape = src * 0.5f + 0.5f;

            for (int pg = 0; pg < kModMapTargetPages; ++pg)
            {
                if (pg == modPageIdx)
                    continue; // mod-slot targets handled in pass 1
                const auto page = static_cast<Page> (pg);
                for (int enc = 0; enc < kMaxModMappingEncoders; ++enc)
                {
                    const ParameterId id = PageModel::parameterForSlot (page, enc);
                    if (id == ParameterId::Count)
                        continue;

                    const auto& d = live_.maps[static_cast<size_t> (t)][static_cast<size_t> (s)][static_cast<size_t> (pg)][static_cast<size_t> (enc)];
                    const float delta = (d.bipolar != 0) ? (src * d.depth) : (uniShape * d.depth);
                    if (delta == 0.0f)
                        continue;

                    if (isTrackParameter (id))
                        params.addModOffset (t, id, delta);
                    else
                        params.addModOffsetGlobal (id, delta);
                }
            }
        }

        // 3. Latch this block's source values for next block's mod-slot targeting.
        for (int s = 0; s < kModSlotsPerTrack; ++s)
            prevSrc_[static_cast<size_t> (t)][static_cast<size_t> (s)] = curSrc[s];
    }
}

namespace
{
    float adsrTemplateY (float ph01, float attackSec, float decaySec, float sustain01, float releaseSec)
    {
        const float tA = std::max (0.001f, attackSec);
        const float tD = std::max (0.001f, decaySec);
        const float tR = std::max (0.001f, releaseSec);
        const float tSus = 0.08f;
        const float T    = tA + tD + tSus + tR;
        const float u    = clamp01 (ph01);
        const float x1   = tA / T;
        const float x2   = (tA + tD) / T;
        const float x3   = (tA + tD + tSus) / T;

        if (u < x1)
        {
            const float t = x1 > 1.0e-6f ? u / x1 : 1.0f;
            return t;
        }
        if (u < x2)
        {
            const float t = (x2 - x1) > 1.0e-6f ? (u - x1) / (x2 - x1) : 1.0f;
            return 1.0f + t * (sustain01 - 1.0f);
        }
        if (u < x3)
            return sustain01;
        const float t = (1.0f - x3) > 1.0e-6f ? (u - x3) / (1.0f - x3) : 1.0f;
        return sustain01 * (1.0f - t);
    }
} // namespace

void ModEngine::writeModLcdSnapshot (int track, int slot, float inputEnv01, double beatAtEnd,
                                   ModLcdSnapshot& out) const
{
    out.carrier01.fill (0.0f);
    out.effective01.fill (0.0f);
    out.scannerPhase01 = 0.0f;
    out.valueBipolar   = 0.0f;
    out.amount01       = 1.0f;
    out.kind           = 0;
    out.active         = false;

    if (track < 0 || track >= kNumTracks || slot < 0 || slot >= kModSlotsPerTrack)
        return;

    const auto& sp = live_.slots[static_cast<size_t> (track)][static_cast<size_t> (slot)];
    out.kind       = static_cast<uint8_t> (sp.kind);
    out.amount01   = clamp01 (sp.waveAmount);

    switch (sp.kind)
    {
        case ModulatorKind::Off:
            return;

        case ModulatorKind::Wave:
        {
            const LFO::Shape sh   = shapeFromPatchIndex (sp.waveShape);
            const float      depth = waveDepth01 (sp.waveAmount);
            for (int i = 0; i < ModLcdSnapshot::kBins; ++i)
            {
                const double phRaw = (static_cast<double> (i) + 0.5)
                                     / static_cast<double> (ModLcdSnapshot::kBins);
                const double ph    = LFO::bentPhase01 (phRaw, sp.waveBend01);
                const float v   = LFO::valueForShape (sh, ph);
                const float a   = std::fabs (v);
                out.carrier01[static_cast<size_t> (i)]   = a;
                out.effective01[static_cast<size_t> (i)] = a * depth;
            }

            if (sp.waveSync != 0)
            {
                const int   divIdx = static_cast<int> (sp.waveDivision);
                const double c     = syncDivisionCyclesPerBeat (divIdx);
                out.scannerPhase01 = static_cast<float> (wrapPhase01 (beatAtEnd * c
                                                                      + static_cast<double> (sp.wavePhase01)));
                const double phRaw = wrapPhase01 (beatAtEnd * c + static_cast<double> (sp.wavePhase01));
                const double phB   = LFO::bentPhase01 (phRaw, sp.waveBend01);
                const float  bip   = LFO::valueForShape (sh, phB) * depth;
                out.valueBipolar   = bip + (sp.waveOffset01 - 0.5f) * 2.0f;
            }
            else
            {
                const auto& lfo = lfos_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
                out.scannerPhase01 = lfo.getWrappedPhase01();
                const double phRaw = static_cast<double> (out.scannerPhase01);
                const double phB   = LFO::bentPhase01 (phRaw, sp.waveBend01);
                const float  bip   = LFO::valueForShape (sh, phB) * depth;
                out.valueBipolar   = bip + (sp.waveOffset01 - 0.5f) * 2.0f;
            }
            out.active = true;
            return;
        }

        case ModulatorKind::Random:
        {
            // S-4-style sample & hold: a fixed row of steps with a playhead that sweeps left ->
            // right and wraps. Each step updates to a new value the instant the playhead reaches
            // it (one at a time); values are stable between visits, so there is no per-frame
            // flicker. Steps left of / under the playhead show the current sweep, those to the
            // right still show the previous sweep until the playhead gets to them.
            const float amt = clamp01 (sp.randomAmount);

            double stepsPerBeat = (sp.randomSync != 0)
                ? syncDivisionCyclesPerBeat (static_cast<int> (sp.randomDivision))
                : 2.0; // representative sweep rate for free-Hz mode
            if (stepsPerBeat < 1.0e-3)
                stepsPerBeat = 1.0;

            constexpr int nSteps = 8;
            const double  p    = beatAtEnd * stepsPerBeat;            // continuous step position
            const long    g    = static_cast<long> (std::floor (p));  // global step index
            const float   frac = static_cast<float> (p - static_cast<double> (g));
            const int     cur  = static_cast<int> (((g % nSteps) + nSteps) % nSteps); // playhead step

            auto hash01 = [] (long k) -> float {
                uint32_t x = static_cast<uint32_t> (k) * 2654435761u + 1013904223u;
                x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
                return 0.12f + 0.88f * static_cast<float> (x & 0xFFFFFFu) / static_cast<float> (0xFFFFFF);
            };

            // Each step's height = the value from the last time the playhead visited it.
            float stepH[nSteps];
            for (int j = 0; j < nSteps; ++j)
            {
                const int  back = (((cur - j) % nSteps) + nSteps) % nSteps; // steps since last visit
                stepH[j] = hash01 (g - back);
            }

            for (int i = 0; i < ModLcdSnapshot::kBins; ++i)
            {
                int slot = (i * nSteps) / ModLcdSnapshot::kBins;
                if (slot >= nSteps)
                    slot = nSteps - 1;
                out.carrier01[static_cast<size_t> (i)]   = stepH[slot];
                out.effective01[static_cast<size_t> (i)] = stepH[slot] * amt;
            }

            // Playhead sweeps 0 -> 1 across the steps; dot rides the current step's level.
            out.scannerPhase01 = (static_cast<float> (cur) + frac) / static_cast<float> (nSteps);
            out.valueBipolar   = stepH[cur] * amt;
            out.amount01       = amt;
            out.active         = true;
            return;
        }

        case ModulatorKind::ADSR:
        {
            const auto& e = adsrs_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
            for (int i = 0; i < ModLcdSnapshot::kBins; ++i)
            {
                const float ph = (static_cast<float> (i) + 0.5f)
                                 / static_cast<float> (ModLcdSnapshot::kBins);
                const float y  = adsrTemplateY (ph, sp.adsrAttackSec, sp.adsrDecaySec,
                                                sp.adsrSustain01, sp.adsrReleaseSec);
                out.carrier01[static_cast<size_t> (i)]   = y;
                out.effective01[static_cast<size_t> (i)] = y * e.getValue();
            }
            out.scannerPhase01 = static_cast<float> (wrapPhase01 (beatAtEnd * 0.35));
            out.valueBipolar   = e.getValue() * 2.0f - 1.0f;
            out.amount01       = 1.0f;
            out.active         = true;
            return;
        }

        case ModulatorKind::InputEnvelope:
        default:
        {
            const float u = clamp01 (inputEnv01);
            for (int i = 0; i < ModLcdSnapshot::kBins; ++i)
            {
                const float skew = static_cast<float> (i) / static_cast<float> (ModLcdSnapshot::kBins - 1);
                const float h    = clamp01 (u * (0.55f + 0.45f * skew));
                out.carrier01[static_cast<size_t> (i)]   = h;
                out.effective01[static_cast<size_t> (i)] = h;
            }
            out.scannerPhase01 = static_cast<float> (wrapPhase01 (beatAtEnd * 0.5));
            out.valueBipolar   = u * 2.0f - 1.0f;
            out.amount01       = u;
            out.active         = true;
            return;
        }
    }
}

} // namespace sculpt
