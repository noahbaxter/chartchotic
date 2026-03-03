/*
    ==============================================================================

        bench_helpers.h
        Synthetic data factories for rendering benchmark scenarios.

        Builds TimeBasedTrackWindow, TimeBasedSustainWindow, TimeBasedGridlineMap
        directly — no MIDI parsing needed.

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
// Column 0 = bar/kick, columns 1-5 = fret pads (guitar) or 1-4 (drums).
inline void addNotes(TimeBasedTrackWindow& tw, int count, double windowStart, double windowEnd)
{
    if (count <= 0) return;

    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<int> colDist(0, 5);
    std::uniform_real_distribution<double> posDist(0.0, 1.0);

    double span = windowEnd - windowStart;

    for (int i = 0; i < count; i++)
    {
        double t = windowStart + span * (double(i) + posDist(rng)) / double(count);
        TimeBasedTrackFrame frame{};

        int col = colDist(rng);
        // Column 0 gets bar notes ~15% of the time
        if (col == 0 && posDist(rng) > 0.15)
            col = 1 + (int)(posDist(rng) * 4.99);

        frame[(size_t)col] = GemWrapper(Gem::NOTE);

        // ~20% chance of chord (add a second note)
        if (posDist(rng) < 0.2 && col >= 1)
        {
            int col2 = 1 + (int)(posDist(rng) * 4.99);
            if (col2 != col)
                frame[(size_t)col2] = GemWrapper(Gem::NOTE);
        }

        tw[t] = frame;
    }
}

inline void addSustains(TimeBasedSustainWindow& sw, int count, double windowStart, double windowEnd)
{
    if (count <= 0) return;

    std::mt19937 rng(123);
    std::uniform_int_distribution<int> colDist(0, 5);
    std::uniform_real_distribution<double> posDist(0.0, 1.0);

    double span = windowEnd - windowStart;

    for (int i = 0; i < count; i++)
    {
        double startT = windowStart + span * (double(i) / double(count));
        double length = span * (0.05 + posDist(rng) * 0.15);
        double endT = std::min(startT + length, windowEnd);

        int col = colDist(rng);
        if (col == 0 && posDist(rng) > 0.15)
            col = 1 + (int)(posDist(rng) * 4.99);

        TimeBasedSustainEvent ev;
        ev.startTime = startT;
        ev.endTime = endT;
        ev.gemColumn = (uint)col;
        ev.sustainType = (col == 0) ? SustainType::LANE : SustainType::SUSTAIN;
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
        if (i % 4 == 0)
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
    int gridlines;
};

constexpr ScenarioSpec SCENARIOS[] = {
    {"empty",          0,   0,  0},
    {"gridlines_only", 0,   0, 20},
    {"light",         10,   4,  8},
    {"medium",        30,  10, 15},
    {"heavy",         60,  20, 20},
    {"stress",       150,  40, 30},
};

constexpr int SCENARIO_COUNT = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

inline ScenarioData buildScenario(const ScenarioSpec& spec, double windowStart, double windowEnd)
{
    ScenarioData data;
    addNotes(data.trackWindow, spec.notes, windowStart, windowEnd);
    addSustains(data.sustainWindow, spec.sustains, windowStart, windowEnd);
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
    {"small",  640,  480},
    {"720p",  1280,  720},
    {"1080p", 1920, 1080},
    {"4k",    3840, 2160},
};

constexpr int RESOLUTION_COUNT = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

} // namespace BenchHelpers
