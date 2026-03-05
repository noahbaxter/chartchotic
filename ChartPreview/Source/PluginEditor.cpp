/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UpdateChecker.h"

#ifdef DEBUG
#include "DebugTools/DebugMidiFilePlayer.h"
#endif

// CI injects CHARTPREVIEW_VERSION_STRING with full version (e.g. 0.9.5-dev.20260226.abc1234)
// Falls back to JucePlugin_VersionString from .jucer (base semver), then "dev" for unset builds
#ifdef CHARTPREVIEW_VERSION_STRING
  #define CHART_PREVIEW_VERSION CHARTPREVIEW_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHART_PREVIEW_VERSION JucePlugin_VersionString
#else
  #define CHART_PREVIEW_VERSION "dev"
#endif

//==============================================================================
ChartPreviewAudioProcessorEditor::ChartPreviewAudioProcessorEditor(ChartPreviewAudioProcessor &p, juce::ValueTree &state)
    : AudioProcessorEditor(&p),
      state(state),
      audioProcessor(p),
      midiInterpreter(state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock()),
      sceneRenderer(state, midiInterpreter),
      trackRenderer(state),
      toolbar(state)
{
    setLookAndFeel(&chartPreviewLnF);

    // Set up resize constraints
    constrainer.setMinimumSize(minWidth, minHeight);
    constrainer.setFixedAspectRatio(aspectRatio);
    setConstrainer(&constrainer);
    setResizable(true, true);

    setSize(defaultWidth, defaultHeight);

    setWantsKeyboardFocus(true);

    latencyInSeconds = audioProcessor.latencyInSeconds;

#ifdef DEBUG
    debugStandalone = juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
#endif

    initAssets();
    initToolbarCallbacks();
    initBottomBar();

    addAndMakeVisible(toolbar);

    #ifdef DEBUG
    if (debugStandalone)
        loadDebugChart(debugController.getChartIndex());

    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    addChildComponent(consoleOutput);

    clearLogsButton.setButtonText("Clear Logs");
    clearLogsButton.onClick = [this]() {
        audioProcessor.clearDebugText();
        consoleOutput.clear();
    };
    addChildComponent(clearLogsButton);
    #endif

    loadState();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void ChartPreviewAudioProcessorEditor::onFrame()
{
    if (targetFrameInterval > 0.0)
    {
        juce::int64 now = juce::Time::getHighResolutionTicks();
        double elapsed = (now - lastFrameTicks)
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (elapsed < targetFrameInterval)
            return;
        lastFrameTicks = now;
    }

    printCallback();

    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    toolbar.setReaperMode(isReaperMode);
    toolbar.updateVisibility();

    // Track position changes for render logic
    if (auto* playHead = audioProcessor.getPlayHead()) {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue()) {
            PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
            bool isCurrentlyPlaying = positionInfo->getIsPlaying();

            lastKnownPosition = currentPosition;
            lastPlayingState = isCurrentlyPlaying;

            // In REAPER mode, throttled cache invalidation while paused to pick up MIDI edits
            // Time-based: ~50ms interval regardless of display refresh rate
            if (isReaperMode && !isCurrentlyPlaying)
            {
                juce::int64 now = juce::Time::getHighResolutionTicks();
                double elapsed = (now - lastCacheInvalidationTicks)
                    / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
                if (elapsed >= 0.05)
                {
                    lastCacheInvalidationTicks = now;
                    audioProcessor.invalidateReaperCache();
                }
            }
            else
            {
                lastCacheInvalidationTicks = 0;
            }
        }
    }

#ifdef DEBUG
    if (debugStandalone)
    {
        debugController.advancePlayhead();
        if (debugChartLengthInBeats > 0.0 && debugController.getCurrentPPQ().toDouble() > debugChartLengthInBeats)
            debugController.nudgePlayhead(-debugChartLengthInBeats);
        lastKnownPosition = debugController.getCurrentPPQ();
        if (debugController.isPlaying())
            lastPlayingState = true;
    }
#endif

    repaint();
}

