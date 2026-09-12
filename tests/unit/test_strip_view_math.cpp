#include "test_helpers.h"
#include <catch2/catch_approx.hpp>   // test_helpers.h pulls in the macros only
#include "Visual/StripViewMath.h"

using namespace StripViewMath;

namespace
{
    constexpr double PROJECT_END = 8000.0;
    constexpr double MIN_SPAN = 2.0;

    // One chart sitting in the middle of the timeline, with room either side to scroll into.
    constexpr double SONG_START = 1000.0;
    constexpr double SONG_END = 1400.0;

    constexpr int RESET_MS = 400;
    constexpr double THRESHOLD = 0.12;
}

//==============================================================================
// clamp

TEST_CASE("StripViewMath - the view is held inside the project, not inside a chart",
          "[stripview]")
{
    auto v = clamp(-50.0, 64.0, PROJECT_END, MIN_SPAN);

    REQUIRE(v.startQN == Catch::Approx(0.0));
    REQUIRE(v.spanQN == Catch::Approx(64.0));
}

TEST_CASE("StripViewMath - a view running off the end slides back to fit", "[stripview]")
{
    auto v = clamp(PROJECT_END - 10.0, 64.0, PROJECT_END, MIN_SPAN);

    REQUIRE(v.startQN == Catch::Approx(PROJECT_END - 64.0));
    REQUIRE(v.spanQN == Catch::Approx(64.0));
}

// Zooming out past the whole timeline is the far end of the range, not an error.
TEST_CASE("StripViewMath - a span wider than the project becomes the project", "[stripview]")
{
    auto v = clamp(0.0, PROJECT_END * 4.0, PROJECT_END, MIN_SPAN);

    REQUIRE(v.spanQN == Catch::Approx(PROJECT_END));
    REQUIRE(v.startQN == Catch::Approx(0.0));
}

TEST_CASE("StripViewMath - a span cannot go below the floor", "[stripview]")
{
    REQUIRE(clamp(0.0, 0.001, PROJECT_END, MIN_SPAN).spanQN == Catch::Approx(MIN_SPAN));
}

//==============================================================================
// zoomSpansQN

namespace
{
    // 4 QN to the bar throughout, so a rung's span is bars * 4.
    std::vector<double> barsOf(const std::vector<double>& spans, double barQN = 4.0)
    {
        std::vector<double> bars;
        for (double s : spans) bars.push_back(s / barQN);
        return bars;
    }
}

// 2 and 4, then every power of two from 8 with its halfway point.
TEST_CASE("StripViewMath - the ladder is 2 4 8 12 16 24 32 48 64", "[stripview]")
{
    auto bars = barsOf(zoomSpansQN(1000.0 * 4.0, 4.0, 2.0));

    REQUIRE(std::vector<double>(bars.begin(), bars.begin() + 9)
            == std::vector<double>{ 2, 4, 8, 12, 16, 24, 32, 48, 64 });
}

// Zooming all the way out lands on the project itself, not on a rung past the end of it.
TEST_CASE("StripViewMath - the whole project is always the top rung", "[stripview]")
{
    double project = 40.0 * 4.0;
    auto spans = zoomSpansQN(project, 4.0, 2.0);

    REQUIRE(spans.back() == Catch::Approx(project));
    REQUIRE(barsOf(spans) == std::vector<double>{ 2, 4, 8, 12, 16, 24, 32, 40 });
}

TEST_CASE("StripViewMath - a project shorter than the floor is its own only rung",
          "[stripview]")
{
    auto spans = zoomSpansQN(4.0, 4.0, 2.0);

    REQUIRE(spans == std::vector<double>{ 4.0 });
}

// Bars are twice as long in 8/4, so the same rung is twice the quarter notes.
TEST_CASE("StripViewMath - rungs follow bar length, not quarter notes", "[stripview]")
{
    auto spans = zoomSpansQN(1000.0, 8.0, 2.0);

    REQUIRE(spans[0] == Catch::Approx(16.0));
    REQUIRE(spans[1] == Catch::Approx(32.0));
}

//==============================================================================
// nextSpanQN

TEST_CASE("StripViewMath - stepping walks the ladder both ways", "[stripview]")
{
    auto spans = zoomSpansQN(1000.0 * 4.0, 4.0, 2.0);

    REQUIRE(nextSpanQN(spans, 16.0 * 4.0, 1) == Catch::Approx(24.0 * 4.0));
    REQUIRE(nextSpanQN(spans, 16.0 * 4.0, -1) == Catch::Approx(12.0 * 4.0));
}

