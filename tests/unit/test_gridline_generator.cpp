#include "test_helpers.h"
#include <catch2/catch_approx.hpp>
#include "Visual/Managers/GridlineGenerator.h"
#include "Midi/Utils/TimeConverter.h"
#include "Editor/AuthoringUtils.h"

// Simple identity ppqToTime for testing (PPQ value = time in seconds)
static auto identityPPQToTime = [](double ppq) { return ppq; };

// ============================================================================
// Basic gridline generation

TEST_CASE("Gridline generator - 4/4 time basics", "[gridline]")
{
    TempoTimeSignatureMap tempoMap;
    tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4, true);

    SECTION("window PPQ 0-8: measure lines at 0 and 4")
    {
        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(8.0), PPQ(0.0), identityPPQToTime);

        int measureCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
        }
        REQUIRE(measureCount == 2); // at 0 and 4
    }

    SECTION("beat lines at 1, 2, 3, 5, 6, 7")
    {
        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(8.0), PPQ(0.0), identityPPQToTime);

        int beatCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::BEAT) beatCount++;
        }
        REQUIRE(beatCount == 6);
    }

    SECTION("half-beat lines between beats")
    {
        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(8.0), PPQ(0.0), identityPPQToTime);

        int halfBeatCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::HALF_BEAT) halfBeatCount++;
        }
        // In 4/4 over 8 PPQ: half-beats at 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5
        REQUIRE(halfBeatCount == 8);
    }
}

// ============================================================================
// Time signature changes

TEST_CASE("Gridline generator - time signature changes", "[gridline]")
{
    SECTION("3/4 time: measure lines every 3 beats")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 3, 4, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(9.0), PPQ(0.0), identityPPQToTime);

        int measureCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
        }
        REQUIRE(measureCount == 3); // at 0, 3, 6
    }

    SECTION("6/8 time: beat spacing = 0.5 PPQ")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 6, 8, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(3.0), PPQ(0.0), identityPPQToTime);

        // In 6/8: beat spacing = 4/8 = 0.5, measure length = 6 * 0.5 = 3.0
        // Beats at 0, 0.5, 1.0, 1.5, 2.0, 2.5 (measure at 0 only)
        int beatCount = 0;
        int measureCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::BEAT) beatCount++;
            if (gl.type == Gridline::MEASURE) measureCount++;
        }
        REQUIRE(measureCount == 1); // at 0
        REQUIRE(beatCount == 5);    // at 0.5, 1.0, 1.5, 2.0, 2.5
    }

    SECTION("timeSigReset resets measure anchor")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4, true);
        tempoMap[PPQ(3.0)] = TempoTimeSignatureEvent(PPQ(3.0), 120.0, 3, 4, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(9.0), PPQ(0.0), identityPPQToTime);

        // Should have measure at 0 (4/4 section), then at 3, 6 (3/4 section)
        int measureCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
        }
        REQUIRE(measureCount == 3);
    }
}

// ============================================================================
// Edge cases

TEST_CASE("Gridline generator - edge cases", "[gridline]")
{
    SECTION("empty tempo map → defaults to 120 BPM, 4/4")
    {
        TempoTimeSignatureMap emptyMap;

        auto gridlines = GridlineGenerator::generateGridlines(
            emptyMap, PPQ(0.0), PPQ(4.0), PPQ(0.0), identityPPQToTime);

        // Should still produce gridlines with default 4/4
        REQUIRE_FALSE(gridlines.empty());

        // Should have a measure line at 0
        bool hasMeasure = false;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) hasMeasure = true;
        }
        REQUIRE(hasMeasure);
    }

    SECTION("zero-length window → empty result")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(2.0), PPQ(2.0), PPQ(0.0), identityPPQToTime);

        REQUIRE(gridlines.empty());
    }

    SECTION("invalid time signature (0/0) → no crash, empty result")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 0, 0, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(4.0), PPQ(0.0), identityPPQToTime);

        REQUIRE(gridlines.empty());
    }
}

// ============================================================================
// Irregular time signatures

TEST_CASE("Gridline generator - irregular time signatures", "[gridline]")
{
    SECTION("5/4 time: measure every 5 beats")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 5, 4, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(10.0), PPQ(0.0), identityPPQToTime);

        int measureCount = 0;
        int beatCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
            if (gl.type == Gridline::BEAT) beatCount++;
        }
        REQUIRE(measureCount == 2); // at 0 and 5
        REQUIRE(beatCount == 8);    // 1,2,3,4 + 6,7,8,9
    }

    SECTION("7/8 time: measure every 3.5 beats")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 7, 8, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(7.0), PPQ(0.0), identityPPQToTime);

        // 7/8: beat spacing = 4/8 = 0.5, measure length = 7 * 0.5 = 3.5
        int measureCount = 0;
        int beatCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
            if (gl.type == Gridline::BEAT) beatCount++;
        }
        REQUIRE(measureCount == 2); // at 0 and 3.5
    }
}

// ============================================================================
// Multiple time signature changes in one window

