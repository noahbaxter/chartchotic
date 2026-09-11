#include <catch2/catch_test_macros.hpp>

#include "Midi/Utils/GridMath.h"
#include "Midi/Utils/PPQ.h"

TEST_CASE("GridMath - a tick is the finest thing that can differ", "[gridmath]")
{
    REQUIRE(GridMath::TICK == 1.0 / PPQ::PPQ_RESOLUTION);

    // The literals that used to be written by hand all sat under a tick, which is what made
    // them read as exact equality.
    REQUIRE(0.001 < GridMath::TICK);
}

TEST_CASE("GridMath - same tolerates a whole tick of slop", "[gridmath]")
{
    REQUIRE(GridMath::same(8.0, 8.0));
    REQUIRE(GridMath::same(8.0, 8.0 + GridMath::TICK * 0.5));
    REQUIRE(GridMath::same(8.0, 8.0 - GridMath::TICK * 0.5));

    REQUIRE_FALSE(GridMath::same(8.0, 8.0 + GridMath::TICK));
    REQUIRE_FALSE(GridMath::same(8.0, 9.0));
}

TEST_CASE("GridMath - a seek that lands a hair low still reads as the same place", "[gridmath]")
{
    // The section-stepping bug: a position asked for at 8.0 comes back a shade under it.
    // Under the old sub-tick tolerance this read as "not yet there", so stepping forward
    // kept re-targeting the section it had just jumped to.
    double landed = PPQ(8.0 - 1e-9).toDouble();

    REQUIRE(GridMath::same(landed, 8.0));
    REQUIRE_FALSE(GridMath::after(8.0, landed));
}

TEST_CASE("GridMath - exactly one of same, after, before holds", "[gridmath]")
{
    const double a = 40.0;
    for (double delta : { -2.0, -1.0, -GridMath::TICK, -GridMath::TICK * 0.5, 0.0,
                          GridMath::TICK * 0.5, GridMath::TICK, 1.0, 2.0 })
    {
        double b = a + delta;
        int holds = (int)GridMath::same(a, b) + (int)GridMath::after(a, b)
                  + (int)GridMath::before(a, b);
        REQUIRE(holds == 1);
    }
}

TEST_CASE("GridMath - after and before are the way round they read", "[gridmath]")
{
    REQUIRE(GridMath::after(9.0, 8.0));
    REQUIRE(GridMath::before(8.0, 9.0));
    REQUIRE_FALSE(GridMath::after(8.0, 9.0));
    REQUIRE_FALSE(GridMath::before(9.0, 8.0));
}

TEST_CASE("PPQ - rounds to the nearest tick rather than toward zero", "[gridmath]")
{
    // Truncation put every one of these a whole tick below the position it meant.
    REQUIRE(PPQ(8.0 - 1e-9).toScaled() == PPQ(8.0).toScaled());
    REQUIRE(PPQ(8.0 + 1e-9).toScaled() == PPQ(8.0).toScaled());
    REQUIRE(PPQ(-2.5 + 1e-9).toScaled() == PPQ(-2.5).toScaled());
}

TEST_CASE("PPQ - a tolerance finer than a tick survives a comparison", "[gridmath]")
{
    // Quantising the operand collapsed this to `pos > now`, so the guard never fired.
    PPQ pos(8.0);
    double now = 8.0;
    REQUIRE_FALSE(pos > now + 1e-4);
    REQUIRE(pos > now - 1e-4);
}
