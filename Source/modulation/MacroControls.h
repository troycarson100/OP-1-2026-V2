#pragma once

#include <array>
#include "../util/Constants.h"
#include "../util/MathUtils.h"

namespace sculpt
{

// Four performance macros. On hardware these map to dedicated controls;
// in the plugin they are bridged from host parameters. Unipolar [0, 1].
class MacroControls
{
public:
    void reset()
    {
        values_.fill (0.0f);
    }

    void setMacro (int index, float value01)
    {
        if (index >= 0 && index < kNumMacros)
            values_[static_cast<size_t> (index)] = clamp01 (value01);
    }

    float getMacro (int index) const
    {
        return (index >= 0 && index < kNumMacros) ? values_[static_cast<size_t> (index)] : 0.0f;
    }

private:
    std::array<float, kNumMacros> values_ {};
};

} // namespace sculpt
