// Every case here is a text event that actually appears in assets/midi, pulled from the
// shipped charts rather than invented, plus the spec entries those charts happen not to use.
#include <catch2/catch_test_macros.hpp>

#include "Midi/EventsTrackText.h"

using EventsTrackText::Kind;
using EventsTrackText::classify;

namespace
{
    Kind kindOf(const char* raw) { return classify(juce::String(raw)).kind; }
    juce::String nameOf(const char* raw) { return classify(juce::String(raw)).value; }
}

TEST_CASE("EventsTrackText - section spellings", "[events]")
{
    REQUIRE(kindOf("[section intro]") == Kind::Section);
    REQUIRE(kindOf("[section_intro]") == Kind::Section);
    REQUIRE(kindOf("[prc_intro]") == Kind::Section);

    REQUIRE(nameOf("[section main_riff_1]") == "main_riff_1");
    REQUIRE(nameOf("[section_gtr_solo_a]") == "gtr_solo_a");
    REQUIRE(nameOf("[prc_chorus_1]") == "chorus_1");
}

TEST_CASE("EventsTrackText - singalong cues are not sections", "[events]")
{
    // The whole point of classifying. Both ship in RB charts as venue cues, and both read
    // exactly like a section name would.
    REQUIRE(kindOf("[chorus]") == Kind::Venue);
    REQUIRE(kindOf("[verse]") == Kind::Venue);

    REQUIRE(kindOf("[chorus]") != Kind::Section);
    REQUIRE(kindOf("[verse]") != Kind::Section);
}

TEST_CASE("EventsTrackText - song boundaries", "[events]")
{
    REQUIRE(kindOf("[end]") == Kind::End);
    REQUIRE(kindOf("[music_start]") == Kind::MusicStart);
    REQUIRE(kindOf("[music_end]") == Kind::MusicEnd);
    REQUIRE(kindOf("[coda]") == Kind::Coda);
}

TEST_CASE("EventsTrackText - trainer events are not the song end", "[events]")
{
    // [end_pg ...] starts with "end". Matching a prefix here would end the song early.
    REQUIRE(kindOf("[end_pg song_trainer_pg_1]") == Kind::Trainer);
    REQUIRE(kindOf("[end_pb song_trainer_pb_2]") == Kind::Trainer);
    REQUIRE(kindOf("[end_key song_trainer_key_3]") == Kind::Trainer);
    REQUIRE(kindOf("[begin_pg song_trainer_pg_1]") == Kind::Trainer);

    REQUIRE(kindOf("[end_pg song_trainer_pg_1]") != Kind::End);
}

TEST_CASE("EventsTrackText - venue, camera, crowd and mix", "[events]")
{
    REQUIRE(kindOf("[crowd_normal]") == Kind::Crowd);
    REQUIRE(kindOf("[crowd_noclap]") == Kind::Crowd);

    REQUIRE(kindOf("[coop_bg_near]") == Kind::Venue);
    REQUIRE(kindOf("[coop_g_closeup_hand]") == Kind::Venue);
    REQUIRE(kindOf("[ProFilm_a.pp]") == Kind::Venue);
    REQUIRE(kindOf("[FogOn]") == Kind::Venue);
    REQUIRE(kindOf("[FogOff]") == Kind::Venue);
    REQUIRE(kindOf("[bonusfx]") == Kind::Venue);
    REQUIRE(kindOf("[play]") == Kind::Venue);
    REQUIRE(kindOf("[play_solo]") == Kind::Venue);
    REQUIRE(kindOf("[next]") == Kind::Venue);
    REQUIRE(kindOf("[idle_realtime]") == Kind::Venue);

    REQUIRE(kindOf("[do_directed_cut directed_guitar_cls]") == Kind::DirectedCut);
    REQUIRE(kindOf("[mix 3 drums0]") == Kind::Mix);
    REQUIRE(kindOf("[map HandMap_DropD2]") == Kind::HandMap);
    REQUIRE(kindOf("[ENABLE_CHART_DYNAMICS]") == Kind::ChartFlag);
}

TEST_CASE("EventsTrackText - a leading space inside the brackets still parses", "[events]")
{
    // Ships exactly like this in at least one chart in assets/midi.
    REQUIRE(kindOf("[ map StrumMap_Pick]") == Kind::HandMap);
}

TEST_CASE("EventsTrackText - malformed input is Unknown, never a section", "[events]")
{
    REQUIRE(kindOf("section intro") == Kind::Unknown);   // no brackets
    REQUIRE(kindOf("[section ]") == Kind::Unknown);      // no name
    REQUIRE(kindOf("[]") == Kind::Unknown);
    REQUIRE(kindOf("") == Kind::Unknown);
    REQUIRE(kindOf("[unrecognised_thing]") == Kind::Unknown);
}

TEST_CASE("EventsTrackText - only structure counts as structure", "[events]")
{
    REQUIRE(EventsTrackText::isStructural(Kind::Section));
    REQUIRE(EventsTrackText::isStructural(Kind::End));
    REQUIRE(EventsTrackText::isStructural(Kind::MusicEnd));
    REQUIRE(EventsTrackText::isStructural(Kind::Coda));

    REQUIRE_FALSE(EventsTrackText::isStructural(Kind::Venue));
    REQUIRE_FALSE(EventsTrackText::isStructural(Kind::Crowd));
    REQUIRE_FALSE(EventsTrackText::isStructural(Kind::Trainer));
    REQUIRE_FALSE(EventsTrackText::isStructural(Kind::Mix));
    REQUIRE_FALSE(EventsTrackText::isStructural(Kind::Unknown));
}
