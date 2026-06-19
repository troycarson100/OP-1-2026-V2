// Portable engine tests: per-step parameter locks (Step storage) + ParameterState override layer.
#include <cassert>
#include <cmath>
#include <cstdio>

#include "core/ParameterState.h"
#include "core/Pattern.h"
#include "core/ParameterIds.h"

using namespace sculpt;

namespace
{
int idi (ParameterId id) { return static_cast<int> (id); }
bool near (float a, float b) { return std::fabs (a - b) < 1.0e-6f; }
}

int main()
{
    // --- Step sparse lock storage ---
    {
        Step s;
        assert (! s.hasLock (idi (ParameterId::TapeSpeed)));

        s.setLock (idi (ParameterId::TapeSpeed), 0.7f);
        assert (s.hasLock (idi (ParameterId::TapeSpeed)) && s.numLocks == 1);
        float v = 0.0f;
        assert (s.getLock (idi (ParameterId::TapeSpeed), v) && near (v, 0.7f));

        s.setLock (idi (ParameterId::TapeSpeed), 0.3f); // update in place, no new slot
        assert (s.numLocks == 1);
        s.getLock (idi (ParameterId::TapeSpeed), v);
        assert (near (v, 0.3f));

        s.setLock (idi (ParameterId::FilterCutoff), 0.9f);
        assert (s.numLocks == 2);

        s.clearLock (idi (ParameterId::TapeSpeed));
        assert (s.numLocks == 1 && ! s.hasLock (idi (ParameterId::TapeSpeed))
                && s.hasLock (idi (ParameterId::FilterCutoff)));

        // Capacity is bounded.
        Step full;
        for (int i = 0; i < kMaxLocksPerStep + 4; ++i)
            full.setLock (kFirstTrackParam + i, 0.1f);
        assert (full.numLocks == kMaxLocksPerStep);
    }

    // --- ParameterState step-override layer ---
    {
        ParameterState ps;
        ps.resetToDefaults();
        const int trk = 2;
        const auto id = ParameterId::FilterCutoff;

        ps.setTrack (trk, id, 0.5f);
        assert (near (ps.effective (trk, id), 0.5f));

        ps.setStepOverride (trk, id, 0.2f);              // override replaces base
        assert (ps.hasStepOverride (trk, id));
        assert (near (ps.effective (trk, id), 0.2f));

        ps.addModOffset (trk, id, 0.1f);                 // modulation still adds on top
        assert (near (ps.effective (trk, id), 0.3f));

        ps.clearStepOverride (trk, id);                  // revert to base (+ mod)
        assert (! ps.hasStepOverride (trk, id));
        assert (near (ps.effective (trk, id), 0.6f));

        // clearStepOverrides drops every lock on the track.
        ps.setStepOverride (trk, ParameterId::TapeSpeed, 0.9f);
        ps.setStepOverride (trk, ParameterId::GrainMix, 0.1f);
        ps.clearStepOverrides (trk);
        assert (! ps.hasStepOverride (trk, ParameterId::TapeSpeed));
        assert (! ps.hasStepOverride (trk, ParameterId::GrainMix));

        // Overrides are per-track: track 0 unaffected by track 2's lock.
        ps.setStepOverride (trk, id, 0.8f);
        assert (! ps.hasStepOverride (0, id));
    }

    std::printf ("ParamLock tests passed\n");
    return 0;
}
