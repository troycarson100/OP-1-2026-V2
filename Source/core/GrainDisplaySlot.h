#pragma once

namespace sculpt
{

// One grain slot for LCD overlay (written from audio thread, read on UI).
struct GrainDisplaySlot
{
    float start01 = 0.0f;   // grain start in material buffer, 0..1
    float len01   = 0.0f;   // grain length as fraction of buffer length
    float phase01 = 0.0f;   // playback progress through grain, 0..1
    bool  active  = false;
};

} // namespace sculpt
