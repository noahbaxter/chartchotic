#include <catch2/catch_test_macros.hpp>
#include "Visual/Utils/GemLookup.h"

TEST_CASE("GemLookup - matches lane and time within the window", "[gem_lookup]")
{
    std::vector<GemLookup::Gem> gems = { { 2, 1.0 }, { 0, 0.5 }, { 3, 1.0 }, { 1, 2.0 } };
    GemLookup::sort(gems);

    REQUIRE(GemLookup::contains(gems, 2, 1.0));
    REQUIRE(GemLookup::contains(gems, 3, 1.0));          // same moment, other lane
    REQUIRE(GemLookup::contains(gems, 0, 0.5015));       // float drift inside the window
    REQUIRE_FALSE(GemLookup::contains(gems, 1, 1.0));    // right time, wrong lane
    REQUIRE_FALSE(GemLookup::contains(gems, 0, 0.503));  // outside the window
    REQUIRE_FALSE(GemLookup::contains(gems, 1, 1.998));  // exactly on the edge is outside
    REQUIRE_FALSE(GemLookup::contains({}, 0, 0.0));
}

TEST_CASE("GemLookup - agrees with a linear scan over a dense chart", "[gem_lookup]")
{
    std::vector<GemLookup::Gem> gems;
    for (int i = 0; i < 2000; ++i)
        gems.push_back({ i % 5, (double)((i * 7919) % 2000) * 0.0105 });   // scattered, with chords
    auto sorted = gems;
    GemLookup::sort(sorted);

    auto linear = [&gems](int lane, double t)
    {
        for (const auto& g : gems)
            if (g.lane == lane && std::abs(g.time - t) < GemLookup::kMatchSec) return true;
        return false;
    };

    for (int i = 0; i < 4000; ++i)
    {
        const double t = i * 0.00525;
        for (int lane = 0; lane < 5; ++lane)
            REQUIRE(GemLookup::contains(sorted, lane, t) == linear(lane, t));
    }
}
