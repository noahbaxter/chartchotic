/*
  ==============================================================================

    PartWindow.h
    Interpreted output for one Part — all 4 difficulties computed at once.

  ==============================================================================
*/

#pragma once

#include <array>
#include "DifficultyWindow.h"
#include "../../UI/ControlConstants.h"

struct PartWindow {
    std::array<DifficultyWindow, 4> difficulties; // indexed by (int)SkillLevel - 1
    // EASY=0, MEDIUM=1, HARD=2, EXPERT=3

    // Star power bands, solo bands and BRE lanes. These come from their markers alone, so they
    // belong to the part and there is one set. The fill activation band is the exception and
    // lives on DifficultyWindow, since it depends on which note ends the fill. Kept out of the
    // sustain windows: only the editor's phrase views draw them, and gameplay code reading
    // sustains should never trip over them.
    SustainWindow phraseWindow;

    DifficultyWindow& forSkill(SkillLevel skill)
    {
        return difficulties[(int)skill - 1];
    }

    const DifficultyWindow& forSkill(SkillLevel skill) const
    {
        return difficulties[(int)skill - 1];
    }
};
