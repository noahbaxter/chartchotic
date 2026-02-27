#include "test_helpers.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/TimeConverter.h"

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
