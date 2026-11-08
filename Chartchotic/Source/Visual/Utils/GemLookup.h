#pragma once

#include <algorithm>
#include <vector>

// A per-frame set of gems (selected, erase targets, lane-tool highlights) that the note renderer
// asks about once for every gem it draws. A scan per gem made a marquee over a dense chart
// quadratic, so the list is sorted by time once a frame and each question is a binary search to
// the match window.
namespace GemLookup
{
    struct Gem { int lane; double time; };

    // Close enough to be the same gem: render times drift by float error, never by this much.
    constexpr double kMatchSec = 0.002;

    inline void sort(std::vector<Gem>& gems)
    {
        std::sort(gems.begin(), gems.end(), [](const Gem& a, const Gem& b) { return a.time < b.time; });
    }

    inline bool contains(const std::vector<Gem>& sortedByTime, int lane, double time)
    {
        auto it = std::lower_bound(sortedByTime.begin(), sortedByTime.end(), time - kMatchSec,
                                   [](const Gem& g, double t) { return g.time <= t; });
        for (; it != sortedByTime.end() && it->time < time + kMatchSec; ++it)
            if (it->lane == lane) return true;
        return false;
    }
}
