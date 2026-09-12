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

    SongSections sectionsAt(std::initializer_list<std::pair<double, const char*>> items)
    {
        TrackTextEvents events;
        for (const auto& [qn, text] : items)
            events.push_back({ PPQ(qn), juce::String(text) });

        SongSections s;
        s.buildFromTextEvents(events);
        return s;
    }

    SongList built(const std::vector<ClipSpan>& in, const SongSections& sections = {})
    {
        SongList list;
        list.buildFromSpans(in, sections);
        return list;
    }
}

// An album item spanning twelve per-song items is the whole reason this exists, so overlap
// has to survive the build rather than being merged away like InstrumentSession's spans are.
TEST_CASE("SongList - overlapping spans are kept as separate songs", "[songlist]")
{
    auto list = built(spans({ { 0.0, 480.0, "Album" },
                              { 0.0, 120.0, "Track 1" },
                              { 120.0, 480.0, "Track 2" } }));

    REQUIRE(list.size() == 3);
    REQUIRE(list.at(0)->name == "Album");
    REQUIRE(list.at(1)->name == "Track 1");
    REQUIRE(list.at(2)->name == "Track 2");
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

// An unnamed item is the common case: REAPER only names a take when you ask it to.
TEST_CASE("SongList - an unnamed song takes the first section inside it", "[songlist]")
{
    auto sections = sectionsAt({ { 4.0, "[section intro]" }, { 64.0, "[section verse_1]" },
                                 { 200.0, "[section chorus]" } });
    auto list = built(spans({ { 0.0, 100.0, "" }, { 100.0, 300.0, "" } }), sections);

    REQUIRE(list.at(0)->name == "intro");
    REQUIRE(list.at(1)->name == "chorus");
}

TEST_CASE("SongList - a song with no name and no sections falls back to its ordinal", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "" }, { 100.0, 200.0, "" } }));

    REQUIRE(list.at(0)->name == "Song 1");
    REQUIRE(list.at(1)->name == "Song 2");
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

TEST_CASE("SongList - at() answers null outside the list and for the session sentinel", "[songlist]")
{
    auto list = built(spans({ { 0.0, 100.0, "One" } }));

    REQUIRE(list.at(SongList::SESSION) == nullptr);
    REQUIRE(list.at(1) == nullptr);
    REQUIRE(list.at(0) != nullptr);
}

// The picker costs vertical space, so it only earns it when there is a choice to make.
TEST_CASE("SongList - one song or none is not pickable", "[songlist]")
{
    REQUIRE_FALSE(built(spans({})).isPickable());
    REQUIRE_FALSE(built(spans({ { 0.0, 100.0, "Only" } })).isPickable());
    REQUIRE(built(spans({ { 0.0, 100.0, "A" }, { 100.0, 200.0, "B" } })).isPickable());
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
