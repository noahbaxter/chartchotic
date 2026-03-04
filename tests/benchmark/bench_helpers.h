/*
    ==============================================================================

        bench_helpers.h
        Synthetic data factories for rendering benchmark scenarios.

        Builds TimeBasedTrackWindow, TimeBasedSustainWindow, TimeBasedGridlineMap
        directly — no MIDI parsing needed. Fixed-seed RNG for reproducibility.

    ==============================================================================
*/

#pragma once

#include "Utils/TimeConverter.h"
#include <random>

namespace BenchHelpers
{

struct ScenarioData
{
    TimeBasedTrackWindow trackWindow;
    TimeBasedSustainWindow sustainWindow;
    TimeBasedGridlineMap gridlines;
};

// Distribute notes across columns with realistic weighting.
// Column 0 = bar/kick, columns 1-maxColumn = fret pads (guitar 5) or drum pads (4).
inline void addNotes(TimeBasedTrackWindow& tw, int count, double windowStart, double windowEnd, int maxColumn = 5)
{
    if (count <= 0) return;

    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<int> colDist(0, maxColumn);
    std::uniform_real_distribution<double> posDist(0.0, 1.0);

    double span = windowEnd - windowStart;

    for (int i = 0; i < count; i++)
    {
        double t = windowStart + span * (double(i) + posDist(rng)) / double(count);
        TimeBasedTrackFrame frame{};

        int col = colDist(rng);
        // Column 0 gets bar notes ~15% of the time
        if (col == 0 && posDist(rng) > 0.15)
            col = 1 + (int)(posDist(rng) * (maxColumn - 0.01));

        frame[(size_t)col] = GemWrapper(Gem::NOTE);

        // ~25% chance of chord (add a second note)
        if (posDist(rng) < 0.25 && col >= 1)
        {
            int col2 = 1 + (int)(posDist(rng) * (maxColumn - 0.01));
            if (col2 != col)
                frame[(size_t)col2] = GemWrapper(Gem::NOTE);
        }

        // ~8% chance of 3-note chord
        if (posDist(rng) < 0.08 && col >= 1)
        {
            int col3 = 1 + (int)(posDist(rng) * (maxColumn - 0.01));
            frame[(size_t)col3] = GemWrapper(Gem::HOPO_GHOST);
        }

        tw[t] = frame;
    }
}

inline void addSustains(TimeBasedSustainWindow& sw, int count, double windowStart, double windowEnd, int maxColumn = 5)
{
    if (count <= 0) return;

    std::mt19937 rng(123);
    std::uniform_int_distribution<int> colDist(0, maxColumn);
    std::uniform_real_distribution<double> posDist(0.0, 1.0);

    double span = windowEnd - windowStart;

    for (int i = 0; i < count; i++)
    {
        double startT = windowStart + span * (double(i) / double(count));
        double length = span * (0.05 + posDist(rng) * 0.20);
        double endT = std::min(startT + length, windowEnd);

        int col = colDist(rng);
        if (col == 0 && posDist(rng) > 0.15)
            col = 1 + (int)(posDist(rng) * (maxColumn - 0.01));

        TimeBasedSustainEvent ev;
        ev.startTime = startT;
        ev.endTime = endT;
        ev.gemColumn = (uint)col;
        ev.sustainType = (col == 0) ? SustainType::LANE : SustainType::SUSTAIN;
        ev.gemType = GemWrapper(Gem::NOTE);
        sw.push_back(ev);
    }
}

inline void addLanes(TimeBasedSustainWindow& sw, int count, double windowStart, double windowEnd)
{
    // Full-width lanes (star power / solo sections)
    for (int i = 0; i < count && i < 5; i++)
    {
        TimeBasedSustainEvent ev;
        ev.startTime = windowStart;
        ev.endTime = windowEnd;
        ev.gemColumn = 1 + (uint)i;
        ev.sustainType = SustainType::LANE;
        ev.gemType = GemWrapper(Gem::NOTE);
        sw.push_back(ev);
    }
}

inline void addGridlines(TimeBasedGridlineMap& gm, int count, double windowStart, double windowEnd)
{
    if (count <= 0) return;

    double span = windowEnd - windowStart;
    for (int i = 0; i < count; i++)
    {
        double t = windowStart + span * double(i) / double(count);
        Gridline type;
        if (i % 8 == 0)
            type = Gridline::MEASURE;
        else if (i % 2 == 0)
            type = Gridline::BEAT;
        else
            type = Gridline::HALF_BEAT;

        gm.push_back({t, type});
    }
}

struct ScenarioSpec
{
    const char* name;
    int notes;
    int sustains;
    int lanes;
    int gridlines;
};

// Notes/sustains/gridlines are independent — tuned to realistic chart densities.
// "stress" = absurd worst case, way beyond any real chart.
constexpr ScenarioSpec SCENARIOS[] = {
    {"empty",           0,   0, 0,  0},
    {"grid_only",       0,   0, 0, 20},
    {"light",          15,   4, 2, 10},
    {"medium",         40,  12, 3, 16},
    {"heavy",          80,  25, 5, 20},
    {"stress",        200,  50, 5, 30},
};

constexpr int SCENARIO_COUNT = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

inline ScenarioData buildScenario(const ScenarioSpec& spec, double windowStart, double windowEnd,
                                   int maxColumn = 5, int sustainOverride = -1)
{
    ScenarioData data;
    addNotes(data.trackWindow, spec.notes, windowStart, windowEnd, maxColumn);
    addSustains(data.sustainWindow, sustainOverride >= 0 ? sustainOverride : spec.sustains, windowStart, windowEnd, maxColumn);
    addLanes(data.sustainWindow, spec.lanes, windowStart, windowEnd);
    addGridlines(data.gridlines, spec.gridlines, windowStart, windowEnd);
    return data;
}

struct Resolution
{
    const char* name;
    int width;
    int height;
};

constexpr Resolution RESOLUTIONS[] = {
    {"720p",  960,   720},
    {"1080p", 1440, 1080},
    {"1440p", 1920, 1440},
};

constexpr int RESOLUTION_COUNT = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

} // namespace BenchHelpers
