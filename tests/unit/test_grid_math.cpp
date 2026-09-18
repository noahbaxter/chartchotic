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

TEST_CASE("GridMath - a triplet gap measures up to the minimum length", "[gridmath]")
{
    // MIDI_MIN_SUSTAIN_LENGTH is PPQ(4.0/12.0): exactly a third of a quarter note, which is
    // exactly the gap between two triplet notes. A third has no exact binary form, so
    // subtracting two triplet positions lands either side of it depending where in the song
    // they are. Roughly half of all triplet pairs measure a hair SHORT, and a bare `>=` refused
    // every one of those: on a triplet grid, notes simply could not be targeted.
    const double minimum = 4.0 / 12.0;

    // Real positions that come up short, built the way a note's QN is: whole ticks over the
    // resolution, exactly what PPQ's conversion to double does. The shortfall is far finer than
    // the tick that is supposed to be the smallest difference meaning anything.
    const double tripletTicks = PPQ::PPQ_RESOLUTION / 3.0;
    for (double startTicks : { 960.0, 1600.0, 2240.0, 3200.0, 3840.0, 4480.0 })
    {
        const double startQN = startTicks / PPQ::PPQ_RESOLUTION;
        const double endQN   = (startTicks + tripletTicks) / PPQ::PPQ_RESOLUTION;
        const double span    = endQN - startQN;

        REQUIRE(GridMath::atLeast(span, minimum));
        // The guard this replaced, shown failing on the same values.
        REQUIRE_FALSE(span >= minimum);
    }

    // Still rejects anything actually shorter: a tick of slop, not a free pass.
    REQUIRE_FALSE(GridMath::atLeast(minimum - 2.0 * GridMath::TICK, minimum));
    REQUIRE(GridMath::atLeast(minimum * 2.0, minimum));
}

TEST_CASE("GridMath - a note on a range edge is inside it", "[gridmath]")
{
    // Same story for the edges: a lane runs from one note's start to another's, so both of
    // those notes have to read as inside it however the subtraction rounded.
    const double lo = 2240.0 / PPQ::PPQ_RESOLUTION;
    const double hi = 4480.0 / PPQ::PPQ_RESOLUTION;

    REQUIRE(GridMath::within(lo, lo, hi));
    REQUIRE(GridMath::within(hi, lo, hi));
    REQUIRE(GridMath::within(3.0, lo, hi));

    REQUIRE_FALSE(GridMath::within(lo - 2.0 * GridMath::TICK, lo, hi));
    REQUIRE_FALSE(GridMath::within(hi + 2.0 * GridMath::TICK, lo, hi));
}
