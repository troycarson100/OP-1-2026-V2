#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace sculpt
{

// Parse a tempo from a sample filename. Returns a BPM in (0..300] or 0 if none is found.
// Priority:
//   1. A number adjacent to "bpm" (e.g. "80bpm", "bpm_128", "90 BPM", "128-bpm").
//   2. Otherwise, if exactly one standalone integer token sits in a plausible tempo range
//      ([60,200]), use it (e.g. "amen_174_4bar.wav" -> 174).
// Portable (no JUCE, std-lib only) so it runs on the Raspberry Pi build too.
inline float parseBpmFromFilename (const char* cname)
{
    if (cname == nullptr)
        return 0.0f;

    std::string lower (cname);
    for (auto& c : lower)
        c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));

    const int n = static_cast<int> (lower.size());

    struct Run { int start; int end; int val; };
    std::vector<Run> runs;
    for (int i = 0; i < n; )
    {
        if (std::isdigit (static_cast<unsigned char> (lower[static_cast<size_t> (i)])))
        {
            int  j = i;
            long v = 0;
            int  digits = 0;
            while (j < n && std::isdigit (static_cast<unsigned char> (lower[static_cast<size_t> (j)])) && digits < 6)
            {
                v = v * 10 + (lower[static_cast<size_t> (j)] - '0');
                ++j;
                ++digits;
            }
            runs.push_back ({ i, j, static_cast<int> (v) });
            i = j;
        }
        else
        {
            ++i;
        }
    }

    auto isSep = [] (char c) { return c == ' ' || c == '_' || c == '-' || c == '.'; };

    auto bpmBefore = [&] (int start) -> bool   // "bpm" immediately before the number (sep allowed)
    {
        int k = start - 1;
        while (k >= 0 && isSep (lower[static_cast<size_t> (k)]))
            --k;
        return k >= 2 && lower.compare (static_cast<size_t> (k - 2), 3, "bpm") == 0;
    };
    auto bpmAfter = [&] (int end) -> bool       // "bpm" immediately after the number (sep allowed)
    {
        int k = end;
        while (k < n && isSep (lower[static_cast<size_t> (k)]))
            ++k;
        return k + 3 <= n && lower.compare (static_cast<size_t> (k), 3, "bpm") == 0;
    };

    for (const auto& r : runs)
        if ((bpmAfter (r.end) || bpmBefore (r.start)) && r.val >= 30 && r.val <= 300)
            return static_cast<float> (r.val);

    int found = 0, val = 0;
    for (const auto& r : runs)
        if (r.val >= 60 && r.val <= 200)
        {
            ++found;
            val = r.val;
        }
    return found == 1 ? static_cast<float> (val) : 0.0f;
}

} // namespace sculpt
