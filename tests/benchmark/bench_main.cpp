/*
  ==============================================================================

    bench_main.cpp
    Created: 4 Mar 2026
    Author:  Noah Baxter

    Headless rendering benchmark for Chartchotic.
    Renders into offscreen images and collects per-phase timing.
    Runs both guitar and drums scenarios.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Visual/Renderers/SceneRenderer.h"
#include "Visual/Art/TrackRenderer.h"
#include "Visual/Managers/AssetManager.h"
#include "Visual/Geometry/PositionMath.h"
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
        case DrawOrder::TRACK_SIDEBARS:  return "SIDEBAR";
        case DrawOrder::TRACK_LANE_LINES: return "LANELINE";
        case DrawOrder::WAVEFORM:       return "WAVE";
        case DrawOrder::TRACK_STRIKELINE: return "STRIKE";
        case DrawOrder::TRACK_CONNECTORS: return "CONNECT";
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
    TimingStats notes, sustains, gridlines, texture, track, execute, total;
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
                  << std::setw(9) << "Texture"
                  << std::setw(8) << "Track"
                  << std::setw(9) << "Execute"
                  << std::setw(9) << "Total"
                  << std::setw(9) << "p95"
                  << std::setw(7) << "@60fps"
                  << std::endl;
        std::cout << std::string(96, '-') << std::endl;

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
                      << std::setw(9) << r->texture.mean
                      << std::setw(8) << r->track.mean
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
            DrawOrder::TRACK_SIDEBARS, DrawOrder::TRACK_LANE_LINES, DrawOrder::TRACK_STRIKELINE,
            DrawOrder::GRID, DrawOrder::LANE, DrawOrder::BAR,
            DrawOrder::SUSTAIN, DrawOrder::NOTE, DrawOrder::TRACK_CONNECTORS, DrawOrder::OVERLAY
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
        json += "\"texture_mean_us\":" + fmt(r.texture.mean) + ",";
        json += "\"track_mean_us\":" + fmt(r.track.mean) + ",";
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
                   juce::ValueTree& state,
                   SceneRenderer& renderer, TrackRenderer& trackRenderer,
                   bool withTexture, bool noRescale, bool lowQuality, float trackOversize,
                   int iterationCount, int warmupCount, double windowStart, double windowEnd,
                   std::vector<BenchResult>& results, int& combo, int totalCombos)
{
    state.setProperty("part", (int)part, nullptr);
    renderer.activePart = part;

    // Flip regions and event markers are not part of what this benchmark varies, but
    // SceneRenderer::paint takes them, so hand it empty ones rather than a stale overload.
    const TimeBasedFlipRegions flipRegions{};
    const TimeBasedEventMarkers eventMarkers{};

    const auto renderType = getRenderType(part);
    const bool isElite = renderType == RenderType::ELITE_DRUMS;
    const bool isDrums = isDrumLike(part);

    // farFadeEnd defaults past HIGHWAY_POS_END, so the board geometry has to be stretched to
    // cover the runway the fade draws over — same two lines HighwayComponent and the render
    // harness run. The old code pinned the fade to 0.85 instead, which put every gem in the
    // top 15% of the highway at zero opacity and still paid to blit it.
    renderer.highwayPosEnd = std::max(PositionConstants::HIGHWAY_POS_END, renderer.farFadeEnd);

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

            const auto& tw = scenarioData.trackWindow;
            const auto& sw = scenarioData.sustainWindow;
            const auto& gm = scenarioData.gridlines;

            // Size every blit source to this viewport, exactly as HighwayComponent does.
            // Without this the whole run measures full-res masters resampled into small
            // destinations, which is a harness artifact and not what the plugin does.
            // --no-rescale leaves every blit reading from the full-res master. Paired against a
            // normal run it answers one question: does this platform's blit cost track SOURCE size
            // (CPU rasterizer) or DESTINATION size (GPU)? The ratio is self-normalizing, so it
            // stays meaningful on slow hardware and inside a VM.
            if (!noRescale)
                renderer.rescaleAssets(res.width);

            if (isElite)
                trackRenderer.setLaneCoords(PositionConstants::eliteDrumBezierLaneCoords.data(),
                                            (int)PositionConstants::ELITE_DRUM_LANE_COUNT);
            else
                trackRenderer.setLaneCoords(isDrums ? renderer.drumLaneCoordsLocal
                                                    : renderer.guitarLaneCoordsLocal,
                                            isDrums ? (int)PositionConstants::DRUM_LANE_COUNT
                                                    : (int)PositionConstants::GUITAR_LANE_COUNT);

            // Replicate HighwayComponent::updateOverflow(): the far end of the highway sits
            // above the visible slot, in a computed overflow band.
            auto farEdge = PositionMath::getFretboardEdge(
                renderType, renderer.farFadeEnd, (uint)res.width, (uint)res.height,
                PositionConstants::HIGHWAY_POS_START, renderer.highwayPosEnd);
            const int overflow = std::max(0, (int)std::ceil(-farEdge.centerY));
            const int totalH   = res.height + overflow;

            // Canvas covers the overflow band, as the plugin's component does. Rendering into a
            // res.height canvas clipped away every far-end gem and gridline that the real frame
            // rasterizes, and left the track layers nowhere to land.
            juce::Image canvas(juce::Image::ARGB, res.width, totalH, true);

            // --track-oversize models what the plugin actually does today: the shared cache is
            // baked once at full scene resolution and blitted down into each ~960px slot. At 1.0
            // the bake matches its destination, which is what Phase 2.1 is trying to achieve, so
            // the pair of runs brackets 2.1's whole value.
            const int bakeW  = std::max(1, (int)std::lround(res.width  * trackOversize));
            const int bakeH  = std::max(1, (int)std::lround(res.height * trackOversize));
            const int bakeOv = (int)std::lround(overflow * trackOversize);

            trackRenderer.rebuild(bakeW, bakeH, bakeOv,
                                  renderer.farFadeEnd, renderer.farFadeLen, renderer.farFadeCurve,
                                  renderer.highwayPosEnd);

            // The baked track layers are full-viewport composites and the single biggest block
            // of blit work in a real frame. Baking them and never registering them, which is
            // what this harness did until now, measured a frame the plugin never draws.
            renderer.overlayYOffset = overflow;
            renderer.clearOverlays();
            renderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &trackRenderer.getLayerImage(TrackRenderer::SIDEBARS));
            renderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &trackRenderer.getLayerImage(TrackRenderer::LANE_LINES));
            renderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &trackRenderer.getLayerImage(TrackRenderer::STRIKELINE));
            renderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &trackRenderer.getLayerImage(TrackRenderer::CONNECTORS));

            std::vector<double> notesSamples, sustainsSamples, gridlinesSamples, textureSamples, trackSamples, executeSamples, totalSamples;
            std::map<DrawOrder, std::vector<double>> layerSamples;

            // Warmup
            for (int w = 0; w < warmupCount; w++)
            {
                canvas.clear({0, 0, res.width, totalH});
                juce::Graphics g(canvas);
                if (lowQuality)
                    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                trackRenderer.paint(g, res.width, totalH);
                if (withTexture)
                    trackRenderer.paintTexture(g, (float)w * 0.1f, res.width, totalH);
                if (overflow > 0)
                    g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));
                renderer.paint(g, res.width, res.height, tw, sw, gm, flipRegions, eventMarkers,
                               windowStart, windowEnd, false);
            }

            for (int i = 0; i < iterationCount; i++)
            {
                canvas.clear({0, 0, res.width, totalH});
                juce::Graphics g(canvas);
                // CoreGraphicsContext never calls CGContextSetInterpolationQuality, so the default
                // here is whatever the context inherits. This flag forces kCGInterpolationNone to
                // measure how much of a frame is resampling rather than compositing.
                if (lowQuality)
                    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);

                // The faded track fill, timed on its own. It is one full-viewport blit and the
                // target of 2.1, so it needs to be separable from the layer composites.
                double trackUs = 0.0;
                {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    trackRenderer.paint(g, res.width, totalH);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    trackUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
                }
                trackSamples.push_back(trackUs);

                // Time texture rendering separately
                double texUs = 0.0;
                if (withTexture)
                {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    trackRenderer.paintTexture(g, (float)i * 0.02f, res.width, totalH);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    texUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
                }
                textureSamples.push_back(texUs);

                if (overflow > 0)
                    g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));

                renderer.paint(g, res.width, res.height, tw, sw, gm, flipRegions, eventMarkers,
                               windowStart, windowEnd, false);

                const auto& pt = renderer.lastPhaseTiming;
                notesSamples.push_back(pt.notes_us);
                sustainsSamples.push_back(pt.sustains_us);
                gridlinesSamples.push_back(pt.gridlines_us);
                executeSamples.push_back(pt.execute_us);
                totalSamples.push_back(pt.total_us + texUs + trackUs);

                for (int li = 0; li < DRAW_ORDER_COUNT; ++li)
                    if (pt.layer_us[li] > 0.0)
                        layerSamples[(DrawOrder)li].push_back(pt.layer_us[li]);
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
            r.texture = computeStats(textureSamples);
            r.track = computeStats(trackSamples);
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
    bool withTexture = false;
    bool noRescale = false;
    bool lowQuality = false;
    float trackOversize = 1.0f;
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
        else if (arg == "--texture")
            withTexture = true;
        else if (arg == "--no-rescale")
            noRescale = true;
        else if (arg == "--low-quality")
            lowQuality = true;
        else if (arg == "--track-oversize" && i + 1 < argc)
            trackOversize = (float)std::atof(argv[++i]);
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::ValueTree state("PluginState");
    state.setProperty("part", (int)Part::GUITAR, nullptr);
    state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
    state.setProperty("autoHopo", false, nullptr);
    state.setProperty("hopoThreshold", 2, nullptr);
    state.setProperty("starPower", 0, nullptr);
    state.setProperty("hitIndicators", 0, nullptr);
    state.setProperty("dynamics", 1, nullptr);
    state.setProperty("kick2x", 1, nullptr);

    AssetManager assets;
    SceneRenderer renderer(state, assets);
    renderer.collectPhaseTiming = true;

    TrackRenderer trackRenderer(state);

    // The highway texture is off unless the user picks one, so it is off here too. It used
    // to be forced on, which put a full-viewport per-pixel scanline pass into every result.
    juce::Image synthTexture;
    if (withTexture)
    {
        synthTexture = juce::Image(juce::Image::ARGB, 256, 256, true);
        juce::Graphics tg(synthTexture);
        for (int y = 0; y < 256; y += 8)
        {
            tg.setColour(juce::Colour::fromHSV((float)y / 256.0f, 0.6f, 0.8f, 0.5f));
            tg.fillRect(0, y, 256, 4);
        }
        trackRenderer.setTexture(synthTexture);
    }

    double windowStart = 0.0;
    double windowEnd = 1.0;

    std::vector<BenchResult> results;
    int totalCombos = SCENARIO_COUNT * RESOLUTION_COUNT * 4; // guitar + bass + drums + elite
    int combo = 0;

    // The four parts a full band puts on screen at once. Bass shares guitar's geometry but
    // gets its own row, because a band is what the multi-highway layout actually costs.
    struct InstrumentSpec { const char* name; Part part; int maxColumn; bool noSustains; };
    constexpr InstrumentSpec INSTRUMENTS[] = {
        {"guitar",      Part::GUITAR,      5, false},  // 0=open, 1-5=frets, has sustains
        {"bass",        Part::BASS,        5, false},
        {"drums",       Part::DRUMS,       4, true },  // 0=kick, 1-4=pads, no sustains
        {"elite_drums", Part::ELITE_DRUMS, 8, true },  // 0=kick, 1-8 across the e-kit
    };

    for (const auto& inst : INSTRUMENTS)
        runInstrument(inst.name, inst.part, inst.maxColumn, inst.noSustains,
                      state, renderer, trackRenderer, withTexture, noRescale, lowQuality, trackOversize,
                      iterationCount, warmupCount, windowStart, windowEnd,
                      results, combo, totalCombos);

    // Saving comes first: a regression makes compareBaseline return non-zero, and returning on
    // that used to skip the save entirely — so you could never re-baseline a slower build, which
    // is the one time you actually want to.
    if (saveBaseline && !baselinePath.empty())
    {
        std::ofstream out(baselinePath);
        out << resultsToJson(results);
        std::cout << "Baseline saved to " << baselinePath << std::endl << std::endl;
    }

    // Output
    if (jsonOutput)
    {
        std::cout << resultsToJson(results);
    }
    else
    {
        printTable(results);

        // Comparing against a baseline this run just overwrote would only ever say "no change".
        if (!baselinePath.empty() && !saveBaseline)
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

    return 0;
}
