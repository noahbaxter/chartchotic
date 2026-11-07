#include "test_helpers.h"
#include "Midi/SongSections.h"
#include "Editor/SectionRenumber.h"

namespace
{
    TrackTextEvents events(std::initializer_list<std::pair<double, const char*>> items)
    {
        TrackTextEvents out;
        for (const auto& [qn, text] : items)
            out.push_back({ PPQ(qn), juce::String(text) });
        return out;
    }

    SongSections built(std::initializer_list<std::pair<double, const char*>> items)
    {
        SongSections s;
        s.buildFromTextEvents(events(items));
        return s;
    }
}

// All three spellings appear in official charts: Embedded uses the spaced form, Interloper
// uses prc_. Getting one of them wrong loses every section in an entire era of charts.
TEST_CASE("SongSections - every section spelling parses", "[sections]")
{
    struct Case { const char* text; const char* name; };
    const Case cases[] = {
        { "[section verse_1a]", "verse_1a" },
        { "[section_verse_1a]", "verse_1a" },
        { "[prc_verse_1a]",     "verse_1a" },
    };

    for (const auto& c : cases)
    {
        auto s = built({ {4.0, c.text} });
        INFO(c.text);
        REQUIRE(s.getSections().size() == 1);
        REQUIRE(s.getSections()[0].name == juce::String(c.name));
        REQUIRE(s.getSections()[0].position == PPQ(4.0));
    }
}

TEST_CASE("SongSections - non-section events are ignored", "[sections]")
{
    auto s = built({ {0.0, "[music_start]"}, {1.0, "[crowd_normal]"},
                     {2.0, "[end]"}, {3.0, "not bracketed"}, {4.0, "[]"} });
    REQUIRE(s.getSections().empty());
    REQUIRE_FALSE(s.hasSections());
}

TEST_CASE("SongSections - sections sort by position", "[sections]")
{
    auto s = built({ {40.0, "[section outro]"}, {8.0, "[section intro]"},
                     {20.0, "[section verse]"} });
    REQUIRE(s.getSections().size() == 3);
    REQUIRE(s.getSections()[0].name == "intro");
    REQUIRE(s.getSections()[1].name == "verse");
    REQUIRE(s.getSections()[2].name == "outro");
}

TEST_CASE("SongSections - sectionAt finds the covering section", "[sections]")
{
    auto s = built({ {8.0, "[section intro]"}, {20.0, "[section verse]"} });

    REQUIRE(s.sectionAt(PPQ(4.0)) == nullptr);           // before the first
    REQUIRE(s.sectionAt(PPQ(8.0))->name == "intro");     // exactly on it
    REQUIRE(s.sectionAt(PPQ(19.9))->name == "intro");
    REQUIRE(s.sectionAt(PPQ(20.0))->name == "verse");
    REQUIRE(s.sectionAt(PPQ(999.0))->name == "verse");   // past the last
}

TEST_CASE("SongSections - coda marks where fills become BREs", "[sections]")
{
    auto none = built({ {8.0, "[section intro]"} });
    REQUIRE_FALSE(none.hasCoda());
    REQUIRE_FALSE(none.isAfterCoda(PPQ(999.0)));

    auto s = built({ {8.0, "[section intro]"}, {640.0, "[coda]"} });
    REQUIRE(s.hasCoda());
    REQUIRE(s.getCodaPPQ() == PPQ(640.0));
    REQUIRE_FALSE(s.isAfterCoda(PPQ(639.0)));
    REQUIRE(s.isAfterCoda(PPQ(640.0)));
    REQUIRE(s.isAfterCoda(PPQ(642.0)));
    REQUIRE(s.getSections().size() == 1);   // coda is not a section
}