void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImageDefault = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    backgroundImageRed = juce::ImageCache::getFromMemory(BinaryData::background_red_jpg, BinaryData::background_red_jpgSize);

    // Load REAPER logo SVG
    reaperLogo = juce::Drawable::createFromImageData(BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);

    scanHighwayTextures();
}

void ChartPreviewAudioProcessorEditor::rebuildFadedTrackImage()
{
    bool isDrums = isPart(state, Part::DRUMS);
    float wNear = isDrums ? sceneRenderer.fretboardWidthScaleNearDrums : sceneRenderer.fretboardWidthScaleNearGuitar;
    float wMid  = isDrums ? sceneRenderer.fretboardWidthScaleMidDrums  : sceneRenderer.fretboardWidthScaleMidGuitar;
    float wFar  = isDrums ? sceneRenderer.fretboardWidthScaleFarDrums  : sceneRenderer.fretboardWidthScaleFarGuitar;

    trackRenderer.rebuild(getWidth(), getHeight(),
                             sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve,
                             wNear, wMid, wFar, sceneRenderer.highwayPosEnd);

    // Register track layer images as scene overlays at interleaved z-positions
    sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &trackRenderer.getLayerImage(TrackRenderer::STRIKELINE));
    sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &trackRenderer.getLayerImage(TrackRenderer::LANE_LINES));
    sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &trackRenderer.getLayerImage(TrackRenderer::SIDEBARS));
    sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &trackRenderer.getLayerImage(TrackRenderer::CONNECTORS));
}

