#pragma once

#include <algorithm>
#include <cstdint>

namespace sculpt
{

// Euclidean (Bjorklund-style) rhythm: `pulses` hits across `steps` steps, then
// rotated so step index 0 advances with `rotate` (left / earlier in time).
// Bit i of result = step i is an on-step after rotation. steps 1..16.
inline uint16_t euclideanRhythm (int steps, int pulses, int rotate)
{
    if (steps < 1)
        steps = 1;
    if (steps > 16)
        steps = 16;
    if (pulses < 1)
        pulses = 1;
    if (pulses > steps)
        pulses = steps;

    uint16_t base = 0;
    for (int i = 0; i < steps; ++i)
    {
        if ((i * pulses) % steps < pulses)
            base |= static_cast<uint16_t> (1u << i);
    }

    const int r = ((rotate % steps) + steps) % steps;
    if (r == 0)
        return base;

    uint16_t out = 0;
    for (int i = 0; i < steps; ++i)
    {
        const int src = (i - r + steps) % steps;
        if ((base >> src) & 1)
            out |= static_cast<uint16_t> (1u << i);
    }
    return out;
}

} // namespace sculpt
