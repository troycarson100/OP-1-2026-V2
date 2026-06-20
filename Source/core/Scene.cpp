#include "Scene.h"
#include "ParameterState.h"
#include "../util/MathUtils.h"

namespace sculpt
{

void Scene::captureFrom (const ParameterState& state, int currentTrack, int currentPage)
{
    for (int t = 0; t < kNumTracks; ++t)
        for (int i = kFirstTrackParam; i < kFirstGlobalTail; ++i)
            trackParams[static_cast<size_t> (t)][static_cast<size_t> (i)]
                = state.getTrack (t, static_cast<ParameterId> (i));

    macros[0] = state.getGlobal (ParameterId::Macro1);
    macros[1] = state.getGlobal (ParameterId::Macro2);
    macros[2] = state.getGlobal (ParameterId::Macro3);
    macros[3] = state.getGlobal (ParameterId::Macro4);

    selectedTrack = currentTrack;
    selectedPage  = currentPage;
    used = true;
}

void Scene::applyTo (ParameterState& state) const
{
    if (! used)
        return;

    for (int t = 0; t < kNumTracks; ++t)
        for (int i = kFirstTrackParam; i < kFirstGlobalTail; ++i)
            state.setTrack (t, static_cast<ParameterId> (i),
                            trackParams[static_cast<size_t> (t)][static_cast<size_t> (i)]);

    state.setGlobal (ParameterId::Macro1, macros[0]);
    state.setGlobal (ParameterId::Macro2, macros[1]);
    state.setGlobal (ParameterId::Macro3, macros[2]);
    state.setGlobal (ParameterId::Macro4, macros[3]);
}

void Scene::morphInto (const Scene& a, const Scene& b, float amount, ParameterState& state)
{
    if (! a.used || ! b.used)
        return;

    const float t01 = clamp01 (amount);

    for (int t = 0; t < kNumTracks; ++t)
        for (int i = kFirstTrackParam; i < kFirstGlobalTail; ++i)
        {
            const auto ti = static_cast<size_t> (t);
            const auto pi = static_cast<size_t> (i);
            state.setTrack (t, static_cast<ParameterId> (i),
                            lerp (a.trackParams[ti][pi], b.trackParams[ti][pi], t01));
        }

    state.setGlobal (ParameterId::Macro1, lerp (a.macros[0], b.macros[0], t01));
    state.setGlobal (ParameterId::Macro2, lerp (a.macros[1], b.macros[1], t01));
    state.setGlobal (ParameterId::Macro3, lerp (a.macros[2], b.macros[2], t01));
    state.setGlobal (ParameterId::Macro4, lerp (a.macros[3], b.macros[3], t01));
}

} // namespace sculpt