void ChartPreviewAudioProcessorEditor::initToolbarCallbacks()
{
    toolbar.onSkillChanged = [this](int id) {
        state.setProperty("skillLevel", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onPartChanged = [this](int id) {
        state.setProperty("part", id, nullptr);
        audioProcessor.refreshMidiDisplay();
        rebuildFadedTrackImage();
#ifdef DEBUG
        if (debugStandalone && debugController.isNotesActive())
            loadDebugChart(debugController.getChartIndex());
#endif
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onAutoHopoChanged = [this](int id) {
        state.setProperty("autoHopo", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onNotesChanged = [this](bool on) {
        state.setProperty("showNotes", on ? 1 : 0, nullptr);
        sceneRenderer.showNotes = on;
        repaint();
    };

    toolbar.onSustainsChanged = [this](bool on) {
        state.setProperty("showSustains", on ? 1 : 0, nullptr);
        sceneRenderer.showSustains = on;
        repaint();
    };

    toolbar.onLanesChanged = [this](bool on) {
        state.setProperty("showLanes", on ? 1 : 0, nullptr);
        sceneRenderer.showLanes = on;
        repaint();
    };

    toolbar.onGridlinesChanged = [this](bool on) {
        state.setProperty("showGridlines", on ? 1 : 0, nullptr);
        sceneRenderer.showGridlines = on;
        repaint();
    };

    toolbar.onHitIndicatorsChanged = [this](bool on) {
        state.setProperty("hitIndicators", on ? 1 : 0, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onStarPowerChanged = [this](bool on) {
        state.setProperty("starPower", on ? 1 : 0, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onKick2xChanged = [this](bool on) {
        state.setProperty("kick2x", on ? 1 : 0, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onDynamicsChanged = [this](bool on) {
        state.setProperty("dynamics", on ? 1 : 0, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onNoteSpeedChanged = [this](int speed) {
        state.setProperty("noteSpeed", speed, nullptr);
        updateDisplaySizeFromSpeedSlider();

        if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            audioProcessor.invalidateReaperCache();

        repaint();
    };

    toolbar.onFramerateChanged = [this](int id) {
        state.setProperty("framerate", id, nullptr);
        switch (id)
        {
        case 1: targetFrameInterval = 1.0 / 15.0; break;
        case 2: targetFrameInterval = 1.0 / 30.0; break;
        case 3: targetFrameInterval = 1.0 / 60.0; break;
        default: targetFrameInterval = 0.0; break;  // Native
        }
    };

    toolbar.onLatencyChanged = [this](int id) {
        state.setProperty("latency", id, nullptr);
        applyLatencySetting(id);
    };

    toolbar.onLatencyOffsetChanged = [this](int offsetMs) {
        applyLatencyOffsetChange();
    };

    toolbar.onRedBackgroundChanged = [this](bool useRed) {
        state.setProperty("redBackground", useRed ? 1 : 0, nullptr);
        repaint();
    };

    toolbar.onGemScaleChanged = [this](float scale) {
        state.setProperty("gemScale", scale, nullptr);
        repaint();
    };

    toolbar.onHighwayLengthChanged = [this](float length) {
        state.setProperty("highwayLength", length, nullptr);
        sceneRenderer.farFadeEnd = length;
        updateDisplaySizeFromSpeedSlider();  // PPQ window scales with highway length
        rebuildFadedTrackImage();
        repaint();
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty())
            trackRenderer.clearTexture();
        else
            loadHighwayTexture(textureName);
        repaint();
    };

    toolbar.onTextureScaleChanged = [this](float scale) {
        state.setProperty("textureScale", scale, nullptr);
        trackRenderer.textureScale = scale;
        repaint();
    };

    toolbar.onTextureOpacityChanged = [this](float opacity) {
        state.setProperty("textureOpacity", opacity, nullptr);
        trackRenderer.textureOpacity = opacity;
        repaint();
    };

#ifdef DEBUG
    auto& dbg = toolbar.getDebugPanel();

    dbg.onDebugPlayChanged = [this](bool playing) {
        debugController.setPlaying(playing);
    };
    dbg.onDebugChartChanged = [this](int index) {
        debugController.setChartIndex(index);
        loadDebugChart(index);
    };
    dbg.onDebugConsoleChanged = [this](bool show) {
        consoleOutput.setVisible(show);
        clearLogsButton.setVisible(show);
    };
    dbg.onProfilerChanged = [this](bool on) {
        sceneRenderer.collectPhaseTiming = on;
    };

#endif
}

void ChartPreviewAudioProcessorEditor::initBottomBar()
{
    // Version label
    versionLabel.setText(juce::String("v") + CHART_PREVIEW_VERSION, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    versionLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(versionLabel);

    // Update checker
    updateBanner.setButtonText("Update Available");
    updateBanner.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE85D45));
    updateBanner.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    updateBanner.setVisible(false);
    updateBanner.onClick = [this]()
    {
        auto info = updateChecker.getLatestUpdateInfo();
        if (info.downloadUrl.isNotEmpty())
            juce::URL(info.downloadUrl).launchInDefaultBrowser();
    };
    addAndMakeVisible(updateBanner);

    updateChecker.onUpdateCheckComplete = [this](const UpdateChecker::UpdateInfo& info)
    {
        if (info.available)
        {
            updateBanner.setButtonText("Update: " + info.version);
            updateBanner.setVisible(true);
            resized();
        }
    };
    updateChecker.checkForUpdates();
}

//==============================================================================
void ChartPreviewAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& bg = (bool)state["redBackground"] ? backgroundImageRed : backgroundImageDefault;
    g.drawImage(bg, getLocalBounds().toFloat());

    // Visual feedback for REAPER connection status
    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
    {
        // Draw REAPER logo in bottom left corner
        if (reaperLogo)
        {
            const int logoSize = 24;
            const int margin = 10;
            juce::Rectangle<float> logoBounds(margin, getHeight() - logoSize - margin, logoSize, logoSize);
            reaperLogo->drawWithin(g, logoBounds, juce::RectanglePlacement::centred, 0.8f);
        }
    }

    // Draw highway (faded track background)
    trackRenderer.paint(g);

    // Draw scrolling highway texture overlay
    {
#ifdef DEBUG
        ScopedPhaseMeasure m(textureRender_us, sceneRenderer.collectPhaseTiming);
#endif
        trackRenderer.paintTexture(g, computeScrollOffset());
    }

    // Draw the highway - delegate to mode-specific rendering
    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    if (isReaperMode)
    {
        paintReaperMode(g);
    }
    else
    {
        paintStandardMode(g);
    }

#ifdef DEBUG
    if (sceneRenderer.collectPhaseTiming)
        drawProfilerOverlay(g);
#endif
}

void ChartPreviewAudioProcessorEditor::paintReaperMode(juce::Graphics& g)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

    // Capture playhead once to avoid TOCTOU null dereference
    auto* playHead = audioProcessor.getPlayHead();

    // Apply latency offset to shift display position
    // Positive offset = MORE delay (notes appear higher/further from strikeline)
    // Negative offset = LESS delay (notes appear lower/closer to strikeline)
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    if (playHead)
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            double bpm = positionInfo->getBpm().orFallback(120.0);
            double latencyOffsetSeconds = latencyOffsetMs / 1000.0;
            double latencyOffsetBeats = latencyOffsetSeconds * (bpm / 60.0);
            // Subtract to shift display backward (positive offset = see more future notes)
            trackWindowStartPPQ = trackWindowStartPPQ - PPQ(latencyOffsetBeats);
        }
    }

    PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ; // No latency in REAPER mode

    // Extend window for notes behind cursor
    PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

    // Generate PPQ-based windows from MidiInterpreter
    TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
    SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);

    // Create a lambda that uses REAPER's timeline conversion (handles ALL tempo changes)
    auto& reaperProvider = audioProcessor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    // Get tempo/timesig map from MidiProcessor (make a copy to avoid holding lock)
    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(audioProcessor.getMidiProcessor().tempoTimeSignatureMapLock);
        tempoTimeSigMap = audioProcessor.getMidiProcessor().tempoTimeSignatureMap;
    }

    // Convert everything to time-based (seconds from cursor)
    PPQ cursorPPQ = trackWindowStartPPQ;  // Cursor is at the strikeline
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);

    // Generate gridlines on-the-fly from tempo/timesig map
    TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

    // Use the constant time window from the slider (in seconds)
    // Window is anchored at the strikeline (time 0), extending forward into the future
    double windowStartTime = 0.0;
    double windowEndTime = displayWindowTimeSeconds;

    sceneRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::paintStandardMode(juce::Graphics& g)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

#ifdef DEBUG
    if (debugStandalone && debugController.isNotesActive())
    {
        PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
        PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

        TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
        SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, trackWindowStartPPQ);

        double bpm = debugController.getBPM();
        auto ppqToTime = [bpm](double ppq) { return ppq * (60.0 / bpm); };

        TempoTimeSignatureMap tempoTimeSigMap = debugMidiTempoMap;
        if (tempoTimeSigMap.empty())
            tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), bpm, 4, 4);

        PPQ cursorPPQ = trackWindowStartPPQ;
        TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
        TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
        TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
            tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

        double windowStartTime = 0.0;
        double windowEndTime = displayWindowTimeSeconds;

        sceneRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, debugController.isPlaying());
        return;
    }
#endif

    // Apply latency compensation when playing
    if (audioProcessor.isPlaying)
    {
        // Use smoothed tempo-aware latency to prevent jitter during tempo changes
        PPQ smoothedLatency = smoothedLatencyInPPQ();
        trackWindowStartPPQ = std::max(PPQ(0.0), trackWindowStartPPQ - smoothedLatency);
    }

    // Capture playhead once to avoid TOCTOU null dereference
    auto* playHead = audioProcessor.getPlayHead();

    // Apply latency offset to shift display position (positive only for standard mode)
    // Positive offset = MORE delay (notes appear higher/further from strikeline)
    int latencyOffsetMs = std::max(0, (int)state.getProperty("latencyOffsetMs"));
    if (playHead)
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            double bpm = positionInfo->getBpm().orFallback(120.0);
            double latencyOffsetSeconds = latencyOffsetMs / 1000.0;
            double latencyOffsetBeats = latencyOffsetSeconds * (bpm / 60.0);
            // Subtract to shift display backward (positive offset = see more future notes)
            trackWindowStartPPQ = trackWindowStartPPQ - PPQ(latencyOffsetBeats);
        }
    }

    PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ + smoothedLatencyInPPQ();

    // Update MidiProcessor's visual window bounds to prevent premature cleanup of visible events
    audioProcessor.setMidiProcessorVisualWindowBounds(trackWindowStartPPQ, trackWindowEndPPQ);

    // In non-REAPER mode, use current BPM from playhead (no tempo map available)
    double currentBPM = 120.0;
    if (playHead)
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            currentBPM = positionInfo->getBpm().orFallback(120.0);
        }
    }

    // Extend window for notes behind cursor
    PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

    // Generate PPQ-based windows from MidiInterpreter
    TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
    SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);

    // Simple constant BPM conversion for non-REAPER mode
    auto ppqToTime = [currentBPM](double ppq) { return ppq * (60.0 / currentBPM); };

    // Create a simple tempo/timesig map for standard mode (default 120 BPM, 4/4)
    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(audioProcessor.getMidiProcessor().tempoTimeSignatureMapLock);
        tempoTimeSigMap = audioProcessor.getMidiProcessor().tempoTimeSignatureMap;

        // If empty, add a default 4/4, currentBPM event at the start
        if (tempoTimeSigMap.empty()) {
            tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), currentBPM, 4, 4);
        }
    }

    // Convert everything to time-based (seconds from cursor)
    PPQ cursorPPQ = trackWindowStartPPQ;
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);

    // Generate gridlines on-the-fly from tempo/timesig map
    TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

    // Use the constant time window from the slider (in seconds)
    // Window is anchored at the strikeline (time 0), extending forward into the future
    double windowStartTime = 0.0;
    double windowEndTime = displayWindowTimeSeconds;

    sceneRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::resized()
{
    const int margin = 10;

    // Toolbar at top
    toolbar.setBounds(0, 0, getWidth(), ToolbarComponent::toolbarHeight);

    // Version label (bottom-left, next to REAPER logo)
    const int versionWidth = 250;
    const int versionHeight = 15;
    versionLabel.setBounds(45, getHeight() - versionHeight - 12, versionWidth, versionHeight);

    // Update banner (bottom-right) — always set bounds so it appears when toggled visible
    updateBanner.setBounds(getWidth() - 150, getHeight() - 30, 140, 22);

    #ifdef DEBUG
    clearLogsButton.setBounds(margin, ToolbarComponent::toolbarHeight + 4, 100, 20);
    consoleOutput.setBounds(margin, ToolbarComponent::toolbarHeight + 28, getWidth() - (2 * margin), getHeight() - ToolbarComponent::toolbarHeight - 38);
    #endif

    rebuildFadedTrackImage();
}

void ChartPreviewAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    displayWindowTimeSeconds = 7.87 / toolbar.getChartSpeedSlider().getValue();

    // PPQ window must cover the full visible highway (farFadeEnd * window time) at worst-case tempo.
    // At 300 BPM, 1 second = 5 quarter notes. Base window of 30 PPQ scaled by highway length.
    const double BASE_PPQ_WINDOW = 30.0;
    double hwScale = std::max(1.0, (double)sceneRenderer.farFadeEnd);
    displaySizeInPPQ = PPQ(BASE_PPQ_WINDOW * hwScale);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartPreviewAudioProcessorEditor::loadState()
{
    // Default to random highway texture if no saved preference (before toolbar reads state)
    if (!state.hasProperty("highwayTexture") && highwayTextureNames.size() > 0)
    {
        auto rng = juce::Random();
        int idx = rng.nextInt(highwayTextureNames.size());
        state.setProperty("highwayTexture", highwayTextureNames[idx], nullptr);
    }

    toolbar.loadState();

    // Restore render toggle flags (default ON if property missing from saved state)
    sceneRenderer.showNotes = !state.hasProperty("showNotes") || (bool)state["showNotes"];
    sceneRenderer.showSustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
    sceneRenderer.showLanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
    sceneRenderer.showGridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];

    // Restore highway length
    if (state.hasProperty("highwayLength"))
        sceneRenderer.farFadeEnd = juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"]);

    // Load highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty() && highwayTextureNames.contains(savedTexture))
        loadHighwayTexture(savedTexture);

    // Restore texture parameters
    if (state.hasProperty("textureScale"))
        trackRenderer.textureScale = (float)state["textureScale"];
    if (state.hasProperty("textureOpacity"))
        trackRenderer.textureOpacity = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);

    // Apply side-effects that listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int frId = (int)state["framerate"];
    // Migrate old 120/144 FPS options (ids 4,5) and unset (0) to Native (id 4)
    if (frId < 1 || frId > 4)
        frId = 4;
    targetFrameInterval = (frId == 1) ? 1.0 / 15.0 :
                          (frId == 2) ? 1.0 / 30.0 :
                          (frId == 3) ? 1.0 / 60.0 : 0.0;

    updateDisplaySizeFromSpeedSlider();
    rebuildFadedTrackImage();
}

