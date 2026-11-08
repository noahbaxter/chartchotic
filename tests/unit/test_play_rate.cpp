#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Host/PlayRateMath.h"

TEST_CASE("Play rate - snaps to 0.05 steps inside 0.25x to 3x", "[play_rate]")
{
    REQUIRE(PlayRateMath::snap(1.0)   == Catch::Approx(1.0));
    REQUIRE(PlayRateMath::snap(0.52)  == Catch::Approx(0.5));
    REQUIRE(PlayRateMath::snap(0.53)  == Catch::Approx(0.55));
    REQUIRE(PlayRateMath::snap(1.234) == Catch::Approx(1.25));
    REQUIRE(PlayRateMath::snap(0.1)   == Catch::Approx(0.25));   // floor
    REQUIRE(PlayRateMath::snap(5.0)   == Catch::Approx(3.0));    // ceiling
}

TEST_CASE("Play rate - same is within a fraction of a step", "[play_rate]")
{
    REQUIRE(PlayRateMath::same(0.5, 0.5000001));
    REQUIRE_FALSE(PlayRateMath::same(0.5, 0.55));
}
