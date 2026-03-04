/*
    ==============================================================================

        bench_rendering.cpp
        Headless rendering pipeline profiler.

        Runs offline (no DAW, no GUI) — creates an offscreen image, invokes
        the rendering pipeline with per-phase instrumentation, and reports
        timing breakdowns as JSON.

        Output: JSON to stdout for consumption by test_benchmark.py.

    ==============================================================================
*/

#include <JuceHeader.h>
#include "Visual/Renderers/HighwayRenderer.h"
#include "Midi/Processing/MidiInterpreter.h"
#include "bench_helpers.h"

#include <chrono>
#include <algorithm>
#include <iostream>

using namespace BenchHelpers;

//==============================================================================
// Stats

struct TimingResult
{
    double mean_us = 0;
    double min_us = 0;
    double max_us = 0;
    double p95_us = 0;
};

static TimingResult computeStats(const std::vector<double>& samples)
{
    if (samples.empty()) return {};

    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());

    double sum = 0;
    for (auto s : sorted) sum += s;

    TimingResult result;
    result.mean_us = sum / sorted.size();
    result.min_us = sorted.front();
    result.max_us = sorted.back();
    result.p95_us = sorted[(size_t)(sorted.size() * 0.95)];
    return result;
}

//==============================================================================
// JSON helpers

static juce::String escapeJson(const char* s) { return juce::String(s); }

static juce::String timingToJson(const TimingResult& t)
{
    return juce::String::formatted(
        "\"mean_us\": %.1f, \"min_us\": %.1f, \"max_us\": %.1f, \"p95_us\": %.1f",
        t.mean_us, t.min_us, t.max_us, t.p95_us);
}

static juce::String timingObjToJson(const char* name, const TimingResult& t)
{
    juce::String s;
    s << "\"" << name << "\": {" << timingToJson(t) << "}";
    return s;
}

//==============================================================================
// DrawOrder name mapping

static const char* drawOrderName(DrawOrder d)
{
    switch (d) {
        case DrawOrder::BACKGROUND:     return "background";
        case DrawOrder::TRACK:          return "track";
        case DrawOrder::HIGHWAY_OVERLAY:return "highway_overlay";
        case DrawOrder::GRID:           return "grid";
        case DrawOrder::LANE:           return "lane";
        case DrawOrder::BAR:            return "bar";
        case DrawOrder::BAR_ANIMATION:  return "bar_animation";
        case DrawOrder::SUSTAIN:        return "sustain";
        case DrawOrder::NOTE:           return "note";
        case DrawOrder::OVERLAY:        return "overlay";
        case DrawOrder::NOTE_ANIMATION: return "note_animation";
        default:                        return "unknown";
    }
}

//==============================================================================
// Machine info

static juce::String getMachineId()
{
#if JUCE_MAC
    #if JUCE_ARM
        return "macOS-arm64";
    #else
        return "macOS-x86_64";
    #endif
#elif JUCE_WINDOWS
    return "windows-x86_64";
#elif JUCE_LINUX
    return "linux-x86_64";
#else
    return "unknown";
#endif
}

