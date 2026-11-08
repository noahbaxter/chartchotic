#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "UI/ControlConstants.h"

TEST_CASE("Note speed - regular speeds keep their window", "[note_speed]")
{
    REQUIRE_FALSE(isOverviewSpeed(NOTE_SPEED_MIN));
    REQUIRE(noteSpeedWindowSeconds(7) == Catch::Approx(7.87 / 7.0));
    REQUIRE(noteSpeedWindowSeconds(NOTE_SPEED_MAX) == Catch::Approx(7.87 / 20.0));
    REQUIRE(noteSpeedLabel(7) == "7");
}

TEST_CASE("Note speed - below the minimum steps out through the overview spans", "[note_speed]")
{
    REQUIRE(NOTE_SPEED_LOWEST == NOTE_SPEED_MIN - 4);

    double previous = noteSpeedWindowSeconds(NOTE_SPEED_MIN);
    for (int speed = NOTE_SPEED_MIN - 1; speed >= NOTE_SPEED_LOWEST; --speed)
    {
        REQUIRE(isOverviewSpeed(speed));
        const double window = noteSpeedWindowSeconds(speed);
        REQUIRE(window > previous);   // every step goes wider
        previous = window;
    }

    REQUIRE(noteSpeedLabel(NOTE_SPEED_MIN - 1) == "8s");
    REQUIRE(noteSpeedLabel(NOTE_SPEED_LOWEST) == "60s");
    REQUIRE(noteSpeedWindowSeconds(NOTE_SPEED_LOWEST - 5) == Catch::Approx(60.0));   // clamped
}
