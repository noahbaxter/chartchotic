#include "test_helpers.h"
#include "Editor/SelectionMath.h"

namespace
{
    SelectedNote sel(double qn, int pitch, int lane = 1) { return { 0, qn, qn, pitch, lane, false }; }
}

TEST_CASE("SelectionMath - toggling adds what is new and drops what was there", "[selection]")
{
    const std::vector<SelectedNote> base = { sel(1.0, 60), sel(2.0, 61) };

    auto out = SelectionMath::toggle(base, { sel(2.0, 61), sel(3.0, 62) });

    REQUIRE(out.size() == 2);
    REQUIRE(SelectionMath::sameNote(out[0], sel(1.0, 60)));   // untouched
    REQUIRE(SelectionMath::sameNote(out[1], sel(3.0, 62)));   // added; 2.0 dropped
}

TEST_CASE("SelectionMath - a note is identified by track, time and pitch", "[selection]")
{
    REQUIRE(SelectionMath::sameNote(sel(1.0, 60), sel(1.0 + 1e-6, 60)));
    REQUIRE_FALSE(SelectionMath::sameNote(sel(1.0, 60), sel(1.0, 61)));

    SelectedNote other = sel(1.0, 60);
    other.trackIdx = 1;
    REQUIRE_FALSE(SelectionMath::sameNote(sel(1.0, 60), other));
}

TEST_CASE("SelectionMath - toggling the same notes twice gets back the start", "[selection]")
{
    const std::vector<SelectedNote> base = { sel(1.0, 60) };
    const std::vector<SelectedNote> box  = { sel(1.0, 60), sel(4.0, 63) };

    auto twice = SelectionMath::toggle(SelectionMath::toggle(base, box), box);
    REQUIRE(twice.size() == 1);
    REQUIRE(SelectionMath::sameNote(twice[0], sel(1.0, 60)));
}
