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

// 4 QN to the bar throughout, so a rung's span is bars * 4.
TEST_CASE("SongList - a long chart offers every bar step", "[songlist]")
{
    auto rungs = SongList::zoomLadder(4000.0, 4.0);

    REQUIRE(rungs == std::vector<int>{ 8, 16, 32, 64, 128, 256,
                                       SongList::ZOOM_SONG, SongList::ZOOM_SESSION });
}

// A 40 bar chart at 64 bars shows the same thing the song rung shows, so it is not a rung.
TEST_CASE("SongList - steps wider than the chart are dropped", "[songlist]")
{
    auto rungs = SongList::zoomLadder(40.0 * 4.0, 4.0);

    REQUIRE(rungs == std::vector<int>{ 8, 16, 32, SongList::ZOOM_SONG, SongList::ZOOM_SESSION });
}

TEST_CASE("SongList - a chart shorter than every step offers only the two views", "[songlist]")
{
    auto rungs = SongList::zoomLadder(4.0 * 4.0, 4.0);

    REQUIRE(rungs == std::vector<int>{ SongList::ZOOM_SONG, SongList::ZOOM_SESSION });
}

// A step exactly as long as the chart is the song view, so it is dropped like a wider one.
TEST_CASE("SongList - a step the length of the chart is not its own rung", "[songlist]")
{
    auto rungs = SongList::zoomLadder(32.0 * 4.0, 4.0);

    REQUIRE(rungs == std::vector<int>{ 8, 16, SongList::ZOOM_SONG, SongList::ZOOM_SESSION });
}

// Bars are twice as long in 8/4, so half as many steps fit the same quarter-note span.
TEST_CASE("SongList - the ladder follows bar length, not quarter notes", "[songlist]")
{
    auto rungs = SongList::zoomLadder(40.0 * 4.0, 8.0);

    REQUIRE(rungs == std::vector<int>{ 8, 16, SongList::ZOOM_SONG, SongList::ZOOM_SESSION });
}

TEST_CASE("SongList - a project with no EVENTS items has no songs", "[songlist]")
{
    REQUIRE(built(spans({})).size() == 0);
}

namespace
{
    struct Window { double start, end; };

    Window confined(double start, double end, double songStart, double songEnd)
    {
        SongList::confineWindow(start, end, songStart, songEnd);
        return { start, end };
    }
}

// Shifted, not truncated. A truncated window compresses the strip against the song's edges,
// so the same zoom stops meaning the same distance on screen.
TEST_CASE("SongList - a window off the bottom shifts up to the song start", "[songlist]")
{
    auto w = confined(-20.0, 44.0, 0.0, 400.0);

    REQUIRE(w.start == Catch::Approx(0.0));
    REQUIRE(w.end == Catch::Approx(64.0));
}

TEST_CASE("SongList - a window off the top shifts down to the song end", "[songlist]")
{
    auto w = confined(380.0, 444.0, 0.0, 400.0);

    REQUIRE(w.start == Catch::Approx(336.0));
    REQUIRE(w.end == Catch::Approx(400.0));
}

TEST_CASE("SongList - a window already inside the song is untouched", "[songlist]")
{
    auto w = confined(100.0, 164.0, 0.0, 400.0);

    REQUIRE(w.start == Catch::Approx(100.0));
    REQUIRE(w.end == Catch::Approx(164.0));
}

// Zoomed out further than the song is long, so there is nothing to shift: the window is
// the song, and the strip shows less than the zoom asked for rather than showing neighbours.
TEST_CASE("SongList - a song shorter than the window becomes the window", "[songlist]")
{
    auto w = confined(0.0, 256.0, 100.0, 140.0);

    REQUIRE(w.start == Catch::Approx(100.0));
    REQUIRE(w.end == Catch::Approx(140.0));
}
