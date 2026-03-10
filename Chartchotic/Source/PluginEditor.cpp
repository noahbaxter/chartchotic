/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// CI injects CHARTCHOTIC_VERSION_STRING with full version (e.g. 0.9.5-dev.20260226.abc1234)
// Falls back to JucePlugin_VersionString from .jucer (base semver), then "dev" for unset builds
#ifdef CHARTCHOTIC_VERSION_STRING
  #define CHARTCHOTIC_VERSION CHARTCHOTIC_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHARTCHOTIC_VERSION JucePlugin_VersionString
#else
  #define CHARTCHOTIC_VERSION "dev"
#endif

//==============================================================================
ChartchoticAudioProcessorEditor::ChartchoticAudioProcessorEditor(ChartchoticAudioProcessor &p, juce::ValueTree &state)
    : AudioProcessorEditor(&p),
      state(state),
      audioProcessor(p),
      midiInterpreter(state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock()),
      toolbar(state),
      assetManager(),
      highway(state, assetManager)
{
    setLookAndFeel(&chartPreviewLnF);

    // Set up resize constraints
    constrainer.setMinimumSize(minWidth, minHeight);
    setConstrainer(&constrainer);
    setResizable(true, true);

    // Restore saved window size
    int savedWidth = state.hasProperty("editorWidth") ? (int)state["editorWidth"] : defaultWidth;
    int savedHeight = state.hasProperty("editorHeight") ? (int)state["editorHeight"] : defaultHeight;
    savedWidth = juce::jlimit(minWidth, 4096, savedWidth);
    savedHeight = juce::jlimit(minHeight, 4096, savedHeight);
    setSize(savedWidth, savedHeight);

    setWantsKeyboardFocus(true);

    latencyInSeconds = audioProcessor.latencyInSeconds;

#ifdef DEBUG
    bool isStandalone = juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
    debugController.init(*this, audioProcessor, state, isStandalone);
#endif

    initAssets();
    initToolbarCallbacks();
    toolbar.setLatencyOffsetRange(CALIBRATION_MIN_MS, CALIBRATION_MAX_MS);

    highway.onOverflowChanged = [this]() { resized(); };

    addAndMakeVisible(highway);
    addAndMakeVisible(toolbar);
    initBottomBar();

    loadState();
    resized();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartchoticAudioProcessorEditor::~ChartchoticAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void ChartchoticAudioProcessorEditor::onFrame()
{
    if (targetFrameInterval > 0.0)
    {
        juce::int64 now = juce::Time::getHighResolutionTicks();
        double ticksPerSec = static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        double elapsed = (now - lastFrameTicks) / ticksPerSec;
        if (elapsed < targetFrameInterval)
            return;
        // Advance by target interval to stay on a stable grid.
        // If we overshot by more than a full interval (e.g. app was suspended), snap to now.
        juce::int64 intervalTicks = static_cast<juce::int64>(targetFrameInterval * ticksPerSec);
        lastFrameTicks += intervalTicks;
        if ((now - lastFrameTicks) / ticksPerSec > targetFrameInterval)
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

    // Frame delta tracking (used by FPS overlay and debug profiler)
    {
        double now = juce::Time::getHighResolutionTicks()
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (lastFrameTimestamp > 0.0)
        {
            double delta_us = (now - lastFrameTimestamp) * 1000000.0;
            fpsRing[fpsRingIndex] = delta_us;
            fpsRingIndex = (fpsRingIndex + 1) % FPS_RING_SIZE;
#ifdef DEBUG
            debugController.frameDelta_us = delta_us;
#endif
        }
        lastFrameTimestamp = now;
    }

#ifdef DEBUG
    debugController.onFrame(lastKnownPosition, lastPlayingState);
#endif

    // Build frame data and push to highway
    HighwayFrameData frameData;
    if (isReaperMode)
        buildReaperFrameData(frameData);
    else
        buildStandardFrameData(frameData);
    highway.setFrameData(frameData);
    highway.repaint();

    repaint();
}

void ChartchoticAudioProcessorEditor::initAssets()
{
    backgroundImageDefault = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    backgroundImageCurrent = backgroundImageDefault;

    // Load REAPER logo SVG
    reaperLogo = juce::Drawable::createFromImageData(BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);

    scanBackgrounds();
    scanHighwayTextures();
}

void ChartchoticAudioProcessorEditor::initToolbarCallbacks()
{
    toolbar.onSkillChanged = [this](int id) {
        state.setProperty("skillLevel", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onPartChanged = [this](int id) {
        state.setProperty("part", id, nullptr);
        audioProcessor.refreshMidiDisplay();

        // Recompute farFadeEnd with new instrument scale (slider value unchanged)
        float userLength = state.hasProperty("highwayLength")
            ? (float)state["highwayLength"] : FAR_FADE_DEFAULT;
        highway.setHighwayLength(computeFarFadeEnd(userLength));
        updateDisplaySizeFromSpeedSlider();

        highway.onInstrumentChanged();
#ifdef DEBUG
        toolbar.getTuningPanel().setDrums(isPart(state, Part::DRUMS));
#endif
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onAutoHopoChanged = [this](bool enabled) {
        state.setProperty("autoHopo", enabled, nullptr);
        audioProcessor.refreshMidiDisplay();
    };
    toolbar.onHopoThresholdChanged = [this](int thresholdIndex) {
        state.setProperty("hopoThreshold", thresholdIndex, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onGemsChanged = [this](bool on) {
        state.setProperty("showGems", on, nullptr);
        highway.setShowGems(on);
    };

    toolbar.onBarsChanged = [this](bool on) {
        state.setProperty("showBars", on, nullptr);
        highway.setShowBars(on);
    };

    toolbar.onSustainsChanged = [this](bool on) {
        state.setProperty("showSustains", on, nullptr);
        highway.setShowSustains(on);
    };

    toolbar.onLanesChanged = [this](bool on) {
        state.setProperty("showLanes", on, nullptr);
        highway.setShowLanes(on);
    };

    toolbar.onGridlinesChanged = [this](bool on) {
        state.setProperty("showGridlines", on, nullptr);
        highway.setShowGridlines(on);
    };

    toolbar.onHitIndicatorsChanged = [this](bool on) {
        state.setProperty("hitIndicators", on, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onStarPowerChanged = [this](bool on) {
        state.setProperty("starPower", on, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onKick2xChanged = [this](bool on) {
        state.setProperty("kick2x", on, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onDynamicsChanged = [this](bool on) {
        state.setProperty("dynamics", on, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onTrackChanged = [this](bool on) {
        state.setProperty("showTrack", on, nullptr);
        highway.setShowTrack(on);
    };

    toolbar.onLaneSeparatorsChanged = [this](bool on) {
        state.setProperty("showLaneSeparators", on, nullptr);
        highway.setShowLaneSeparators(on);
    };

    toolbar.onStrikelineChanged = [this](bool on) {
        state.setProperty("showStrikeline", on, nullptr);
        highway.setShowStrikeline(on);
    };

    toolbar.onHighwayChanged = [this](bool on) {
        state.setProperty("showHighway", on, nullptr);
        highway.setShowHighway(on);
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

    toolbar.onShowFpsChanged = [this](bool on) {
        showFps = on;
    };

    toolbar.onLatencyChanged = [this](int id) {
        state.setProperty("latency", id, nullptr);
        applyLatencySetting(id);
    };

    toolbar.onLatencyOffsetChanged = [this](int offsetMs) {
        // State is already set by the slider; apply side-effects
        audioProcessor.refreshMidiDisplay();
        bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
        if (isReaperMode)
            audioProcessor.invalidateReaperCache();
        repaint();
    };

    toolbar.onBackgroundChanged = [this](const juce::String& bgName) {
        state.setProperty("background", bgName, nullptr);
        loadBackground(bgName);
        repaint();
    };

    toolbar.onGemScaleChanged = [this](float scale) {
        state.setProperty("gemScale", scale, nullptr);
        highway.setGemScale(scale);
    };

    toolbar.onBarScaleChanged = [this](float scale) {
        state.setProperty("barScale", scale, nullptr);
        highway.setBarScale(scale);
    };

    toolbar.onHighwayLengthChanged = [this](float length) {
        state.setProperty("highwayLength", length, nullptr);
        highway.setHighwayLength(computeFarFadeEnd(length));
        updateDisplaySizeFromSpeedSlider();
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty())
            highway.clearTexture();
        else
            loadHighwayTexture(textureName);
        repaint();
    };

    toolbar.onTextureScaleChanged = [this](float scale) {
        state.setProperty("textureScale", scale, nullptr);
        highway.setTextureScale(scale);
    };

    toolbar.onTextureOpacityChanged = [this](float opacity) {
        state.setProperty("textureOpacity", opacity, nullptr);
        highway.setTextureOpacity(opacity);
    };

    toolbar.onOpenBackgroundFolder = [this]() {
        backgroundDirectory.revealToUser();
    };

    toolbar.onOpenTextureFolder = [this]() {
        highwayTextureDirectory.revealToUser();
    };

#ifdef DEBUG
    debugController.wireCallbacks(toolbar, highway,
        [this]() { repaint(); });
#endif
}

void ChartchoticAudioProcessorEditor::initBottomBar()
{
    // Version label (clickable when update available)
    versionLabel.setText(juce::String("v") + CHARTCHOTIC_VERSION, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.normalColour = juce::Colours::white.withAlpha(0.6f);
    versionLabel.hoverColour = juce::Colour(Theme::coral);
    versionLabel.setColour(juce::Label::textColourId, versionLabel.normalColour);
    versionLabel.isClickable = [this]() { return updateBanner.hasUpdate(); };
    versionLabel.onClick = [this]() { updateBanner.showPrompt(); };
    versionLabel.onHover = [this](bool h) { updateBanner.setBadgeHovered(h); };
    addAndMakeVisible(versionLabel);

    // Update checker
    addAndMakeVisible(updateBanner);

    updateChecker.onUpdateCheckComplete = [this](const UpdateChecker::UpdateInfo& info)
    {
        if (info.available)
        {
            updateBanner.setUpdateInfo(info.version, info.downloadUrl);
            versionLabel.normalColour = juce::Colours::white.withAlpha(0.8f);
            versionLabel.setColour(juce::Label::textColourId, versionLabel.normalColour);
            resized();
        }
    };
    updateChecker.checkForUpdates();
}

//==============================================================================
void ChartchoticAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.drawImage(backgroundImageCurrent, getLocalBounds().toFloat());

    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
    {
        if (reaperLogo)
        {
            float s = (float)getHeight() / (float)defaultHeight;
            int logoSize = juce::roundToInt(24.0f * s);
            int logoMargin = juce::roundToInt(10.0f * s);
            juce::Rectangle<float> logoBounds((float)logoMargin, (float)(getHeight() - logoSize - logoMargin),
                                              (float)logoSize, (float)logoSize);
            reaperLogo->drawWithin(g, logoBounds, juce::RectanglePlacement::centred, 0.8f);
        }
    }
}

void ChartchoticAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    if (showFps)
        drawFpsOverlay(g);

#ifdef DEBUG
    if (highway.getSceneRenderer().collectPhaseTiming)
        debugController.drawProfilerOverlay(g, highway.getSceneRenderer());
#endif
}

void ChartchoticAudioProcessorEditor::buildReaperFrameData(HighwayFrameData& out)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

    // Capture playhead once to avoid TOCTOU null dereference
    auto* playHead = audioProcessor.getPlayHead();

    // Apply latency offset (ms added to playhead time)
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
    TrackWindow ppqTrackWindow;
    SustainWindow ppqSustainWindow;
    {
#ifdef DEBUG
        ScopedPhaseMeasure lm(debugController.lockWait_us, highway.getSceneRenderer().collectPhaseTiming);
#endif
        ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
        ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    }

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

    out.trackWindow = timeTrackWindow;
    out.sustainWindow = timeSustainWindow;
    out.gridlines = timeGridlineMap;
    out.windowStartTime = 0.0;
    out.windowEndTime = displayWindowTimeSeconds;
    out.isPlaying = audioProcessor.isPlaying;
    out.scrollOffset = computeScrollOffset();
}

void ChartchoticAudioProcessorEditor::buildStandardFrameData(HighwayFrameData& out)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

#ifdef DEBUG
    if (debugController.isStandalone() && debugController.isNotesActive())
    {
        debugController.buildStandaloneFrameData(out, highway.getSceneRenderer(), midiInterpreter,
                                                  displaySizeInPPQ.toDouble(), displayWindowTimeSeconds);
        out.scrollOffset = computeScrollOffset();
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

    // Apply latency offset (ms added to playhead time)
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
    TrackWindow ppqTrackWindow;
    SustainWindow ppqSustainWindow;
    {
#ifdef DEBUG
        ScopedPhaseMeasure lm(debugController.lockWait_us, highway.getSceneRenderer().collectPhaseTiming);
#endif
        ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
        ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    }

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

    out.trackWindow = timeTrackWindow;
    out.sustainWindow = timeSustainWindow;
    out.gridlines = timeGridlineMap;
    out.windowStartTime = 0.0;
    out.windowEndTime = displayWindowTimeSeconds;
    out.isPlaying = audioProcessor.isPlaying;
    out.scrollOffset = computeScrollOffset();
}

void ChartchoticAudioProcessorEditor::resized()
{
    state.setProperty("editorWidth", getWidth(), nullptr);
    state.setProperty("editorHeight", getHeight(), nullptr);

    // Virtual scene dimensions — maintain 4:3 internally, anchor strikeline to bottom
    sceneWidth = getWidth();
    sceneHeight = juce::roundToInt(sceneWidth / sceneAspectRatio);
    sceneOffsetY = getHeight() - sceneHeight;

    const int margin = 10;

    // Toolbar at top — scales with editor width
    int tbHeight = juce::roundToInt(getWidth() * ToolbarComponent::toolbarRatio);
    toolbar.setBounds(0, 0, getWidth(), tbHeight);

    // Highway component — fills the virtual 4:3 scene area, extended upward for overflow
    highway.renderWidth = sceneWidth;
    highway.renderHeight = sceneHeight;
    highway.updateOverflow();
    int hwOverflow = highway.getTopOverflow();
    highway.setBounds(0, sceneOffsetY - hwOverflow, sceneWidth, sceneHeight + hwOverflow);

    // Version label + update badge (bottom-left) — same scale as toolbar
    float s = (float)tbHeight / (float)ToolbarComponent::referenceHeight;

    int barH = juce::roundToInt(20.0f * s);
    int barMargin = juce::roundToInt(10.0f * s);
    int barY = getHeight() - barH - barMargin;

    // Font: use toolbar's font sizing approach
    versionLabel.setFont(Theme::getUIFont(Theme::fontSize));

    // Start after REAPER logo area
    int logoArea = juce::roundToInt(40.0f * s);
    int gap = juce::roundToInt(5.0f * s);
    int x = logoArea;

    // Badge to the LEFT of the version label
    if (updateBanner.hasUpdate())
    {
        int badgeSize = barH;
        updateBanner.setBounds(x, barY, badgeSize, badgeSize);
        x += badgeSize + gap;
    }

    int versionWidth = (int)versionLabel.getFont().getStringWidthFloat(versionLabel.getText()) + juce::roundToInt(12.0f * s);
    versionLabel.setBounds(x, barY, versionWidth, barH);

    #ifdef DEBUG
    int stripH = toolbar.getStripHeight();
    debugController.getClearButton().setBounds(margin, stripH + 4, 100, 20);
    debugController.getConsole().setBounds(margin, stripH + 28, getWidth() - (2 * margin), getHeight() - stripH - 38);
    #endif

}

void ChartchoticAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    // Scale by highway length so apparent note speed stays consistent across different lengths.
    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;
    double hwLength = (double)highway.getSceneRenderer().farFadeEnd;
    double hwSpeedScale = hwLength / (double)HWY_SPEED_REFERENCE;
    displayWindowTimeSeconds = 7.87 * hwSpeedScale / (double)noteSpeed;

    // PPQ window must cover the full visible highway (farFadeEnd * window time) at worst-case tempo.
    // At 300 BPM, 1 second = 5 quarter notes. Base window of 30 PPQ scaled by highway length.
    const double BASE_PPQ_WINDOW = 30.0;
    double hwScale = std::max(1.0, hwLength);
    displaySizeInPPQ = PPQ(BASE_PPQ_WINDOW * hwScale);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartchoticAudioProcessorEditor::loadState()
{
    // Migrate old redBackground bool to new background string
    if (state.hasProperty("redBackground") && !state.hasProperty("background"))
    {
        if ((bool)state["redBackground"] && backgroundNames.contains("background_red"))
            state.setProperty("background", "background_red", nullptr);
        state.removeProperty("redBackground", nullptr);
    }

    // Load saved background image
    {
        juce::String savedBg = state.getProperty("background").toString();
        loadBackground(savedBg);
    }

    // Default to gothic if no saved preference (first launch only)
    if (!state.hasProperty("highwayTexture") && highwayTextureNames.contains("kanaizo_gothic"))
        state.setProperty("highwayTexture", "kanaizo_gothic", nullptr);

    toolbar.loadState();

    // Restore render toggle flags (default ON if property missing from saved state)
    highway.getSceneRenderer().showGems = !state.hasProperty("showGems") || (bool)state["showGems"];
    highway.getSceneRenderer().showBars = !state.hasProperty("showBars") || (bool)state["showBars"];
    highway.getSceneRenderer().showSustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
    highway.getSceneRenderer().showLanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
    highway.getSceneRenderer().showGridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];
    highway.getSceneRenderer().showTrack = !state.hasProperty("showTrack") || (bool)state["showTrack"];
    highway.getSceneRenderer().showLaneSeparators = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
    highway.getSceneRenderer().showStrikeline = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
    highway.showHighway = !state.hasProperty("showHighway") || (bool)state["showHighway"];

    // Restore highway length (apply per-instrument scale)
    {
        float userLength = state.hasProperty("highwayLength")
            ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"])
            : FAR_FADE_DEFAULT;
        highway.getSceneRenderer().farFadeEnd = computeFarFadeEnd(userLength);
    }

    // Load highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty() && highwayTextureNames.contains(savedTexture))
        loadHighwayTexture(savedTexture);

    // Restore texture parameters
    if (state.hasProperty("textureScale"))
        highway.getTrackRenderer().textureScale = (float)state["textureScale"];
    if (state.hasProperty("textureOpacity"))
        highway.getTrackRenderer().textureOpacity = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);

    // Apply side-effects that listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int frId = (int)state["framerate"];
    // Migrate old 120/144 FPS options (ids 4,5) and unset (0) to Native (id 4)
    if (frId < 1 || frId > 4)
        frId = 4;
    targetFrameInterval = (frId == 1) ? 1.0 / 15.0 :
                          (frId == 2) ? 1.0 / 30.0 :
                          (frId == 3) ? 1.0 / 60.0 : 0.0;

    showFps = (bool)state.getProperty("showFps", false);

    updateDisplaySizeFromSpeedSlider();
    highway.rebuildTrack();
}

void ChartchoticAudioProcessorEditor::applyLatencySetting(int latencyValue)
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


juce::ComponentBoundsConstrainer* ChartchoticAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartchoticAudioProcessorEditor::parentSizeChanged()
{
    AudioProcessorEditor::parentSizeChanged();
}

void ChartchoticAudioProcessorEditor::scanBackgrounds()
{
#if JUCE_MAC
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chartchotic/backgrounds");
#elif JUCE_WINDOWS
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chartchotic/backgrounds");
#endif

    backgroundNames.clear();

    if (!backgroundDirectory.isDirectory())
        backgroundDirectory.createDirectory();

    auto files = backgroundDirectory.findChildFiles(juce::File::findFiles, false, "*.png;*.jpg;*.jpeg");
    files.sort();
    for (auto& f : files)
        backgroundNames.add(f.getFileNameWithoutExtension());

    toolbar.setBackgroundList(backgroundNames);
}

void ChartchoticAudioProcessorEditor::loadBackground(const juce::String& filename)
{
    if (filename.isEmpty())
    {
        backgroundImageCurrent = backgroundImageDefault;
        return;
    }

    // Try common extensions
    for (auto ext : { ".png", ".jpg", ".jpeg" })
    {
        auto file = backgroundDirectory.getChildFile(filename + ext);
        if (file.existsAsFile())
        {
            auto img = juce::ImageFileFormat::loadFrom(file);
            if (img.isValid())
            {
                backgroundImageCurrent = img;
                return;
            }
        }
    }

    // File not found, fall back to default
    backgroundImageCurrent = backgroundImageDefault;
}

void ChartchoticAudioProcessorEditor::scanHighwayTextures()
{
#if JUCE_MAC
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chartchotic/highways");
#elif JUCE_WINDOWS
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chartchotic/highways");
#endif

    highwayTextureNames.clear();

    if (!highwayTextureDirectory.isDirectory())
        highwayTextureDirectory.createDirectory();

    // Install bundled default textures if missing
    struct BundledTexture { const char* filename; const char* data; int size; };
    const BundledTexture bundled[] = {
        { "kanaizo_amber.png",             BinaryData::kanaizo_amber_png,             BinaryData::kanaizo_amber_pngSize },
        { "kanaizo_darkwood.png",          BinaryData::kanaizo_darkwood_png,          BinaryData::kanaizo_darkwood_pngSize },
        { "kanaizo_gothic.png",            BinaryData::kanaizo_gothic_png,            BinaryData::kanaizo_gothic_pngSize },
        { "kanaizo_ornate.png",            BinaryData::kanaizo_ornate_png,            BinaryData::kanaizo_ornate_pngSize },
        { "kanaizo_BlueCaustic.png",       BinaryData::kanaizo_BlueCaustic_png,       BinaryData::kanaizo_BlueCaustic_pngSize },
        { "kanaizo_BlueMagentaHex.png",    BinaryData::kanaizo_BlueMagentaHex_png,    BinaryData::kanaizo_BlueMagentaHex_pngSize },
        { "kanaizo_FretWood.png",          BinaryData::kanaizo_FretWood_png,          BinaryData::kanaizo_FretWood_pngSize },
        { "kanaizo_IcyMetal.png",          BinaryData::kanaizo_IcyMetal_png,          BinaryData::kanaizo_IcyMetal_pngSize },
        { "kanaizo_BrightRadiated.png",    BinaryData::kanaizo_BrightRadiated_png,    BinaryData::kanaizo_BrightRadiated_pngSize },
        { "kanaizo_ModernHueistic.png",    BinaryData::kanaizo_ModernHueistic_png,    BinaryData::kanaizo_ModernHueistic_pngSize },
        { "kanaizo_RedCaustic.png",        BinaryData::kanaizo_RedCaustic_png,        BinaryData::kanaizo_RedCaustic_pngSize },
        { "kanaizo_ScratchedRegular.png",  BinaryData::kanaizo_ScratchedRegular_png,  BinaryData::kanaizo_ScratchedRegular_pngSize },
    };
    for (auto& t : bundled)
    {
        auto file = highwayTextureDirectory.getChildFile(t.filename);
        if (!file.existsAsFile())
            file.replaceWithData(t.data, t.size);
    }

    auto files = highwayTextureDirectory.findChildFiles(juce::File::findFiles, false, "*.png");
    files.sort();
    for (auto& f : files)
        highwayTextureNames.add(f.getFileNameWithoutExtension());

    toolbar.setHighwayTextureList(highwayTextureNames);
}

void ChartchoticAudioProcessorEditor::loadHighwayTexture(const juce::String& filename)
{
    auto file = highwayTextureDirectory.getChildFile(filename + ".png");
    if (!file.existsAsFile())
    {
        highway.clearTexture();
        return;
    }

    auto img = juce::ImageFileFormat::loadFrom(file);
    if (img.isValid())
        highway.setTexture(img);
    else
        highway.clearTexture();
}

float ChartchoticAudioProcessorEditor::computeScrollOffset()
{
    // Get current playhead position in PPQ and BPM
    double ppq = 0.0;
    double bpm = 120.0;

#ifdef DEBUG
    {
        float debugOffset;
        if (debugController.computeScrollOffset(debugOffset, displayWindowTimeSeconds))
            return debugOffset;
    }
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

void ChartchoticAudioProcessorEditor::drawFpsOverlay(juce::Graphics& g)
{
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < FPS_RING_SIZE; ++i)
        if (fpsRing[i] > 0.0) { sum += fpsRing[i]; ++count; }
    double avgDelta = count > 0 ? sum / count : 0.0;
    double fps = avgDelta > 0.0 ? 1000000.0 / avgDelta : 0.0;

    float s = (float)getHeight() / (float)defaultHeight;
    int tbHeight = juce::roundToInt(getHeight() * ToolbarComponent::toolbarRatio);
    int pad = juce::roundToInt(8.0f * s);
    int textH = juce::roundToInt(16.0f * s);
    auto area = juce::Rectangle<int>(pad, tbHeight + pad, juce::roundToInt(80.0f * s), textH);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(area.toFloat().expanded(4.0f * s, 2.0f * s), 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(Theme::getUIFont(12.0f * s));
    g.drawText(juce::String(fps, 1) + " FPS", area, juce::Justification::centredLeft);
}