void ChartPreviewAudioProcessorEditor::applyLatencySetting(int latencyValue)
{
    switch (latencyValue) {
    case 1: latencyInSeconds = 0.250; break;
    case 2: latencyInSeconds = 0.500; break;
    case 3: latencyInSeconds = 0.750; break;
    case 4: latencyInSeconds = 1.000; break;
    case 5: latencyInSeconds = 1.500; break;
    default: latencyInSeconds = 0.500; break;
    }
    audioProcessor.setLatencyInSeconds(latencyInSeconds);
}

void ChartPreviewAudioProcessorEditor::applyLatencyOffsetChange()
{
    auto& latencyOffsetInput = toolbar.getLatencyOffsetInput();
    int offsetValue = latencyOffsetInput.getText().getIntValue();

    // Get pipeline type to determine valid range
    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    int minValue = isReaperMode ? -2000 : 0;
    int maxValue = 2000;

    if (offsetValue >= minValue && offsetValue <= maxValue)
    {
        state.setProperty("latencyOffsetMs", offsetValue, nullptr);
        audioProcessor.refreshMidiDisplay();

        // In REAPER mode, invalidate cache to immediately apply offset
        if (isReaperMode)
        {
            audioProcessor.invalidateReaperCache();
        }

        repaint();
    }
    else
    {
        // Invalid value, restore previous
        latencyOffsetInput.setText(juce::String((int)state["latencyOffsetMs"]), false);
    }
}

