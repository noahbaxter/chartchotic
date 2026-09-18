#include "test_helpers.h"
#include <catch2/catch_approx.hpp>   // test_helpers.h pulls in the macros only
#include "Visual/PhraseMath.h"

using namespace PhraseMath;

namespace
{
    struct Note { PPQ startPPQ; PPQ endPPQ; uint pitch; };

    std::vector<int> sorted(std::vector<int> v)
    {
        std::sort(v.begin(), v.end());
        return v;
    }
}

//==============================================================================
// pitches

TEST_CASE("PhraseMath - star power is 116, except on elite where it is 104", "[stripphrase]")
{
    REQUIRE(pitches(Kind::StarPower, Part::GUITAR) == std::vector<int>{ 116 });
    REQUIRE(pitches(Kind::StarPower, Part::DRUMS) == std::vector<int>{ 116 });
    REQUIRE(pitches(Kind::StarPower, Part::ELITE_DRUMS) == std::vector<int>{ 104 });
}

TEST_CASE("PhraseMath - solo is 103 on every part", "[stripphrase]")
{
    REQUIRE(pitches(Kind::Solo, Part::BASS) == std::vector<int>{ 103 });
    REQUIRE(pitches(Kind::Solo, Part::ELITE_DRUMS) == std::vector<int>{ 103 });
}

TEST_CASE("PhraseMath - a fill is a drum phrase and nothing else has one", "[stripphrase]")
{
    REQUIRE(sorted(pitches(Kind::Fill, Part::DRUMS)) == std::vector<int>{ 120, 121, 122, 123, 124 });
    REQUIRE(pitches(Kind::Fill, Part::ELITE_DRUMS) == std::vector<int>{ 120 });
    REQUIRE(pitches(Kind::Fill, Part::GUITAR).empty());
}

TEST_CASE("PhraseMath - BRE marker count is per part", "[stripphrase]")
{
    // Getting this wrong writes a phrase no game reads, so each family is pinned.
    REQUIRE(sorted(pitches(Kind::Bre, Part::GUITAR)) == std::vector<int>{ 120, 121, 122, 123, 124 });
    REQUIRE(sorted(pitches(Kind::Bre, Part::DRUMS))  == std::vector<int>{ 120, 121, 122, 123, 124 });
    REQUIRE(sorted(pitches(Kind::Bre, Part::PRO_GUITAR))
            == std::vector<int>{ 120, 121, 122, 123, 124, 125 });
    REQUIRE(pitches(Kind::Bre, Part::PRO_KEYS)    == std::vector<int>{ 120 });
    REQUIRE(pitches(Kind::Bre, Part::ELITE_DRUMS) == std::vector<int>{ 120 });

    // 6-fret has no BRE notes at all.
    REQUIRE(pitches(Kind::Bre, Part::GHL_GUITAR).empty());
}

//==============================================================================
// what the picker offers

TEST_CASE("PhraseMath - drums can write a fill, other parts cannot", "[stripphrase]")
{
    REQUIRE(kindsFor(Part::DRUMS)
            == std::vector<Kind>{ Kind::StarPower, Kind::Solo, Kind::Fill, Kind::Bre });
    REQUIRE(kindsFor(Part::GUITAR)
            == std::vector<Kind>{ Kind::StarPower, Kind::Solo, Kind::Bre });

    // 6-fret has no BRE either, so it is down to two.
    REQUIRE(kindsFor(Part::GHL_GUITAR) == std::vector<Kind>{ Kind::StarPower, Kind::Solo });
}

TEST_CASE("PhraseMath - every kind the picker offers has notes to write", "[stripphrase]")
{
    // Otherwise the picker offers a phrase that silently writes nothing.
    for (Part part : { Part::DRUMS, Part::ELITE_DRUMS, Part::GUITAR, Part::BASS,
                       Part::GHL_GUITAR, Part::PRO_GUITAR, Part::PRO_KEYS })
        for (Kind kind : kindsFor(part))
            REQUIRE_FALSE(pitches(kind, part).empty());
}

//==============================================================================
// rangesOf

TEST_CASE("PhraseMath - ranges are read off one pitch and come back sorted", "[stripphrase]")
{
    std::vector<Note> notes = {
        { PPQ(32.0), PPQ(40.0), 116 },
        { PPQ(4.0),  PPQ(8.0),  116 },
        { PPQ(4.0),  PPQ(6.0),  96 },    // a gem, not a phrase
    };

    auto r = rangesOf(notes, 116);
    REQUIRE(r.size() == 2);
    REQUIRE(r[0].startQN == Catch::Approx(4.0));
    REQUIRE(r[0].endQN == Catch::Approx(8.0));
    REQUIRE(r[1].startQN == Catch::Approx(32.0));
}

//==============================================================================
// phraseAt

TEST_CASE("PhraseMath - a position inside a phrase finds it, its end does not", "[stripphrase]")
{
    std::vector<PhraseSpan> r = { { 4.0, 8.0 }, { 16.0, 20.0 } };

    REQUIRE(phraseAt(r, 4.0) == 0);
    REQUIRE(phraseAt(r, 7.9) == 0);
    REQUIRE(phraseAt(r, 8.0) == -1);
    REQUIRE(phraseAt(r, 17.0) == 1);
    REQUIRE(phraseAt(r, 12.0) == -1);
}

//==============================================================================
// clampEnd

TEST_CASE("PhraseMath - an end is held short of the next phrase", "[stripphrase]")
{
    std::vector<PhraseSpan> others = { { 16.0, 20.0 } };

    REQUIRE(clampEnd(4.0, 12.0, others) == Catch::Approx(12.0));
    REQUIRE(clampEnd(4.0, 24.0, others) == Catch::Approx(16.0));
}

TEST_CASE("PhraseMath - a phrase behind the start does not clamp the end", "[stripphrase]")
{
    std::vector<PhraseSpan> others = { { 0.0, 2.0 } };

    REQUIRE(clampEnd(4.0, 12.0, others) == Catch::Approx(12.0));
}

//==============================================================================
// isBre

TEST_CASE("PhraseMath - a fill at or after the coda is a BRE", "[stripphrase]")
{
    REQUIRE_FALSE(isBre(10.0, std::nullopt));
    REQUIRE_FALSE(isBre(10.0, 20.0));
    REQUIRE(isBre(20.0, 20.0));
    REQUIRE(isBre(24.0, 20.0));
}