TEST_CASE("Gridline generator - multiple time sig changes", "[gridline]")
{
    SECTION("4/4 then 3/4 in same window")
    {
        TempoTimeSignatureMap tempoMap;
        tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4, true);
        tempoMap[PPQ(4.0)] = TempoTimeSignatureEvent(PPQ(4.0), 120.0, 3, 4, true);

        auto gridlines = GridlineGenerator::generateGridlines(
            tempoMap, PPQ(0.0), PPQ(10.0), PPQ(0.0), identityPPQToTime);

        int measureCount = 0;
        for (auto& gl : gridlines)
        {
            if (gl.type == Gridline::MEASURE) measureCount++;
        }
        // 4/4: measure at 0. Then 3/4: measures at 4, 7
        REQUIRE(measureCount == 3);
    }
}

// ============================================================================
// Snap follows the gridlines' measure anchor

static GridSection sectionAt(const TempoTimeSignatureMap& map, double qn)
{
    return gridSectionFor(map, qn);
}

TEST_CASE("Snap - a bar short by a 16th moves the grid after it", "[gridline][snap]")
{
    // 8/8, then one 13/16 bar at 8, then back to 8/8 at 11.25: everything after sits a 16th
    // off the QN-0 grid.
    TempoTimeSignatureMap map;
    map[PPQ(0.0)]   = TempoTimeSignatureEvent(PPQ(0.0),   120.0, 8,  8,  true);
    map[PPQ(8.0)]   = TempoTimeSignatureEvent(PPQ(8.0),   120.0, 13, 16, true);
    map[PPQ(11.25)] = TempoTimeSignatureEvent(PPQ(11.25), 120.0, 8,  8,  true);

    auto snap = [&](double qn, int div) { return snapToStep(qn, div, 0, sectionAt(map, qn)); };

    SECTION("1/4 and 1/8 snap to the shifted grid")
    {
        REQUIRE(snap(13.4, 4) == Catch::Approx(13.25));
        REQUIRE(snap(13.6, 8) == Catch::Approx(13.75));
    }

    SECTION("before the odd bar nothing moves")
    {
        REQUIRE(snap(5.4, 4) == Catch::Approx(5.0));
    }

    SECTION("inside the odd bar the next bar line is a snap point")
    {
        REQUIRE(snap(11.05, 8) == Catch::Approx(11.0));
        REQUIRE(snap(11.2, 8)  == Catch::Approx(11.25));
    }

    SECTION("every drawn measure and beat line is its own snap point")
    {
        auto gridlines = GridlineGenerator::generateGridlines(
            map, PPQ(0.0), PPQ(20.0), PPQ(0.0), identityPPQToTime);
        int checked = 0;
        for (const auto& gl : gridlines)
        {
            if (gl.type != Gridline::MEASURE && gl.type != Gridline::BEAT) continue;
            for (int div : { 4, 8, 16 })
            {
                // Beats are 8ths in 8/8 and 16ths in the 13/16 bar, so a coarser grid only
                // has to hit the measure lines.
                const bool oddBar = gl.time > 8.0 && gl.time < 11.25;
                if (gl.type == Gridline::BEAT && div < (oddBar ? 16 : 8)) continue;
                REQUIRE(snap(gl.time, div) == Catch::Approx(gl.time));
                ++checked;
            }
        }
        REQUIRE(checked > 0);
    }
}

TEST_CASE("Snap - every bar line is reachable at any grid size", "[gridline][snap]")
{
    // 3/4, then 7/8 at 9, then 5/4 at 16. A 1/1 step is four beats, which lines up with none
    // of these bars, so without the bar lines the 1 of most bars could not be reached.
    TempoTimeSignatureMap map;
    map[PPQ(0.0)]  = TempoTimeSignatureEvent(PPQ(0.0),  120.0, 3, 4, true);
    map[PPQ(9.0)]  = TempoTimeSignatureEvent(PPQ(9.0),  120.0, 7, 8, true);
    map[PPQ(16.0)] = TempoTimeSignatureEvent(PPQ(16.0), 120.0, 5, 4, true);

    auto snap = [&](double qn, int div) { return snapToStep(qn, div, 0, sectionAt(map, qn)); };

    auto gridlines = GridlineGenerator::generateGridlines(map, PPQ(0.0), PPQ(30.0), PPQ(0.0), identityPPQToTime);
    int bars = 0;
    for (const auto& gl : gridlines)
    {
        if (gl.type != Gridline::MEASURE) continue;
        ++bars;
        for (int div : { 1, 2 })
        {
            INFO("bar line " << gl.time << " at 1/" << div);
            REQUIRE(snap(gl.time, div) == Catch::Approx(gl.time));
            // And from either side, as a cursor actually arrives at it.
            REQUIRE(snap(gl.time - 0.2, div) == Catch::Approx(gl.time));
            REQUIRE(snap(gl.time + 0.2, div) == Catch::Approx(gl.time));
        }
    }
    REQUIRE(bars >= 8);

    // The regular grid is still there between bar lines: 1/1 in 3/4 still lands on beat 4.
    REQUIRE(snap(4.1, 1) == Catch::Approx(4.0));
}