juce::ComponentBoundsConstrainer* ChartPreviewAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartPreviewAudioProcessorEditor::parentSizeChanged()
{
    AudioProcessorEditor::parentSizeChanged();
}

void ChartPreviewAudioProcessorEditor::scanHighwayTextures()
{
#if JUCE_MAC
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chart Preview/highways");
#elif JUCE_WINDOWS
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chart Preview/highways");
#endif

    highwayTextureNames.clear();

    if (!highwayTextureDirectory.isDirectory())
        highwayTextureDirectory.createDirectory();

    auto files = highwayTextureDirectory.findChildFiles(juce::File::findFiles, false, "*.png");
    files.sort();
    for (auto& f : files)
        highwayTextureNames.add(f.getFileNameWithoutExtension());

    toolbar.setHighwayTextureList(highwayTextureNames);
}

void ChartPreviewAudioProcessorEditor::loadHighwayTexture(const juce::String& filename)
{
    auto file = highwayTextureDirectory.getChildFile(filename + ".png");
    if (!file.existsAsFile())
    {
        trackRenderer.clearTexture();
        return;
    }

    auto img = juce::ImageFileFormat::loadFrom(file);
    if (img.isValid())
        trackRenderer.setTexture(img);
    else
        trackRenderer.clearTexture();
}

