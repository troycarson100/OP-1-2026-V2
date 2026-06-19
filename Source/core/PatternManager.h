#pragma once

#include <array>
#include "Pattern.h"
#include "../util/Constants.h"

namespace sculpt
{

// Fixed bank of sequencer patterns. Mirrors SceneManager: allocation-free,
// safe to touch at the start of an audio block. Portable, no JUCE.
class PatternManager
{
public:
    void reset()
    {
        for (auto& p : patterns_)
            p.clear();
        current_ = 0;
    }

    int  getCurrentIndex() const          { return current_; }
    void setCurrentIndex (int idx)        { if (validIndex (idx)) current_ = idx; }

    Pattern&       current()              { return patterns_[static_cast<size_t> (current_)]; }
    const Pattern& current() const        { return patterns_[static_cast<size_t> (current_)]; }

    Pattern&       pattern (int idx)      { return patterns_[static_cast<size_t> (clampIndex (idx))]; }
    const Pattern& pattern (int idx) const { return patterns_[static_cast<size_t> (clampIndex (idx))]; }

private:
    static bool validIndex (int idx)  { return idx >= 0 && idx < kNumSeqPatterns; }
    static int  clampIndex (int idx)  { return idx < 0 ? 0 : (idx >= kNumSeqPatterns ? kNumSeqPatterns - 1 : idx); }

    std::array<Pattern, kNumSeqPatterns> patterns_ {};
    int current_ = 0;
};

} // namespace sculpt