// The real EVENTS track from Job for a Cowboy - Embedded (Harmonix), inlined rather than
// read from disk so the test does not depend on a chart we cannot commit. Spaced form.
TEST_CASE("SongSections - Embedded EVENTS track", "[sections]")
{
    auto s = built({
        {  16.00, "[section verse_1a]" },   {  16.00, "[crowd_noclap]" },
        {  16.00, "[crowd_intense]" },      {  17.00, "[music_start]" },
        {  88.00, "[section verse_1b]" },   { 142.00, "[section chorus_1a]" },
        { 174.00, "[section chorus_1b]" },  { 206.00, "[section main_riff_1]" },
        { 238.00, "[section verse_2a]" },   { 270.00, "[section verse_2b]" },
        { 310.00, "[section verse_2c]" },   { 374.00, "[section break]" },
        { 438.00, "[section chorus_2a]" },  { 470.00, "[section chorus_2b]" },
        { 502.00, "[section gtr_solo_a]" }, { 538.00, "[section gtr_solo_b]" },
        { 566.00, "[section outro]" },      { 629.50, "[music_end]" },
        { 638.00, "[end]" },                { 640.00, "[coda]" },
    });

    REQUIRE(s.getSections().size() == 14);
    REQUIRE(s.getSections().front().name == "verse_1a");
    REQUIRE(s.getSections().back().name == "outro");
    REQUIRE(s.sectionAt(PPQ(520.0))->name == "gtr_solo_a");

    // The coda sits past [end], so the trailing fill is a BRE and the 33 before it are not.
    REQUIRE(s.hasCoda());
    REQUIRE_FALSE(s.isAfterCoda(PPQ(610.0)));
    REQUIRE(s.isAfterCoda(PPQ(642.0)));
}

// Interloper - Idle Years, same era of tooling, prc_ form. No coda.
TEST_CASE("SongSections - Interloper EVENTS track", "[sections]")
{
    auto s = built({
        {    0.00, "[crowd_normal]" },      {   22.00, "[music_start]" },
        {   76.00, "[prc_intro_a]" },       {  144.00, "[prc_intro_b]" },
        {  208.00, "[prc_verse_1]" },       {  336.00, "[prc_prechorus_1]" },
        {  400.00, "[prc_chorus_1]" },      {  496.00, "[prc_interlude]" },
        {  528.00, "[prc_bridge]" },        {  624.00, "[prc_gtr_solo_a]" },
        {  752.00, "[prc_gtr_solo_b]" },    {  816.00, "[prc_verse_2]" },
        {  944.00, "[prc_prechorus_2a]" },  { 1008.00, "[prc_prechorus_2b]" },
        { 1072.00, "[prc_chorus_2]" },      { 1168.00, "[prc_postchorus]" },
        { 1264.00, "[prc_chorus_3]" },      { 1328.00, "[prc_outro]" },
        { 1398.00, "[music_end]" },         { 1512.00, "[end]" },
    });

    REQUIRE(s.getSections().size() == 16);
    REQUIRE(s.getSections().front().name == "intro_a");
    REQUIRE(s.getSections().back().name == "outro");
    REQUIRE(s.sectionAt(PPQ(700.0))->name == "gtr_solo_a");
    REQUIRE_FALSE(s.hasCoda());
}

// Two charts on one timeline: each carries its own [music_end] + [end], and the first
// chart's last section has to stop at them rather than run into the second chart.
TEST_CASE("SongSections - end markers bound each clip", "[sections]")
{
    auto s = built({ {8.0,    "[section intro]"},
                     {40.0,   "[section outro]"},
                     {64.0,   "[music_end]"},
                     {68.0,   "[end]"},
                     {200.0,  "[section verse_1]"},
                     {260.0,  "[music_end]"},
                     {264.0,  "[end]"} });

    REQUIRE(s.getSections().size() == 3);

    // The later marker of a pair wins, so a band stops at [end], not at [music_end].
    REQUIRE(s.endMarkerBetween(PPQ(40.0), PPQ(200.0)) == PPQ(68.0));
    REQUIRE(s.endMarkerBetween(PPQ(200.0), PPQ(1.0e12)) == PPQ(264.0));

    // No marker in the span: `from` comes back, meaning nothing cuts this section short.
    REQUIRE(s.endMarkerBetween(PPQ(8.0), PPQ(40.0)) == PPQ(8.0));

    // Still the last one overall, which is what the whole-song zoom spans.
    REQUIRE(s.getEndPPQ() == PPQ(264.0));
}