// Straight after fitting a chart the span is the chart's length, which is not a rung. The
// notch has to move you, not spend itself snapping sideways.
TEST_CASE("StripViewMath - a span off the ladder steps to the next rung", "[stripview]")
{
    auto spans = zoomSpansQN(1000.0 * 4.0, 4.0, 2.0);
    double odd = 37.0 * 4.0;   // between the 32 and 48 rungs

    REQUIRE(nextSpanQN(spans, odd, 1) == Catch::Approx(48.0 * 4.0));
    REQUIRE(nextSpanQN(spans, odd, -1) == Catch::Approx(32.0 * 4.0));
}

TEST_CASE("StripViewMath - stepping past either end stays put", "[stripview]")
{
    auto spans = zoomSpansQN(1000.0 * 4.0, 4.0, 2.0);

    REQUIRE(nextSpanQN(spans, spans.front(), -1) == Catch::Approx(spans.front()));
    REQUIRE(nextSpanQN(spans, spans.back(), 1) == Catch::Approx(spans.back()));
}

//==============================================================================
// spanAbout

// The whole point of anchoring: the moment under the pointer stays under the pointer.
TEST_CASE("StripViewMath - zooming holds the anchored position still", "[stripview]")
{
    View before { 1000.0, 400.0 };
    double anchor = 0.25;
    double held = before.startQN + anchor * before.spanQN;   // 1100

    auto after = spanAbout(before, anchor, 200.0, PROJECT_END, MIN_SPAN);

    REQUIRE(after.spanQN == Catch::Approx(200.0));
    REQUIRE(after.startQN + anchor * after.spanQN == Catch::Approx(held));
}

TEST_CASE("StripViewMath - zooming out holds the anchored position too", "[stripview]")
{
    View before { 1000.0, 400.0 };
    double anchor = 0.75;
    double held = before.startQN + anchor * before.spanQN;   // 1300

    auto after = spanAbout(before, anchor, 800.0, PROJECT_END, MIN_SPAN);

    REQUIRE(after.spanQN == Catch::Approx(800.0));
    REQUIRE(after.startQN + anchor * after.spanQN == Catch::Approx(held));
}

//==============================================================================
// clusterSpans

namespace
{
    std::vector<NoteSpan> spansOf(std::initializer_list<std::pair<double, double>> items)
    {
        std::vector<NoteSpan> out;
        for (const auto& [s, e] : items) out.push_back({ s, e });
        return out;
    }

    constexpr double GAP = 16.0;   // 4 bars of 4 QN
}

// The whole point: a scrap of MIDI is one place to stop, not one per note in it.
TEST_CASE("StripViewMath - notes close together are one cluster", "[stripview]")
{
    auto out = clusterSpans(spansOf({ {100, 101}, {102, 103}, {104, 108} }), GAP);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].startQN == Catch::Approx(100.0));
    REQUIRE(out[0].endQN == Catch::Approx(108.0));
}

TEST_CASE("StripViewMath - silence longer than the gap splits a cluster", "[stripview]")
{
    auto out = clusterSpans(spansOf({ {100, 104}, {200, 204} }), GAP);

    REQUIRE(out.size() == 2);
    REQUIRE(out[1].startQN == Catch::Approx(200.0));
}

// The gap is what separates clusters, not the run length, so a sparse passage stays one
// thing rather than fragmenting into a stop per phrase.
TEST_CASE("StripViewMath - a sparse run stays one cluster", "[stripview]")
{
    auto out = clusterSpans(spansOf({ {0, 1}, {15, 16}, {30, 31}, {45, 46} }), GAP);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].endQN == Catch::Approx(46.0));
}

// A long note covering the ones after it must not let the cluster end early.
TEST_CASE("StripViewMath - a cluster takes the furthest end, not the last", "[stripview]")
{
    auto out = clusterSpans(spansOf({ {100, 180}, {104, 108} }), GAP);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].endQN == Catch::Approx(180.0));
}

TEST_CASE("StripViewMath - no notes means no clusters", "[stripview]")
{
    REQUIRE(clusterSpans({}, GAP).empty());
}

//==============================================================================
// mergedPoints

TEST_CASE("StripViewMath - coincident snap points are kept once", "[stripview]")
{
    auto out = mergedPoints({ 400.0, 100.0, 400.0, 250.0 }, 1.0e-6);

    REQUIRE(out == std::vector<double>{ 100.0, 250.0, 400.0 });
}