float ChartPreviewAudioProcessorEditor::computeScrollOffset()
{
    // Get current playhead position in PPQ and BPM
    double ppq = 0.0;
    double bpm = 120.0;

#ifdef DEBUG
    if (debugStandalone)
    {
        bpm = debugController.getBPM();
        ppq = debugController.getCurrentPPQ().toDouble();
    }
    else
#endif
    {
        if (auto* playHead = audioProcessor.getPlayHead())
        {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue())
            {
                ppq = positionInfo->getPpqPosition().orFallback(0.0);
                bpm = positionInfo->getBpm().orFallback(120.0);
            }
        }
    }

    // Apply latency offset to match note rendering position
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    double latencyOffsetBeats = (latencyOffsetMs / 1000.0) * (bpm / 60.0);
    ppq -= latencyOffsetBeats;

    double absoluteTime = ppq * (60.0 / bpm);

    // Scroll rate: one full highway length per displayWindowTimeSeconds
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    double raw = std::fmod(-absoluteTime * scrollRate, 1.0);
    return (float)(raw < 0.0 ? raw + 1.0 : raw);
}

#ifdef DEBUG
void ChartPreviewAudioProcessorEditor::drawProfilerOverlay(juce::Graphics& g)
{
    auto& t = sceneRenderer.lastPhaseTiming;

    // Update FPS ring buffer
    profilerRing[profilerRingIndex] = t.total_us;
    profilerRingIndex = (profilerRingIndex + 1) % PROFILER_RING_SIZE;

    // Calculate rolling average FPS
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

    // Layer breakdown — fixed list so it doesn't jump around
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

const ChartPreviewAudioProcessorEditor::DebugChartEntry ChartPreviewAudioProcessorEditor::debugChartRegistry[] = {
    {nullptr,                               0},
    {BinaryData::test_mid,                  BinaryData::test_midSize},
    {BinaryData::stress_mid,                BinaryData::stress_midSize},
    {BinaryData::sleepy_tea_mid,            BinaryData::sleepy_tea_midSize},
    {BinaryData::further_side_mid,          BinaryData::further_side_midSize},
    {BinaryData::scarlet_mid,               BinaryData::scarlet_midSize},
    {BinaryData::everything_went_black_mid, BinaryData::everything_went_black_midSize},
    {BinaryData::akroasis_mid,              BinaryData::akroasis_midSize},
};

void ChartPreviewAudioProcessorEditor::loadDebugChart(int index)
{
    constexpr int registrySize = (int)(sizeof(debugChartRegistry) / sizeof(debugChartRegistry[0]));
    if (index <= 0 || index >= registrySize)
    {
        // Clear everything
        {
            const juce::ScopedLock lock(audioProcessor.getNoteStateMapLock());
            for (auto& map : audioProcessor.getNoteStateMapArray())
                map.clear();
        }
        debugMidiTempoMap.clear();
        return;
    }

    const auto& entry = debugChartRegistry[index];
    bool isDrums = isPart(state, Part::DRUMS);

    auto result = DebugMidiFilePlayer::loadMidiFile(
        entry.data, entry.size,
        isDrums,
        audioProcessor.getNoteStateMapArray(),
        audioProcessor.getNoteStateMapLock(),
        audioProcessor.getMidiProcessor(),
        state);

    debugMidiTempoMap = result.tempoMap;
    debugChartLengthInBeats = result.lengthInBeats;
    debugController.setBPM(result.initialBPM);
}
#endif
