#include "ModEngine.h"
#include "LFO.h"
#include "SyncDivision.h"
#include "../ui/PageModel.h"
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
        }
    }
}

void ModEngine::triggerAdsr (int track, int slot)
{
    if (track < 0 || track >= kNumTracks || slot < 0 || slot >= kModSlotsPerTrack)
        return;
    const uint32_t bit = 1u << static_cast<uint32_t> (track * kModSlotsPerTrack + slot);
    pendingAdsrTrig_.fetch_or (bit, std::memory_order_relaxed);
}

float ModEngine::sourceValue (int track, int slot, float inputEnv01, int numSamples,
                              double beatAtBlockStart, double beatAtBlockEnd)
{
    const auto& sp = live_.slots[static_cast<size_t> (track)][static_cast<size_t> (slot)];
    switch (sp.kind)
    {
        case ModulatorKind::Off:
            return 0.0f;
        case ModulatorKind::Wave:
        {
            const LFO::Shape sh = shapeFromPatchIndex (sp.waveShape);
            const float      depth = waveDepth01 (sp.waveAmount);
            float            bipolar = 0.0f;

            if (sp.waveSync != 0)
            {
                const int divIdx = static_cast<int> (sp.waveDivision);
                const double c = syncDivisionCyclesPerBeat (divIdx);
                const double ph = wrapPhase01 (beatAtBlockStart * c + static_cast<double> (sp.wavePhase01));
                bipolar = LFO::valueForShape (sh, ph) * depth;
            }
            else
            {
                auto& lfo = lfos_[static_cast<size_t> (track)][static_cast<size_t> (slot)];
                lfo.setRateHz (sp.waveRateHz);
                lfo.setShape (sh);
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
                r.setRateHz (sp.randomRateHz);
                r.setSlew (sp.randomSlew01);
                r.update (numSamples);
            }
            return r.getValue();
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

    const uint32_t trig = pendingAdsrTrig_.exchange (0, std::memory_order_acq_rel);
    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            if (trig & (1u << static_cast<uint32_t> (t * kModSlotsPerTrack + s)))
                adsrs_[static_cast<size_t> (t)][static_cast<size_t> (s)].trigger();
        }
    }

    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < kModSlotsPerTrack; ++s)
        {
            if (live_.slots[static_cast<size_t> (t)][static_cast<size_t> (s)].kind == ModulatorKind::Off)
                continue;

            const float src = sourceValue (t, s, inputEnv01, numSamples,
                                           beatAtBlockStart, beatAtBlockEnd);
            const float uniShape = src * 0.5f + 0.5f;

            for (int pg = 0; pg < kModMapTargetPages; ++pg)
            {
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
    }
}

} // namespace sculpt
