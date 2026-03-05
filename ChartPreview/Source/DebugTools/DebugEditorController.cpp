#ifdef DEBUG

#include "DebugEditorController.h"
#include "../PluginProcessor.h"
#include "../UI/ToolbarComponent.h"
#include "../Visual/Managers/GridlineGenerator.h"
#include "../Utils/TimeConverter.h"

DebugEditorController::DebugEditorController() {}

void DebugEditorController::init(juce::Component& parent, ChartPreviewAudioProcessor& processor,
                                  juce::ValueTree& state, bool isStandalone)
{
    processorPtr = &processor;
    statePtr = &state;
    standalone = isStandalone;

    if (standalone)
        loadDebugChart(playbackController.getChartIndex());

    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    parent.addChildComponent(consoleOutput);

    clearLogsButton.setButtonText("Clear Logs");
    clearLogsButton.onClick = [this]() {
        processorPtr->clearDebugText();
        consoleOutput.clear();
    };
    parent.addChildComponent(clearLogsButton);
}

void DebugEditorController::wireCallbacks(ToolbarComponent& toolbar,
                                           SceneRenderer& sceneRenderer,
                                           TrackRenderer& trackRenderer,
                                           std::function<void()> rebuildTrackImage,
                                           std::function<void()> repaintEditor)
{
    auto& dbg = toolbar.getDebugPanel();

    dbg.onDebugPlayChanged = [this](bool playing) {
        playbackController.setPlaying(playing);
    };
    dbg.onDebugChartChanged = [this](int index) {
        playbackController.setChartIndex(index);
        loadDebugChart(index);
    };
    dbg.onDebugConsoleChanged = [this](bool show) {
        consoleOutput.setVisible(show);
        clearLogsButton.setVisible(show);
    };
    dbg.onProfilerChanged = [&sceneRenderer](bool on) {
        sceneRenderer.collectPhaseTiming = on;
    };

    auto& tune = toolbar.getTuningPanel();
    tune.initDefaults(trackRenderer);

    tune.onLayerChanged = [this, &trackRenderer, rebuildTrackImage](int layer, float scale, float x, float y) {
        bool isDrums = isPart(*statePtr, Part::DRUMS);
        auto* layers = isDrums ? trackRenderer.layersDrums : trackRenderer.layersGuitar;
        layers[layer] = {scale, x, y};
        rebuildTrackImage();
    };

    tune.onTileStepChanged = [&trackRenderer, rebuildTrackImage](float step) {
        trackRenderer.tileStep = step;
        rebuildTrackImage();
    };

    tune.onTileScaleStepChanged = [&trackRenderer, rebuildTrackImage](float step) {
        trackRenderer.tileScaleStep = step;
        rebuildTrackImage();
    };

    tune.onTuningChanged = [&tune, &sceneRenderer, repaintEditor]() {
        tune.applyTo(sceneRenderer);
        repaintEditor();
    };
}

void DebugEditorController::onFrame(PPQ& lastKnownPosition, bool& lastPlayingState)
{
    if (!standalone) return;

    playbackController.advancePlayhead();
    if (debugChartLengthInBeats > 0.0 && playbackController.getCurrentPPQ().toDouble() > debugChartLengthInBeats)
        playbackController.nudgePlayhead(-debugChartLengthInBeats);
    lastKnownPosition = playbackController.getCurrentPPQ();
    if (playbackController.isPlaying())
        lastPlayingState = true;
}

void DebugEditorController::drawProfilerOverlay(juce::Graphics& g, const SceneRenderer& sceneRenderer)
{
    auto& t = sceneRenderer.lastPhaseTiming;

    profilerRing[profilerRingIndex] = t.total_us;
    profilerRingIndex = (profilerRingIndex + 1) % PROFILER_RING_SIZE;

    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < PROFILER_RING_SIZE; ++i)
    {
        if (profilerRing[i] > 0.0)
        {
            sum += profilerRing[i];
            ++count;
        }
    }
    double avgTotal = count > 0 ? sum / count : 0.0;
    double fps = avgTotal > 0.0 ? 1000000.0 / avgTotal : 0.0;

    juce::String text;
    text << "FPS: " << juce::String(fps, 1) << "\n"
         << "Notes:   " << juce::String((int)t.notes_us) << " us\n"
         << "Sustain: " << juce::String((int)t.sustains_us) << " us\n"
         << "Grid:    " << juce::String((int)t.gridlines_us) << " us\n"
         << "Anim:    " << juce::String((int)t.animation_us) << " us\n"
         << "Texture: " << juce::String((int)textureRender_us) << " us\n"
         << "Execute: " << juce::String((int)t.execute_us) << " us\n"
         << "Total:   " << juce::String((int)t.total_us) << " us";

    auto layerUs = [&](DrawOrder d) -> int {
        auto it = t.layer_us.find(d);
        return it != t.layer_us.end() ? (int)it->second : 0;
    };

    text << "\n---\n"
         << "Grid:    " << layerUs(DrawOrder::GRID) << " us\n"
         << "Lane:    " << layerUs(DrawOrder::LANE) << " us\n"
         << "Bar:     " << layerUs(DrawOrder::BAR) << " us\n"
         << "Sustain: " << layerUs(DrawOrder::SUSTAIN) << " us\n"
         << "Note:    " << layerUs(DrawOrder::NOTE) << " us\n"
         << "Overlay: " << layerUs(DrawOrder::OVERLAY) << " us";

    g.setFont(juce::Font(12.0f));
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRect(4, 40, 140, 224);
    g.setColour(juce::Colours::white);
    g.drawFittedText(text, 8, 42, 132, 220, juce::Justification::topLeft, 16);
}

