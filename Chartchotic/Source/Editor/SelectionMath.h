#pragma once

#include <algorithm>
#include <vector>
#include "AuthoringTypes.h"
#include "../Midi/Utils/GridMath.h"

// Shift-selection in edit mode: every note the click or box touches flips, so a note already
// selected drops out and one that was not comes in.
namespace SelectionMath
{
    inline bool sameNote(const SelectedNote& a, const SelectedNote& b)
    {
        return a.trackIdx == b.trackIdx && a.pitch == b.pitch && GridMath::same(a.startQN, b.startQN);
    }

    inline std::vector<SelectedNote> toggle(std::vector<SelectedNote> base,
                                            const std::vector<SelectedNote>& flipped)
    {
        for (const auto& f : flipped)
        {
            auto it = std::find_if(base.begin(), base.end(),
                                   [&f](const SelectedNote& n) { return sameNote(n, f); });
            if (it != base.end()) base.erase(it);
            else                  base.push_back(f);
        }
        return base;
    }
}