// A section rewritten by the plugin drifts a tick or two earlier each time it is saved, so
// a project edited a dozen times ends up with a ladder of copies behind every real marker.
// The numbers here are taken from a real project: everlong's [section intro] sits 12 QN
// into a clip starting at 2048, and five drifted copies had piled up behind it.
TEST_CASE("SongSections - markers a couple of ticks apart are one section", "[sections]")
{
    const double tick = 1.0 / PPQ::PPQ_RESOLUTION;

    auto s = built({ {2060.0 - 10 * tick, "[section intro]"},
                     {2060.0 -  8 * tick, "[section intro]"},
                     {2060.0 -  6 * tick, "[section intro]"},
                     {2060.0 -  4 * tick, "[section intro]"},
                     {2060.0 -  2 * tick, "[section intro]"},
                     {2060.0,             "[section intro]"} });

    REQUIRE(s.getSections().size() == 1);

    // The survivor is the one on the true position, not a drifted copy.
    REQUIRE(s.getSections()[0].position == PPQ(2060.0));
    REQUIRE(s.getSections()[0].name == "intro");
}

TEST_CASE("SongSections - a rename keeps the newest name", "[sections]")
{
    const double tick = 1.0 / PPQ::PPQ_RESOLUTION;

    auto s = built({ {100.0 - 4 * tick, "[section old_name]"},
                     {100.0 - 2 * tick, "[section newer_name]"},
                     {100.0,            "[section newest_name]"} });

    REQUIRE(s.getSections().size() == 1);
    REQUIRE(s.getSections()[0].name == "newest_name");
}

// The tightest gap between two genuine consecutive sections across every chart in
// assets/midi is 4 QN. Nothing anywhere near the collapse threshold.
TEST_CASE("SongSections - real sections are never collapsed", "[sections]")
{
    auto s = built({ {0.0,   "[section intro]"},
                     {4.0,   "[section verse_1]"},     // the tightest real spacing seen
                     {8.0,   "[section chorus_1]"},
                     {12.0,  "[section verse_2]"} });

    REQUIRE(s.getSections().size() == 4);
    REQUIRE(s.getSections()[1].name == "verse_1");

    // Even a gap well under the tightest real one survives, so long as it clears the
    // threshold that separates a section from a rewrite of the same section.
    auto tight = built({ {0.0,  "[section a]"},
                         {0.25, "[section b]"} });
    REQUIRE(tight.getSections().size() == 2);
}

// Venue cues read exactly like section names and ship in the same track.
TEST_CASE("SongSections - singalong cues are not sections", "[sections]")
{
    auto s = built({ {0.0,  "[section intro]"},
                     {8.0,  "[chorus]"},
                     {16.0, "[verse]"},
                     {24.0, "[crowd_normal]"},
                     {32.0, "[section chorus_1]"},
                     {40.0, "[end_pg song_trainer_pg_1]"} });

    REQUIRE(s.getSections().size() == 2);
    REQUIRE(s.getSections()[0].name == "intro");
    REQUIRE(s.getSections()[1].name == "chorus_1");

    // The trainer event must not have ended the song.
    REQUIRE(s.getEndPPQ() == PPQ(0.0));
}

TEST_CASE("SectionRenumber: each song counts its families from 1", "[sections]")
{
    struct S { double qn; std::string name; };
    // Two songs split at QN 100, numbered as if the project were one long song.
    std::vector<S> list = { { 0, "Section 1" }, { 10, "Section 2" }, { 20, "Verse 3" },
                            { 100, "Section 3" }, { 110, "Section 4" }, { 120, "Verse 9" } };

    SectionRenumber::renumber(list, [](const S& s) { return s.qn < 100 ? 0 : 1; });

    REQUIRE(list[0].name == "Section 1");
    REQUIRE(list[1].name == "Section 2");
    REQUIRE(list[3].name == "Section 1");
    REQUIRE(list[4].name == "Section 2");
    // A family of one per song is left alone, even when the project holds two of them.
    REQUIRE(list[2].name == "Verse 3");
    REQUIRE(list[5].name == "Verse 9");
}