void DebugEditorController::paintStandalone(juce::Graphics& g,
                                             SceneRenderer& sceneRenderer,
                                             MidiInterpreter& midiInterpreter,
                                             double displaySizeInPPQ,
                                             double displayWindowTimeSeconds)
{
    if (!standalone || !playbackController.isNotesActive()) return;

    PPQ trackWindowStartPPQ = playbackController.getCurrentPPQ();
    PPQ trackWindowEndPPQ = trackWindowStartPPQ + PPQ(displaySizeInPPQ);
    PPQ extendedStart = trackWindowStartPPQ - PPQ(displaySizeInPPQ);

    TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
    SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, trackWindowStartPPQ);

    double bpm = playbackController.getBPM();
    auto ppqToTime = [bpm](double ppq) { return ppq * (60.0 / bpm); };

    TempoTimeSignatureMap tempoTimeSigMap = debugMidiTempoMap;
    if (tempoTimeSigMap.empty())
        tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), bpm, 4, 4);

    PPQ cursorPPQ = trackWindowStartPPQ;
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
    TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

    sceneRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap,
                        0.0, displayWindowTimeSeconds, playbackController.isPlaying());
}

bool DebugEditorController::computeScrollOffset(float& outOffset, double displayWindowTimeSeconds) const
{
    if (!standalone) return false;

    double bpm = playbackController.getBPM();
    double ppq = playbackController.getCurrentPPQ().toDouble();

    int latencyOffsetMs = (int)statePtr->getProperty("latencyOffsetMs");
    double latencyOffsetBeats = (latencyOffsetMs / 1000.0) * (bpm / 60.0);
    ppq -= latencyOffsetBeats;

    double absoluteTime = ppq * (60.0 / bpm);
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    double raw = std::fmod(-absoluteTime * scrollRate, 1.0);
    outOffset = (float)(raw < 0.0 ? raw + 1.0 : raw);
    return true;
}

bool DebugEditorController::keyPressed(const juce::KeyPress& key, ToolbarComponent& toolbar)
{
    if (!standalone) return false;

    if (key == juce::KeyPress::spaceKey)
    {
        playbackController.togglePlay();
        toolbar.setDebugPlay(playbackController.isPlaying());
        return true;
    }
    return false;
}

bool DebugEditorController::mouseWheelMove(const juce::MouseWheelDetails& wheel, bool shiftDown,
                                            double scrollNormalBeats, double scrollShiftBeats)
{
    if (!standalone) return false;

    double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
    double jumpBeats = shiftDown ? scrollShiftBeats : scrollNormalBeats;
    playbackController.nudgePlayhead(wheelDelta * jumpBeats);
    return true;
}

bool DebugEditorController::collectPhaseTiming() const
{
    // Delegates to the profiler toggle state — but that's on SceneRenderer.
    // This is just a convenience for the texture scoped measure.
    return false; // Caller should check sceneRenderer.collectPhaseTiming directly
}

void DebugEditorController::loadDebugChart(int index)
{
    constexpr int registrySize = CHART_REGISTRY_SIZE;
    if (index <= 0 || index >= registrySize)
    {
        {
            const juce::ScopedLock lock(processorPtr->getNoteStateMapLock());
            for (auto& map : processorPtr->getNoteStateMapArray())
                map.clear();
        }
        debugMidiTempoMap.clear();
        return;
    }

    const auto& entry = debugChartRegistry[index];
    bool isDrums = isPart(*statePtr, Part::DRUMS);

    auto result = DebugMidiFilePlayer::loadMidiFile(
        entry.data, entry.size,
        isDrums,
        processorPtr->getNoteStateMapArray(),
        processorPtr->getNoteStateMapLock(),
        processorPtr->getMidiProcessor(),
        *statePtr);

    debugMidiTempoMap = result.tempoMap;
    debugChartLengthInBeats = result.lengthInBeats;
    playbackController.setBPM(result.initialBPM);
}

const DebugEditorController::DebugChartEntry DebugEditorController::debugChartRegistry[] = {
    {nullptr,                               0},
    {BinaryData::test_mid,                  BinaryData::test_midSize},
    {BinaryData::stress_mid,                BinaryData::stress_midSize},
    {BinaryData::sleepy_tea_mid,            BinaryData::sleepy_tea_midSize},
    {BinaryData::further_side_mid,          BinaryData::further_side_midSize},
    {BinaryData::scarlet_mid,               BinaryData::scarlet_midSize},
    {BinaryData::everything_went_black_mid, BinaryData::everything_went_black_midSize},
    {BinaryData::akroasis_mid,              BinaryData::akroasis_midSize},
};

#endif
