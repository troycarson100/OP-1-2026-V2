// Portable engine tests: PatternManager is trivially-copyable and round-trips through a raw blob
// (the basis for plugin-state persistence of sequencer patterns + p-locks).
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <type_traits>
#include <vector>

#include "core/PatternManager.h"
#include "core/ParameterIds.h"

using namespace sculpt;

int main()
{
    static_assert (std::is_trivially_copyable<PatternManager>::value, "must be memcpy-able");
    static_assert (std::is_trivially_copyable<Pattern>::value, "must be memcpy-able");
    static_assert (std::is_trivially_copyable<Step>::value, "must be memcpy-able");

    PatternManager src;
    src.reset();
    src.setCurrentIndex (3);
    auto& p = src.current();
    p.setTrig (0, 0, true);
    p.setTrig (5, 17, true);
    p.setLock (0, 0, static_cast<int> (ParameterId::FilterCutoff), 0.42f);
    p.setLock (0, 0, static_cast<int> (ParameterId::TapeSpeed), 0.7f);
    p.setLock (5, 17, static_cast<int> (ParameterId::GrainMix), 0.9f);
    // A second pattern, to confirm the whole bank round-trips.
    src.pattern (7).setTrig (2, 4, true);

    // Serialize -> bytes -> deserialize into a fresh manager.
    std::vector<uint8_t> blob (sizeof (PatternManager));
    std::memcpy (blob.data(), &src, blob.size());

    PatternManager dst;
    dst.reset();
    assert (blob.size() == sizeof (PatternManager));
    std::memcpy (&dst, blob.data(), blob.size());

    assert (dst.getCurrentIndex() == 3);
    assert (dst.current().hasTrig (0, 0));
    assert (dst.current().hasTrig (5, 17));
    assert (! dst.current().hasTrig (1, 0));

    float v = 0.0f;
    assert (dst.current().getLock (0, 0, static_cast<int> (ParameterId::FilterCutoff), v) && v == 0.42f);
    assert (dst.current().getLock (0, 0, static_cast<int> (ParameterId::TapeSpeed), v) && v == 0.7f);
    assert (dst.current().lockCount (0, 0) == 2);
    assert (dst.current().getLock (5, 17, static_cast<int> (ParameterId::GrainMix), v) && v == 0.9f);
    assert (dst.pattern (7).hasTrig (2, 4));

    std::printf ("PatternSerialize tests passed\n");
    return 0;
}
