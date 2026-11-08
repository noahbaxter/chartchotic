#pragma once

#include <algorithm>
#include <cmath>

// The speed control's range and step: 0.25x to 3x in 0.05 steps.
namespace PlayRateMath
{
    constexpr double kMin  = 0.25;
    constexpr double kMax  = 3.0;
    constexpr double kStep = 0.05;

    inline double snap(double rate)
    {
        const double stepped = std::round(rate / kStep) * kStep;
        return std::clamp(stepped, kMin, kMax);
    }

    // Close enough to the same rate that the control need not redraw, or set it again.
    inline bool same(double a, double b) { return std::abs(a - b) < kStep * 0.25; }
}