//==============================================================================

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int iterations = 30;

    for (int i = 1; i < argc; i++)
    {
        juce::String arg(argv[i]);
        if (arg == "--iterations" && i + 1 < argc)
            iterations = juce::String(argv[i + 1]).getIntValue();
    }

    // Headless renderer setup
    juce::ValueTree state("ChartPreviewState");
    state.setProperty("part", (int)Part::GUITAR, nullptr);
    state.setProperty("hitIndicators", false, nullptr);
    state.setProperty("starPower", false, nullptr);

    NoteStateMapArray noteStateMapArray{};
    juce::CriticalSection noteStateMapLock;
    MidiInterpreter midiInterpreter(state, noteStateMapArray, noteStateMapLock);
    HighwayRenderer renderer(state, midiInterpreter);
    renderer.collectPhaseTiming = true;

    // Synthetic highway texture
    juce::Image fakeTexture(juce::Image::ARGB, 512, 512, true);
    {
        juce::Graphics tg(fakeTexture);
        tg.setGradientFill(juce::ColourGradient(
            juce::Colours::darkgrey, 0, 0,
            juce::Colours::black, 512, 512, false));
        tg.fillAll();
    }
    renderer.setHighwayTexture(fakeTexture);
    renderer.setScrollOffset(0.5);

    double windowStart = -2.0;
    double windowEnd = 2.0;

    juce::String machineId = getMachineId();
    juce::String timestamp = juce::Time::getCurrentTime().toISO8601(true);

    juce::StringArray resultEntries;

    // Run scenarios for a given instrument config
    auto runInstrument = [&](const char* instrumentName, int maxColumn, bool noSustains)
    {
        for (int si = 0; si < SCENARIO_COUNT; si++)
        {
            const auto& spec = SCENARIOS[si];
            auto scenarioData = buildScenario(spec, windowStart, windowEnd,
                                              maxColumn, noSustains ? 0 : -1);

            for (int ri = 0; ri < RESOLUTION_COUNT; ri++)
            {
                const auto& res = RESOLUTIONS[ri];
                juce::Image canvas(juce::Image::ARGB, res.width, res.height, true);

                const auto& tw = scenarioData.trackWindow;
                const auto& sw = scenarioData.sustainWindow;
                const auto& gm = scenarioData.gridlines;

                // Per-phase sample accumulators
                std::vector<double> totalSamples, textureSamples, buildNotesSamples,
                                    buildSustainsSamples, buildGridlinesSamples,
                                    animDetectSamples, execDrawsSamples,
                                    afterLanesSamples, animAdvanceSamples;
                std::map<DrawOrder, std::vector<double>> layerSamples;

                // Warmup
                for (int w = 0; w < 3; w++)
                {
                    canvas.clear({0, 0, res.width, res.height});
                    juce::Graphics g(canvas);
                    renderer.paint(g, tw, sw, gm, windowStart, windowEnd, true);
                }

                for (int i = 0; i < iterations; i++)
                {
                    canvas.clear({0, 0, res.width, res.height});
                    juce::Graphics g(canvas);
                    renderer.paint(g, tw, sw, gm, windowStart, windowEnd, true);

                    const auto& pt = renderer.lastPhaseTiming;
                    totalSamples.push_back(pt.total_us);
                    textureSamples.push_back(pt.texture_us);
                    buildNotesSamples.push_back(pt.build_notes_us);
                    buildSustainsSamples.push_back(pt.build_sustains_us);
                    buildGridlinesSamples.push_back(pt.build_gridlines_us);
                    animDetectSamples.push_back(pt.animation_detect_us);
                    execDrawsSamples.push_back(pt.execute_draws_us);
                    afterLanesSamples.push_back(pt.after_lanes_us);
                    animAdvanceSamples.push_back(pt.animation_advance_us);

                    for (const auto& [layer, us] : pt.layer_us)
                        layerSamples[layer].push_back(us);
                }

                // Build JSON entry
                juce::String entry;
                entry << "    {\n"
                      << "      \"scenario\": \"" << escapeJson(spec.name) << "\",\n"
                      << "      \"resolution\": \"" << escapeJson(res.name) << "\",\n"
                      << "      \"width\": " << res.width << ", \"height\": " << res.height << ",\n"
                      << "      \"instrument\": \"" << instrumentName << "\",\n"
                      << "      " << timingObjToJson("total", computeStats(totalSamples)) << ",\n"
                      << "      \"phases\": {\n"
                      << "        " << timingObjToJson("texture", computeStats(textureSamples)) << ",\n"
                      << "        " << timingObjToJson("build_notes", computeStats(buildNotesSamples)) << ",\n"
                      << "        " << timingObjToJson("build_sustains", computeStats(buildSustainsSamples)) << ",\n"
                      << "        " << timingObjToJson("build_gridlines", computeStats(buildGridlinesSamples)) << ",\n"
                      << "        " << timingObjToJson("animation_detect", computeStats(animDetectSamples)) << ",\n"
                      << "        " << timingObjToJson("execute_draws", computeStats(execDrawsSamples)) << ",\n"
                      << "        " << timingObjToJson("after_lanes", computeStats(afterLanesSamples)) << ",\n"
                      << "        " << timingObjToJson("animation_advance", computeStats(animAdvanceSamples)) << "\n"
                      << "      },\n"
                      << "      \"layers\": {";

                bool firstLayer = true;
                for (const auto& [layer, samples] : layerSamples)
                {
                    if (!firstLayer) entry << ",";
                    entry << "\n        " << timingObjToJson(drawOrderName(layer), computeStats(samples));
                    firstLayer = false;
                }

                entry << "\n      }\n"
                      << "    }";
                resultEntries.add(entry);
            }
        }
    };

    // Guitar scenarios
    state.setProperty("part", (int)Part::GUITAR, nullptr);
    runInstrument("guitar", 5, false);

    // Drums scenarios (4 columns, no sustains)
    state.setProperty("part", (int)Part::DRUMS, nullptr);
    runInstrument("drums", 4, true);

    // Print JSON
    std::cout << "{\n";
    std::cout << "  \"machine\": \"" << machineId.toStdString() << "\",\n";
    std::cout << "  \"timestamp\": \"" << timestamp.toStdString() << "\",\n";
    std::cout << "  \"iterations\": " << iterations << ",\n";
    std::cout << "  \"results\": [\n";
    for (int i = 0; i < resultEntries.size(); i++)
    {
        std::cout << resultEntries[i].toStdString();
        if (i < resultEntries.size() - 1) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";

    return 0;
}
