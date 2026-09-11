/*
    ==============================================================================

        GridMath.h
        Author: Noah Baxter

        One home for "are these two positions the same place?".

        Every position on the chart is stored to the tick (see PPQ), so a tolerance
        finer than a tick cannot mean anything: a value that has been through
        seconds and back, or built up by adding a triplet at a time, lands on a
        neighbouring tick rather than the one it means. Tolerances written as bare
        literals kept landing just under that, so the guard read as exact equality
        and quietly never fired. Ask through here instead.

    ==============================================================================
*/

#pragma once

#include "PPQ.h"
#include <cmath>

namespace GridMath
{
    // The atom every position is stored on. Nothing musical is finer, so nothing here
    // splits it.
    inline constexpr double TICK = 1.0 / PPQ::PPQ_RESOLUTION;

    // Same place. A whole tick rather than half of one: a round trip through seconds can
    // land either side of the tick it means, so half would still call those two positions
    // different.
    inline bool same(double a, double b) { return std::abs(a - b) < TICK; }

    // Strictly past / strictly short of, with the tick's worth of slop that `same` allows.
    // Written so that exactly one of same/after/before holds for any pair.
    inline bool after (double a, double b) { return a - b >= TICK; }
    inline bool before(double a, double b) { return b - a >= TICK; }
}
