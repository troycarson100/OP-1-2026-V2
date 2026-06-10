#pragma once

#include "../core/ParameterIds.h"

namespace sculpt
{

// The physical pages of the instrument. Each page maps to one stage of the
// track chain and exposes up to one parameter per encoder.
enum class Page : int
{
    Material = 0,
    Granular,
    Filter,
    Color,
    Space,
    Mixer,
    Count
};

constexpr int kMaxParamsPerPage = 8;

// Static page descriptions - which parameters a page puts under the encoders.
class PageModel
{
public:
    static const char* pageName (Page page);

    // Returns the ParameterId for an encoder slot on a page, or
    // ParameterId::Count when the slot is empty.
    static ParameterId parameterForSlot (Page page, int slot);

    static int parameterCount (Page page);
};

} // namespace sculpt
