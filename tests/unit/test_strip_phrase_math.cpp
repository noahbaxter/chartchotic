#include "test_helpers.h"
#include <catch2/catch_approx.hpp>   // test_helpers.h pulls in the macros only
#include "Visual/StripPhraseMath.h"

using namespace StripPhraseMath;

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

TEST_CASE("StripPhraseMath - star power is 116, except on elite where it is 104", "[stripphrase]")
{
    REQUIRE(pitches(Kind::StarPower, Part::GUITAR) == std::vector<int>{ 116 });
    REQUIRE(pitches(Kind::StarPower, Part::DRUMS) == std::vector<int>{ 116 });
    REQUIRE(pitches(Kind::StarPower, Part::ELITE_DRUMS) == std::vector<int>{ 104 });
}

TEST_CASE("StripPhraseMath - solo is 103 on every part", "[stripphrase]")
{
    REQUIRE(pitches(Kind::Solo, Part::BASS) == std::vector<int>{ 103 });
    REQUIRE(pitches(Kind::Solo, Part::ELITE_DRUMS) == std::vector<int>{ 103 });
}

TEST_CASE("StripPhraseMath - a fill or BRE is all five markers, elite only 120", "[stripphrase]")
{
    REQUIRE(sorted(pitches(Kind::FillBre, Part::GUITAR)) == std::vector<int>{ 120, 121, 122, 123, 124 });
    REQUIRE(sorted(pitches(Kind::FillBre, Part::DRUMS)) == std::vector<int>{ 120, 121, 122, 123, 124 });
    REQUIRE(pitches(Kind::FillBre, Part::ELITE_DRUMS) == std::vector<int>{ 120 });
}

//==============================================================================
// rangesOf

TEST_CASE("StripPhraseMath - ranges are read off one pitch and come back sorted", "[stripphrase]")
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

TEST_CASE("StripPhraseMath - a position inside a phrase finds it, its end does not", "[stripphrase]")
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

TEST_CASE("StripPhraseMath - an end is held short of the next phrase", "[stripphrase]")
{
    std::vector<PhraseSpan> others = { { 16.0, 20.0 } };

    REQUIRE(clampEnd(4.0, 12.0, others) == Catch::Approx(12.0));
    REQUIRE(clampEnd(4.0, 24.0, others) == Catch::Approx(16.0));
}

TEST_CASE("StripPhraseMath - a phrase behind the start does not clamp the end", "[stripphrase]")
{
    std::vector<PhraseSpan> others = { { 0.0, 2.0 } };

    REQUIRE(clampEnd(4.0, 12.0, others) == Catch::Approx(12.0));
}

//==============================================================================
// isBre

TEST_CASE("StripPhraseMath - a fill at or after the coda is a BRE", "[stripphrase]")
{
    REQUIRE_FALSE(isBre(10.0, std::nullopt));
    REQUIRE_FALSE(isBre(10.0, 20.0));
    REQUIRE(isBre(20.0, 20.0));
    REQUIRE(isBre(24.0, 20.0));
}

//==============================================================================
// planFillBre

TEST_CASE("StripPhraseMath - a drum phrase before the coda is a fill and leaves it alone", "[stripphrase]")
{
    auto plan = planFillBre(8.0, Part::DRUMS, false, 40.0, {});

    REQUIRE_FALSE(plan.bre);
    REQUIRE(plan.coda == CodaChange::Keep);
}

TEST_CASE("StripPhraseMath - a drum phrase past the coda is a BRE, never a stray fill", "[stripphrase]")
{
    auto plan = planFillBre(48.0, Part::DRUMS, false, 40.0, {});

    REQUIRE(plan.bre);
    REQUIRE(plan.coda == CodaChange::Keep);
}

TEST_CASE("StripPhraseMath - a BRE with no coda places one at its start", "[stripphrase]")
{
    auto plan = planFillBre(48.0, Part::GUITAR, false, std::nullopt, {});

    REQUIRE(plan.bre);
    REQUIRE(plan.coda == CodaChange::Place);
    REQUIRE(plan.codaQN == Catch::Approx(48.0));
}

TEST_CASE("StripPhraseMath - a BRE before the coda pulls the coda back to it", "[stripphrase]")
{
    auto plan = planFillBre(32.0, Part::GUITAR, false, 40.0, {});

    REQUIRE(plan.coda == CodaChange::Place);
    REQUIRE(plan.codaQN == Catch::Approx(32.0));
}

TEST_CASE("StripPhraseMath - placing a coda that would turn drum fills into BREs is refused",
          "[stripphrase]")
{
    std::vector<PhraseSpan> drumFills = { { 50.0, 52.0 } };
    auto plan = planFillBre(48.0, Part::GUITAR, false, std::nullopt, drumFills);

    REQUIRE(plan.coda == CodaChange::Refuse);
    REQUIRE(plan.reinterpreted == 1);
}

TEST_CASE("StripPhraseMath - drum phrases already past the old coda do not block moving it back",
          "[stripphrase]")
{
    // The one at 44 is already a BRE, so pulling the coda from 40 to 32 changes nothing about it.
    std::vector<PhraseSpan> drumFills = { { 44.0, 46.0 } };
    auto plan = planFillBre(32.0, Part::GUITAR, false, 40.0, drumFills);

    REQUIRE(plan.coda == CodaChange::Place);
}

TEST_CASE("StripPhraseMath - asking for a BRE on drums follows the guitar rules", "[stripphrase]")
{
    auto plan = planFillBre(48.0, Part::DRUMS, true, std::nullopt, {});

    REQUIRE(plan.bre);
    REQUIRE(plan.coda == CodaChange::Place);
}

//==============================================================================
// codaOrphaned

TEST_CASE("StripPhraseMath - a coda with no BRE left after it is orphaned", "[stripphrase]")
{
    REQUIRE_FALSE(codaOrphaned(std::nullopt, {}));
    REQUIRE(codaOrphaned(40.0, {}));
    REQUIRE(codaOrphaned(40.0, { { 8.0, 12.0 } }));
    REQUIRE_FALSE(codaOrphaned(40.0, { { 40.0, 48.0 } }));
}