// A section end that is just the next section's start is a duplicate; one a hair off is not.
TEST_CASE("StripViewMath - points merge only inside the tolerance", "[stripview]")
{
    auto out = mergedPoints({ 100.0, 100.0 + 1.0e-9, 100.5 }, 1.0e-6);

    REQUIRE(out.size() == 2);
    REQUIRE(out[1] == Catch::Approx(100.5));
}

//==============================================================================
// BoundaryDetent

namespace
{
    // A view sitting inside the chart, small enough that the detent applies.
    constexpr double SPAN = 100.0;

    // Enough push to cross in one go: threshold is 0.12 of the span.
    constexpr double HARD_PUSH = SPAN * THRESHOLD + 1.0;
    constexpr double SOFT_PUSH = SPAN * THRESHOLD * 0.5;

    double resolveAt(BoundaryDetent& d, double wantStart, long long nowMs)
    {
        return d.resolve(wantStart, SPAN, SONG_START, SONG_END, nowMs, RESET_MS, THRESHOLD);
    }
}

TEST_CASE("StripViewMath - scrolling inside a chart is never resisted", "[stripview]")
{
    BoundaryDetent d;

    REQUIRE(resolveAt(d, 1100.0, 0) == Catch::Approx(1100.0));
    REQUIRE(resolveAt(d, 1200.0, 10) == Catch::Approx(1200.0));
}

// The first notch out is held at the edge rather than doing nothing, so the view still moves
// and lands somewhere meaningful: flush against the boundary.
TEST_CASE("StripViewMath - the first notch past the end is held at the edge", "[stripview]")
{
    BoundaryDetent d;
    double top = SONG_END - SPAN;

    REQUIRE(resolveAt(d, top + SOFT_PUSH, 0) == Catch::Approx(top));
}

TEST_CASE("StripViewMath - a second notch soon after crosses", "[stripview]")
{
    BoundaryDetent d;
    double top = SONG_END - SPAN;

    REQUIRE(resolveAt(d, top + SOFT_PUSH, 0) == Catch::Approx(top));

    double want = top + SOFT_PUSH;
    REQUIRE(resolveAt(d, want, 50) == Catch::Approx(want));
}

// The case Noah called out: stop at the edge, walk away, come back. A lone notch must NOT
// spring through, because the bank decayed while nothing was happening.
TEST_CASE("StripViewMath - a notch after a pause re-arms instead of jumping", "[stripview]")
{
    BoundaryDetent d;
    double top = SONG_END - SPAN;

    REQUIRE(resolveAt(d, top + SOFT_PUSH, 0) == Catch::Approx(top));

    // Long enough that the bank is gone.
    REQUIRE(resolveAt(d, top + SOFT_PUSH, RESET_MS + 1) == Catch::Approx(top));

    // And it still takes a prompt follow-up to get out, exactly as from a standstill.
    double want = top + SOFT_PUSH;
    REQUIRE(resolveAt(d, want, RESET_MS + 40) == Catch::Approx(want));
}

// A trackpad glide banks pressure fast, so the detent is barely felt rather than fought.
TEST_CASE("StripViewMath - one hard shove crosses immediately", "[stripview]")
{
    BoundaryDetent d;
    double top = SONG_END - SPAN;
    double want = top + HARD_PUSH;

    REQUIRE(resolveAt(d, want, 0) == Catch::Approx(want));
}

TEST_CASE("StripViewMath - the bottom edge resists the same way", "[stripview]")
{
    BoundaryDetent d;

    REQUIRE(resolveAt(d, SONG_START - SOFT_PUSH, 0) == Catch::Approx(SONG_START));

    double want = SONG_START - SOFT_PUSH;
    REQUIRE(resolveAt(d, want, 50) == Catch::Approx(want));
}

// Pressure banked leaning on one edge must not fire you through the opposite one.
TEST_CASE("StripViewMath - pressure does not carry across to the other edge", "[stripview]")
{
    BoundaryDetent d;
    double top = SONG_END - SPAN;

    REQUIRE(resolveAt(d, top + SOFT_PUSH, 0) == Catch::Approx(top));
    REQUIRE(resolveAt(d, SONG_START - SOFT_PUSH, 20) == Catch::Approx(SONG_START));
}

// Zoomed wider than the chart, the view straddles both edges no matter where it sits, so a
// detent would be friction against nothing.
TEST_CASE("StripViewMath - a view wider than the chart is not resisted", "[stripview]")
{
    BoundaryDetent d;
    double wide = (SONG_END - SONG_START) * 2.0;

    double got = d.resolve(SONG_START - 50.0, wide, SONG_START, SONG_END,
                           0, RESET_MS, THRESHOLD);

    REQUIRE(got == Catch::Approx(SONG_START - 50.0));
}
