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

class ChartchoticAudioProcessor;
class ToolbarComponent;

class DebugEditorController
{
public:
    DebugEditorController();

    void init(juce::Component& parent, ChartchoticAudioProcessor& processor,
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
                         int viewportWidth, int viewportHeight,
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

    // Profiler timing targets
    double textureRender_us = 0.0;
    double frameDelta_us = 0.0;
    double lockWait_us = 0.0;
    bool collectPhaseTiming() const;

private:
    bool standalone = false;
    juce::ValueTree* statePtr = nullptr;
    ChartchoticAudioProcessor* processorPtr = nullptr;

    DebugPlaybackController playbackController;

    // Profiler
    static constexpr int PROFILER_RING_SIZE = 60;
    double profilerRing[PROFILER_RING_SIZE] = {};
    int profilerRingIndex = 0;
    double frameDeltaRing[PROFILER_RING_SIZE] = {};
    int frameDeltaRingIndex = 0;
    double lockWaitRing[PROFILER_RING_SIZE] = {};
    int lockWaitRingIndex = 0;

    // Console
    juce::TextEditor consoleOutput;
    juce::TextButton clearLogsButton;

    // Debug chart loading (scans assets/midi/ directory)
    TempoTimeSignatureMap debugMidiTempoMap;
    double debugChartLengthInBeats = 0.0;
    void loadDebugChart(int index);
    void scanMidiDirectory();

    struct DebugChartEntry { juce::String name; juce::File file; };
    std::vector<DebugChartEntry> chartEntries; // index 0 = "None" (empty)
};

#endif
