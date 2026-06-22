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
    Mod,
    Count
};

constexpr int kMaxParamsPerPage = 16;

// Static page descriptions - which parameters a page puts under the encoders.
class PageModel
{
public:
    static const char* pageName (Page page);

    // Returns the ParameterId for an encoder slot on a page, or
    // ParameterId::Count when the slot is empty.
    // Granular uses two 8-encoder sub-pages: 0 = core grains, 1 = sync/Euclidean/pitch quant + patterns.
    static ParameterId parameterForSlot (Page page, int slot, int granularEncoderPage = 0);

    static int parameterCount (Page page, int granularEncoderPage = 0);

    // Modulation destination for a target page + encoder (0..kMaxModMappingEncoders-1). Distinct
    // from parameterForSlot because the Material page has two encoder sub-pages: this flattens its
    // most musical destinations (incl. Sample Slot / Sample Mode) into one 8-slot mod list so they
    // are reachable on the Mod page. Other pages fall through to parameterForSlot.
    static ParameterId modDestinationForSlot (Page page, int enc);
};

} // namespace sculpt
