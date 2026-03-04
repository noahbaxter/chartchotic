/*
  ==============================================================================

    bench_main.cpp
    Created: 4 Mar 2026
    Author:  Noah Baxter

    Headless rendering benchmark for Chart Preview.
    Renders into offscreen images and collects per-phase timing.
    Runs both guitar and drums scenarios.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Visual/Renderers/SceneRenderer.h"
#include "Midi/Processing/MidiInterpreter.h"
#include "bench_helpers.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <map>

using namespace BenchHelpers;

//==============================================================================
// Stats

struct TimingStats
{
    double mean = 0;
    double p95 = 0;
    double max = 0;
};

static TimingStats computeStats(std::vector<double> samples)
{
    if (samples.empty()) return {};
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (auto s : samples) sum += s;

    TimingStats st;
    st.mean = sum / samples.size();
    st.p95 = samples[(size_t)(samples.size() * 0.95)];
    st.max = samples.back();
    return st;
}

//==============================================================================

static const char* drawOrderName(DrawOrder d)
{
    switch (d) {
        case DrawOrder::BACKGROUND:     return "BG";
        case DrawOrder::TRACK:          return "TRACK";
        case DrawOrder::GRID:           return "GRID";
        case DrawOrder::LANE:           return "LANE";
        case DrawOrder::BAR:            return "BAR";
        case DrawOrder::BAR_ANIMATION:  return "BAR_ANIM";
        case DrawOrder::SUSTAIN:        return "SUSTAIN";
        case DrawOrder::NOTE:           return "NOTE";
        case DrawOrder::OVERLAY:        return "OVERLAY";
        case DrawOrder::NOTE_ANIMATION: return "NOTE_ANIM";
        default:                        return "?";
    }
}

struct BenchResult
{
    std::string scenario;
    std::string instrument;
    std::string resolution;
    int width, height, iterations;
    TimingStats notes, sustains, gridlines, execute, total;
    std::map<DrawOrder, TimingStats> layers;
};

//==============================================================================
// Table output

static constexpr double FRAME_BUDGET_US = 16666.7; // 60fps

void printTable(const std::vector<BenchResult>& results)
{
    // Group by instrument
    std::map<std::string, std::vector<const BenchResult*>> byInstrument;
    for (const auto& r : results)
        byInstrument[r.instrument].push_back(&r);

    for (const auto& [instrument, entries] : byInstrument)
    {
        std::cout << std::endl << "=== " << instrument << " ===" << std::endl << std::endl;
        std::cout << std::left << std::setw(13) << "Scenario"
                  << std::setw(8) << "Res"
                  << std::right
                  << std::setw(8) << "Notes"
                  << std::setw(9) << "Sustains"
                  << std::setw(7) << "Grid"
                  << std::setw(9) << "Execute"
                  << std::setw(9) << "Total"
                  << std::setw(9) << "p95"
                  << std::setw(7) << "@60fps"
                  << std::endl;
        std::cout << std::string(79, '-') << std::endl;

        std::string lastScenario;
        for (const auto* r : entries)
        {
            if (!lastScenario.empty() && r->scenario != lastScenario)
                std::cout << std::endl;
            lastScenario = r->scenario;

            double pct = (r->total.mean / FRAME_BUDGET_US) * 100.0;
            std::cout << std::left << std::setw(13) << r->scenario
                      << std::setw(8) << r->resolution
                      << std::right << std::fixed << std::setprecision(0)
                      << std::setw(8) << r->notes.mean
                      << std::setw(9) << r->sustains.mean
                      << std::setw(7) << r->gridlines.mean
                      << std::setw(9) << r->execute.mean
                      << std::setw(9) << r->total.mean
                      << std::setw(9) << r->total.p95
                      << std::setw(6) << pct << "%"
                      << std::endl;
        }
    }

    // Layer breakdown table
    for (const auto& [instrument, entries] : byInstrument)
    {
        std::cout << std::endl << "=== " << instrument << " layers ===" << std::endl << std::endl;

        // Collect all layer types seen
        std::vector<DrawOrder> layerOrder = {
            DrawOrder::GRID, DrawOrder::LANE, DrawOrder::BAR,
            DrawOrder::SUSTAIN, DrawOrder::NOTE, DrawOrder::OVERLAY
        };

        std::cout << std::left << std::setw(13) << "Scenario"
                  << std::setw(8) << "Res";
        for (auto layer : layerOrder)
            std::cout << std::right << std::setw(9) << drawOrderName(layer);
        std::cout << std::endl;
        std::cout << std::string(13 + 8 + 9 * (int)layerOrder.size(), '-') << std::endl;

        std::string lastScenario;
        for (const auto* r : entries)
        {
            if (!lastScenario.empty() && r->scenario != lastScenario)
                std::cout << std::endl;
            lastScenario = r->scenario;

            std::cout << std::left << std::setw(13) << r->scenario
                      << std::setw(8) << r->resolution
                      << std::right << std::fixed << std::setprecision(0);
            for (auto layer : layerOrder)
            {
                auto it = r->layers.find(layer);
                if (it != r->layers.end())
                    std::cout << std::setw(9) << it->second.mean;
                else
                    std::cout << std::setw(9) << "-";
            }
            std::cout << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "(all values in microseconds, averaged over " << results[0].iterations << " iterations)" << std::endl;
}

//==============================================================================
// Baseline comparison

bool compareBaseline(const std::vector<BenchResult>& results, const std::string& baselinePath)
{
    std::ifstream f(baselinePath);
    if (!f.is_open())
    {
        std::cout << "No baseline found at " << baselinePath << std::endl;
        return true;
    }
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Simple JSON parser — find each {...} block
    struct BL { std::string scenario, instrument, resolution; double total; };
    std::vector<BL> baseline;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos)
    {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        std::string block = json.substr(pos, end - pos + 1);
        pos = end + 1;

        auto getStr = [&](const std::string& key) -> std::string {
            size_t k = block.find("\"" + key + "\"");
            if (k == std::string::npos) return "";
            size_t colon = block.find(':', k);
            size_t q1 = block.find('"', colon + 1);
            size_t q2 = block.find('"', q1 + 1);
            return block.substr(q1 + 1, q2 - q1 - 1);
        };
        auto getNum = [&](const std::string& key) -> double {
            size_t k = block.find("\"" + key + "\"");
            if (k == std::string::npos) return 0.0;
            size_t colon = block.find(':', k);
            size_t start = colon + 1;
            while (start < block.size() && block[start] == ' ') ++start;
            return std::stod(block.substr(start));
        };

        BL b;
        b.scenario = getStr("scenario");
        b.instrument = getStr("instrument");
        b.resolution = getStr("resolution");
        b.total = getNum("total_mean_us");
        if (!b.scenario.empty()) baseline.push_back(b);
    }

    const double threshold = 0.20;
    bool ok = true;
    std::vector<std::string> regressions;

    for (const auto& r : results)
    {
        for (const auto& b : baseline)
        {
            if (b.scenario == r.scenario && b.instrument == r.instrument && b.resolution == r.resolution)
            {
                if (b.total > 0)
                {
                    double pct = (r.total.mean - b.total) / b.total;
                    if (pct > threshold)
                    {
                        ok = false;
                        regressions.push_back("  " + r.instrument + " " + r.scenario + " " + r.resolution + ": "
                            + std::to_string((int)b.total) + " -> " + std::to_string((int)r.total.mean)
                            + " us (+" + std::to_string((int)(pct * 100)) + "%)");
                    }
                }
                break;
            }
        }
    }

    if (!regressions.empty())
    {
        std::cout << "REGRESSIONS DETECTED (>20%):" << std::endl;
        for (const auto& line : regressions)
            std::cout << line << std::endl;
    }
    else
    {
        std::cout << "No regressions vs baseline." << std::endl;
    }

    return ok;
}

//==============================================================================
// JSON output

std::string resultsToJson(const std::vector<BenchResult>& results)
{
    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << v;
        return ss.str();
    };

    std::string json = "[\n";
    for (size_t i = 0; i < results.size(); ++i)
    {
        auto& r = results[i];
        json += "  {";
        json += "\"scenario\":\"" + r.scenario + "\",";
        json += "\"instrument\":\"" + r.instrument + "\",";
        json += "\"resolution\":\"" + r.resolution + "\",";
        json += "\"width\":" + std::to_string(r.width) + ",";
        json += "\"height\":" + std::to_string(r.height) + ",";
        json += "\"iterations\":" + std::to_string(r.iterations) + ",";
        json += "\"notes_mean_us\":" + fmt(r.notes.mean) + ",";
        json += "\"sustains_mean_us\":" + fmt(r.sustains.mean) + ",";
        json += "\"gridlines_mean_us\":" + fmt(r.gridlines.mean) + ",";
        json += "\"execute_mean_us\":" + fmt(r.execute.mean) + ",";
        json += "\"total_mean_us\":" + fmt(r.total.mean) + ",";
        json += "\"total_p95_us\":" + fmt(r.total.p95) + ",";
        json += "\"total_max_us\":" + fmt(r.total.max) + ",";
        json += "\"layers\":{";
        bool firstLayer = true;
        for (const auto& [layer, stats] : r.layers)
        {
            if (!firstLayer) json += ",";
            json += std::string("\"") + drawOrderName(layer) + "\":{";
            json += "\"mean\":" + fmt(stats.mean) + ",";
            json += "\"p95\":" + fmt(stats.p95) + ",";
            json += "\"max\":" + fmt(stats.max) + "}";
            firstLayer = false;
        }
        json += "}}";

        if (i + 1 < results.size()) json += ",";
        json += "\n";
    }
    json += "]\n";
    return json;
}

//==============================================================================

void runInstrument(const char* instrumentName, Part part, int maxColumn, bool noSustains,
                   juce::ValueTree& state, MidiInterpreter& midiInterpreter, SceneRenderer& renderer,
                   int iterationCount, int warmupCount, double windowStart, double windowEnd,
                   std::vector<BenchResult>& results, int& combo, int totalCombos)
{
    state.setProperty("part", (int)part, nullptr);

    for (int si = 0; si < SCENARIO_COUNT; si++)
    {
        const auto& spec = SCENARIOS[si];
        auto scenarioData = buildScenario(spec, windowStart, windowEnd,
                                           maxColumn, noSustains ? 0 : -1);

        for (int ri = 0; ri < RESOLUTION_COUNT; ri++)
        {
            const auto& res = RESOLUTIONS[ri];
            ++combo;
            std::cerr << "[" << combo << "/" << totalCombos << "] "
                      << instrumentName << " " << spec.name << " @ " << res.name
                      << " (" << iterationCount << " iters)..." << std::flush;

            juce::Image canvas(juce::Image::ARGB, res.width, res.height, true);

            const auto& tw = scenarioData.trackWindow;
            const auto& sw = scenarioData.sustainWindow;
            const auto& gm = scenarioData.gridlines;

            std::vector<double> notesSamples, sustainsSamples, gridlinesSamples, executeSamples, totalSamples;
            std::map<DrawOrder, std::vector<double>> layerSamples;

            // Warmup
            for (int w = 0; w < warmupCount; w++)
            {
                canvas.clear({0, 0, res.width, res.height});
                juce::Graphics g(canvas);
                renderer.paint(g, tw, sw, gm, windowStart, windowEnd, false);
            }

            for (int i = 0; i < iterationCount; i++)
            {
                canvas.clear({0, 0, res.width, res.height});
                juce::Graphics g(canvas);
                renderer.paint(g, tw, sw, gm, windowStart, windowEnd, false);

                const auto& pt = renderer.lastPhaseTiming;
                notesSamples.push_back(pt.notes_us);
                sustainsSamples.push_back(pt.sustains_us);
                gridlinesSamples.push_back(pt.gridlines_us);
                executeSamples.push_back(pt.execute_us);
                totalSamples.push_back(pt.total_us);

                for (const auto& [layer, us] : pt.layer_us)
                    layerSamples[layer].push_back(us);
            }

            BenchResult r;
            r.scenario = spec.name;
            r.instrument = instrumentName;
            r.resolution = res.name;
            r.width = res.width;
            r.height = res.height;
            r.iterations = iterationCount;
            r.notes = computeStats(notesSamples);
            r.sustains = computeStats(sustainsSamples);
            r.gridlines = computeStats(gridlinesSamples);
            r.execute = computeStats(executeSamples);
            r.total = computeStats(totalSamples);
            for (const auto& [layer, samples] : layerSamples)
                r.layers[layer] = computeStats(samples);
            results.push_back(r);

            std::cerr << " " << (int)r.total.mean << " us/frame" << std::endl;
        }
    }
}

//==============================================================================

int main(int argc, char* argv[])
{
    std::string outputPath;
    std::string baselinePath;
    bool saveBaseline = false;
    bool jsonOutput = false;
    int iterationCount = 50;
    int warmupCount = 5;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg == "--iterations" && i + 1 < argc)
            iterationCount = std::atoi(argv[++i]);
        else if (arg == "--warmup" && i + 1 < argc)
            warmupCount = std::atoi(argv[++i]);
        else if (arg == "--baseline" && i + 1 < argc)
            baselinePath = argv[++i];
        else if (arg == "--save-baseline")
            saveBaseline = true;
        else if (arg == "--json")
            jsonOutput = true;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::ValueTree state("PluginState");
    state.setProperty("part", (int)Part::GUITAR, nullptr);
    state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
    state.setProperty("autoHopo", (int)HopoMode::OFF, nullptr);
    state.setProperty("starPower", 0, nullptr);
    state.setProperty("hitIndicators", 0, nullptr);
    state.setProperty("dynamics", 1, nullptr);
    state.setProperty("kick2x", 1, nullptr);

    NoteStateMapArray noteStateMapArray{};
    juce::CriticalSection noteStateMapLock;
    MidiInterpreter midiInterpreter(state, noteStateMapArray, noteStateMapLock);
    SceneRenderer renderer(state, midiInterpreter);
    renderer.collectPhaseTiming = true;

    double windowStart = 0.0;
    double windowEnd = 1.0;

    std::vector<BenchResult> results;
    int totalCombos = SCENARIO_COUNT * RESOLUTION_COUNT * 2; // guitar + drums
    int combo = 0;

    // Guitar: 6 columns (0=open, 1-5=frets), has sustains
    runInstrument("guitar", Part::GUITAR, 5, false,
                  state, midiInterpreter, renderer,
                  iterationCount, warmupCount, windowStart, windowEnd,
                  results, combo, totalCombos);

    // Drums: 5 columns (0=kick, 1-4=pads), no sustains
    runInstrument("drums", Part::DRUMS, 4, true,
                  state, midiInterpreter, renderer,
                  iterationCount, warmupCount, windowStart, windowEnd,
                  results, combo, totalCombos);

    // Output
    if (jsonOutput)
    {
        std::cout << resultsToJson(results);
    }
    else
    {
        printTable(results);

        if (!baselinePath.empty())
        {
            std::cout << std::endl;
            bool ok = compareBaseline(results, baselinePath);
            if (!ok) return 1;
        }
    }

    if (!outputPath.empty())
    {
        std::ofstream out(outputPath);
        out << resultsToJson(results);
        std::cerr << "Results written to " << outputPath << std::endl;
    }

    if (saveBaseline && !baselinePath.empty())
    {
        std::ofstream out(baselinePath);
        out << resultsToJson(results);
        std::cout << std::endl << "Baseline saved to " << baselinePath << std::endl;
    }

    return 0;
}
