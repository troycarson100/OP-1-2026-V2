#pragma once

namespace sculpt
{

// Engine-level play state. The host (or hardware transport buttons) drive
// this; the engine never asks JUCE about transport directly.
class Transport
{
public:
    void play()              { playing_ = true; }
    void stop()              { playing_ = false; }
    void setPlaying (bool p) { playing_ = p; }
    bool isPlaying() const   { return playing_; }

    // Informational only; host position in seconds when available.
    void   setHostTimeSeconds (double t) { hostTimeSeconds_ = t; }
    double getHostTimeSeconds() const    { return hostTimeSeconds_; }

private:
    bool   playing_         = true;   // The prototype free-runs by default.
    double hostTimeSeconds_ = 0.0;
};

} // namespace sculpt
