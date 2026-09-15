#include "test_helpers.h"
#include <catch2/catch_approx.hpp>   // test_helpers.h pulls in the macros only
#include "Midi/SongList.h"

namespace
{
    std::vector<ClipSpan> spans(std::initializer_list<std::tuple<double, double, const char*>> items)
    {
        std::vector<ClipSpan> out;
        for (const auto& [start, end, name] : items)
            out.push_back({ start, end, juce::String(name) });
        return out;
    }

    SongList built(const std::vector<ClipSpan>& in)
    {
        SongList list;
        list.buildFromSpans(in);
        return list;
    }
}

// Charts sit end to end on one timeline, gaps and all, and each item is exactly one song.
TEST_CASE("SongList - every item is its own song", "[songlist]")
{
    auto list = built(spans({ { 0.0, 120.0, "Track 1" },
                              { 120.0, 300.0, "Track 2" },
                              { 400.0, 480.0, "Track 3" } }));

    REQUIRE(list.size() == 3);
    REQUIRE(list.at(0)->name == "Track 1");
    REQUIRE(list.at(1)->name == "Track 2");
    REQUIRE(list.at(2)->name == "Track 3");
}

TEST_CASE("SongList - spans are sorted by start", "[songlist]")
{
    auto list = built(spans({ { 200.0, 300.0, "Second" }, { 0.0, 100.0, "First" } }));

    REQUIRE(list.size() == 2);
    REQUIRE(list.at(0)->name == "First");
    REQUIRE(list.at(0)->startQN == Catch::Approx(0.0));
    REQUIRE(list.at(1)->name == "Second");
}

TEST_CASE("SongList - empty and degenerate spans are dropped", "[songlist]")
{
    auto list = built(spans({ { 0.0, 0.0, "Zero" }, { 100.0, 50.0, "Backwards" },
                              { 0.0, 10.0, "Real" } }));

    REQUIRE(list.size() == 1);
    REQUIRE(list.at(0)->name == "Real");
}

TEST_CASE("SongList - an unnamed song falls back to its ordinal", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "" }, { 100.0, 200.0, "  " } }));

    REQUIRE(list.at(0)->name == "Chart 1");
    REQUIRE(list.at(1)->name == "Chart 2");
}

// REAPER names a new item after its track, so an untouched EVENTS item reports "EVENTS".
// Taking that at face value called every song in the project the same thing.
TEST_CASE("SongList - the inherited track name counts as no name", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "EVENTS" }, { 100.0, 200.0, "events" } }));

    REQUIRE(list.at(0)->name == "Chart 1");
    REQUIRE(list.at(1)->name == "Chart 2");
}

// Ordinals follow position, so moving an item renumbers it without anyone retyping.
TEST_CASE("SongList - ordinals come from timeline order, not item order", "[songlist]")
{
    auto list = built(spans({ { 200.0, 300.0, "" }, { 0.0, 100.0, "" } }));

    REQUIRE(list.at(0)->startQN == Catch::Approx(0.0));
    REQUIRE(list.at(0)->name == "Chart 1");
    REQUIRE(list.at(1)->name == "Chart 2");
}

// Two items named the same is legal in REAPER and must not collapse them: the picker shows
// two rows, told apart by their order, which is what the ordinal fallback already encodes.
TEST_CASE("SongList - duplicate take names stay two songs", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "Reprise" }, { 100.0, 200.0, "Reprise" } }));

    REQUIRE(list.size() == 2);
    REQUIRE(list.at(0)->name == "Reprise");
    REQUIRE(list.at(1)->name == "Reprise");
}

TEST_CASE("SongList - at() answers null outside the list", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "One" } }));

    REQUIRE(list.at(-1) == nullptr);
    REQUIRE(list.at(1) == nullptr);
    REQUIRE(list.at(0) != nullptr);
}

TEST_CASE("SongList - a project with no EVENTS items has no songs", "[songlist]")
{
    REQUIRE(built(spans({})).size() == 0);
}

