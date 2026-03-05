#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "DebugPlaybackController.h"
#include "DebugMidiFilePlayer.h"
#include "../Visual/Renderers/SceneRenderer.h"
#include "../Visual/Renderers/TrackRenderer.h"
#include "../Visual/Utils/RenderTiming.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "../Utils/Utils.h"

class ChartPreviewAudioProcessor;
class ToolbarComponent;

class DebugEditorController
{
public:
    DebugEditorController();

    void init(juce::Component& parent, ChartPreviewAudioProcessor& processor,
              juce::ValueTree& state, bool isStandalone);

    void wireCallbacks(ToolbarComponent& toolbar,
                       SceneRenderer& sceneRenderer,
                       TrackRenderer& trackRenderer,
                       std::function<void()> rebuildTrackImage,
                       std::function<void()> repaintEditor);

    // Called from onFrame() — advances playhead, handles looping
    void onFrame(PPQ& lastKnownPosition, bool& lastPlayingState);

    // Called from paint() — draws profiler overlay
    void drawProfilerOverlay(juce::Graphics& g, const SceneRenderer& sceneRenderer);

    // Paint path for standalone debug mode
    void paintStandalone(juce::Graphics& g,
                         SceneRenderer& sceneRenderer,
                         MidiInterpreter& midiInterpreter,
                         double displaySizeInPPQ,
                         double displayWindowTimeSeconds);

    // Scroll offset computation for standalone mode
    // Returns true if handled (caller should use outOffset), false if not standalone
    bool computeScrollOffset(float& outOffset, double displayWindowTimeSeconds) const;

    // Key/wheel handlers — return true if consumed
    bool keyPressed(const juce::KeyPress& key, ToolbarComponent& toolbar);
    bool mouseWheelMove(const juce::MouseWheelDetails& wheel, bool shiftDown,
                        double scrollNormalBeats, double scrollShiftBeats);

    // State queries
    bool isStandalone() const { return standalone; }
    bool isNotesActive() const { return playbackController.isNotesActive(); }

    // Console
    juce::TextEditor& getConsole() { return consoleOutput; }
    juce::TextButton& getClearButton() { return clearLogsButton; }

    // Profiler timing target for texture phase
    double textureRender_us = 0.0;
    bool collectPhaseTiming() const;

private:
    bool standalone = false;
    juce::ValueTree* statePtr = nullptr;
    ChartPreviewAudioProcessor* processorPtr = nullptr;

    DebugPlaybackController playbackController;

    // Profiler
    static constexpr int PROFILER_RING_SIZE = 60;
    double profilerRing[PROFILER_RING_SIZE] = {};
    int profilerRingIndex = 0;

    // Console
    juce::TextEditor consoleOutput;
    juce::TextButton clearLogsButton;

    // Debug chart loading
    TempoTimeSignatureMap debugMidiTempoMap;
    double debugChartLengthInBeats = 0.0;
    void loadDebugChart(int index);

    struct DebugChartEntry { const char* data; int size; };
    static constexpr int CHART_REGISTRY_SIZE = 8;
    static const DebugChartEntry debugChartRegistry[CHART_REGISTRY_SIZE];
};

#endif
