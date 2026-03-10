#ifdef DEBUG

#include "DebugEditorController.h"
#include "../PluginProcessor.h"
#include "../Midi/Processing/MidiInterpreter.h"
#include "../UI/ToolbarComponent.h"
#include "../Visual/Managers/GridlineGenerator.h"
#include "../Utils/TimeConverter.h"

#ifndef CHARTCHOTIC_MIDI_ASSET_DIR
  #define CHARTCHOTIC_MIDI_ASSET_DIR ""
#endif

DebugEditorController::DebugEditorController() {}

void DebugEditorController::init(juce::Component& parent, ChartchoticAudioProcessor& processor,
                                  juce::ValueTree& state, bool isStandalone)
{
    processorPtr = &processor;
    statePtr = &state;
    standalone = isStandalone;

    scanMidiDirectory();

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
                                           HighwayComponent& highway,
                                           std::function<void()> repaintEditor)
{
    auto& dbg = toolbar.getDebugPanel();

    // Pass discovered chart names to the toolbar panel
    {
        std::vector<juce::String> names;
        for (auto& entry : chartEntries)
            names.push_back(entry.name);
        dbg.setChartNames(names);
    }

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
    dbg.onProfilerChanged = [&highway](bool on) {
        highway.getSceneRenderer().collectPhaseTiming = on;
    };

    auto& tune = toolbar.getTuningPanel();
    tune.initDefaults(highway.getTrackRenderer());

    tune.onLayerChanged = [this, &highway](int layer, float scale, float x, float y) {
        bool isDrums = isPart(*statePtr, Part::DRUMS);
        auto* layers = isDrums ? highway.getTrackRenderer().layersDrums : highway.getTrackRenderer().layersGuitar;
        layers[layer] = {scale, x, y};
        highway.rebuildTrack();
    };

    tune.onTileStepChanged = [&highway](float step) {
        highway.getTrackRenderer().tileStep = step;
        highway.rebuildTrack();
    };

    tune.onTileScaleStepChanged = [&highway](float step) {
        highway.getTrackRenderer().tileScaleStep = step;
        highway.rebuildTrack();
    };

    tune.onTextureScaleChanged = [&highway, repaintEditor](float val) {
        highway.getTrackRenderer().textureScale = val;
        repaintEditor();
    };

    tune.onTextureOpacityChanged = [&highway, repaintEditor](float val) {
        highway.getTrackRenderer().textureOpacity = val;
        repaintEditor();
    };

    tune.onPolyShadeChanged = [&highway]() {
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
    };

    tune.onPerspectiveChanged = [&highway]() {
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
    };

    tune.onTuningChanged = [&tune, &highway, repaintEditor]() {
        tune.applyTo(highway.getSceneRenderer());
        repaintEditor();
    };

    tune.onLaneCoordsChanged = [&tune, &highway, repaintEditor]() {
        tune.applyTo(highway.getSceneRenderer());
        highway.rebuildTrack();
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

    frameDeltaRing[frameDeltaRingIndex] = frameDelta_us;
    frameDeltaRingIndex = (frameDeltaRingIndex + 1) % PROFILER_RING_SIZE;

    lockWaitRing[lockWaitRingIndex] = lockWait_us;
    lockWaitRingIndex = (lockWaitRingIndex + 1) % PROFILER_RING_SIZE;

    auto ringAvg = [](const double* ring, int size) {
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < size; ++i)
            if (ring[i] > 0.0) { sum += ring[i]; ++count; }
        return count > 0 ? sum / count : 0.0;
    };

    double avgTotal = ringAvg(profilerRing, PROFILER_RING_SIZE);
    double avgFrameDelta = ringAvg(frameDeltaRing, PROFILER_RING_SIZE);
    double avgLockWait = ringAvg(lockWaitRing, PROFILER_RING_SIZE);
    double fps = avgFrameDelta > 0.0 ? 1000000.0 / avgFrameDelta : 0.0;

    juce::String text;
    text << "FPS: " << juce::String(fps, 1) << "\n"
         << "Delta:   " << juce::String((int)avgFrameDelta) << " us\n"
         << "Lock:    " << juce::String((int)avgLockWait) << " us\n"
         << "Notes:   " << juce::String((int)t.notes_us) << " us\n"
         << "Sustain: " << juce::String((int)t.sustains_us) << " us\n"
         << "Grid:    " << juce::String((int)t.gridlines_us) << " us\n"
         << "Anim:    " << juce::String((int)t.animation_us) << " us\n"
         << "Texture: " << juce::String((int)textureRender_us) << " us\n"
         << "Execute: " << juce::String((int)t.execute_us) << " us\n"
         << "Total:   " << juce::String((int)t.total_us) << " us";

    auto layerUs = [&](DrawOrder d) -> int {
        return (int)t.layer_us[static_cast<int>(d)];
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
    g.fillRect(4, 40, 140, 252);
    g.setColour(juce::Colours::white);
    g.drawFittedText(text, 8, 42, 132, 248, juce::Justification::topLeft, 18);
}

void DebugEditorController::buildStandaloneFrameData(HighwayFrameData& out,
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

    out.trackWindow = timeTrackWindow;
    out.sustainWindow = timeSustainWindow;
    out.gridlines = timeGridlineMap;
    out.windowStartTime = 0.0;
    out.windowEndTime = displayWindowTimeSeconds;
    out.isPlaying = playbackController.isPlaying();
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

void DebugEditorController::scanMidiDirectory()
{
    chartEntries.clear();
    chartEntries.push_back({"None", {}});

    juce::File midiDir(CHARTCHOTIC_MIDI_ASSET_DIR);
    if (!midiDir.isDirectory()) return;

    auto files = midiDir.findChildFiles(juce::File::findFiles, false, "*.mid");
    files.sort();

    for (auto& f : files)
    {
        juce::String name = f.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        chartEntries.push_back({name, f});
    }
}

void DebugEditorController::loadDebugChart(int index)
{
    if (index <= 0 || index >= (int)chartEntries.size() || !chartEntries[index].file.existsAsFile())
    {
        {
            const juce::ScopedLock lock(processorPtr->getNoteStateMapLock());
            for (auto& map : processorPtr->getNoteStateMapArray())
                map.clear();
        }
        debugMidiTempoMap.clear();
        return;
    }

    juce::MemoryBlock data;
    chartEntries[index].file.loadFileAsData(data);

    bool isDrums = isPart(*statePtr, Part::DRUMS);

    auto result = DebugMidiFilePlayer::loadMidiFile(
        (const char*)data.getData(), (int)data.getSize(),
        isDrums,
        processorPtr->getNoteStateMapArray(),
        processorPtr->getNoteStateMapLock(),
        processorPtr->getMidiProcessor(),
        *statePtr);

    debugMidiTempoMap = result.tempoMap;
    debugChartLengthInBeats = result.lengthInBeats;
    playbackController.setBPM(result.initialBPM);
}

#endif
