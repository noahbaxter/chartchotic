/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    bool isStandalone = juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
    debugController.init(*this, audioProcessor, state, isStandalone);
#endif

    initAssets();
    initToolbarCallbacks();
    initBottomBar();

    addAndMakeVisible(toolbar);

    loadState();
    resized();

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
    toolbar.setLatencyOffsetRange(isReaperMode ? -2000 : 0, 2000);
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
    {
        double now = juce::Time::getHighResolutionTicks()
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (lastRepaintTimestamp > 0.0)
            debugController.frameDelta_us = (now - lastRepaintTimestamp) * 1000000.0;
        lastRepaintTimestamp = now;
    }
    debugController.onFrame(lastKnownPosition, lastPlayingState);
#endif

    repaint();
}

void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImageDefault = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    backgroundImageCurrent = backgroundImageDefault;

    // Load REAPER logo SVG
    reaperLogo = juce::Drawable::createFromImageData(BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);

    scanBackgrounds();
    scanHighwayTextures();
}

void ChartPreviewAudioProcessorEditor::rebuildFadedTrackImage()
{
    bool isDrums = isPart(state, Part::DRUMS);
    const auto& fbw = isDrums ? sceneRenderer.fbWidthsDrums : sceneRenderer.fbWidthsGuitar;
    float wNear = fbw.near, wMid = fbw.mid, wFar = fbw.far;

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
        toolbar.getTuningPanel().setDrums(isPart(state, Part::DRUMS));
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

    toolbar.onGemsChanged = [this](bool on) {
        state.setProperty("showGems", on ? 1 : 0, nullptr);
        sceneRenderer.showGems = on;
        repaint();
    };

    toolbar.onBarsChanged = [this](bool on) {
        state.setProperty("showBars", on ? 1 : 0, nullptr);
        sceneRenderer.showBars = on;
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

    toolbar.onTrackChanged = [this](bool on) {
        state.setProperty("showTrack", on ? 1 : 0, nullptr);
        sceneRenderer.showTrack = on;
        repaint();
    };

    toolbar.onStrikelineChanged = [this](bool on) {
        state.setProperty("showStrikeline", on ? 1 : 0, nullptr);
        sceneRenderer.showStrikeline = on;
        repaint();
    };

    toolbar.onHighwayChanged = [this](bool on) {
        state.setProperty("showHighway", on ? 1 : 0, nullptr);
        showHighway = on;
        repaint();
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

    toolbar.onOpenBackgroundFolder = [this]() {
        backgroundDirectory.revealToUser();
    };

    toolbar.onOpenTextureFolder = [this]() {
        highwayTextureDirectory.revealToUser();
    };

#ifdef DEBUG
    debugController.wireCallbacks(toolbar, sceneRenderer, trackRenderer,
        [this]() { rebuildFadedTrackImage(); },
        [this]() { repaint(); });
#endif
}

void ChartPreviewAudioProcessorEditor::initBottomBar()
{
    // Version label (clickable when update available)
    versionLabel.setText(juce::String("v") + CHART_PREVIEW_VERSION, juce::dontSendNotification);
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

#ifdef DEBUG
    // Always show update banner in debug builds for testing
    updateBanner.setUpdateInfo("99.0.0", "https://github.com/noahbaxter/chart-preview/releases");
    resized();
#endif
}

//==============================================================================
void ChartPreviewAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImage(backgroundImageCurrent, getLocalBounds().toFloat());

    // Visual feedback for REAPER connection status
    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
    {
        // Draw REAPER logo in bottom left corner — scaled with editor
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

    // Draw highway (faded track background + scrolling texture)
    if (showHighway)
    {
        trackRenderer.paint(g);
#ifdef DEBUG
        ScopedPhaseMeasure m(debugController.textureRender_us, sceneRenderer.collectPhaseTiming);
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
        debugController.drawProfilerOverlay(g, sceneRenderer);
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
    TrackWindow ppqTrackWindow;
    SustainWindow ppqSustainWindow;
    {
#ifdef DEBUG
        ScopedPhaseMeasure lm(debugController.lockWait_us, sceneRenderer.collectPhaseTiming);
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
    if (debugController.isStandalone() && debugController.isNotesActive())
    {
        debugController.paintStandalone(g, sceneRenderer, midiInterpreter,
                                         displaySizeInPPQ.toDouble(), displayWindowTimeSeconds);
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
    TrackWindow ppqTrackWindow;
    SustainWindow ppqSustainWindow;
    {
#ifdef DEBUG
        ScopedPhaseMeasure lm(debugController.lockWait_us, sceneRenderer.collectPhaseTiming);
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

    // Use the constant time window from the slider (in seconds)
    // Window is anchored at the strikeline (time 0), extending forward into the future
    double windowStartTime = 0.0;
    double windowEndTime = displayWindowTimeSeconds;

    sceneRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::resized()
{
    const int margin = 10;

    // Toolbar at top — scales with editor height
    int tbHeight = juce::roundToInt(getHeight() * ToolbarComponent::toolbarRatio);
    toolbar.setBounds(0, 0, getWidth(), tbHeight);

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

    rebuildFadedTrackImage();
}

void ChartPreviewAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : 7;
    displayWindowTimeSeconds = 7.87 / (double)noteSpeed;

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

    // Default to random highway texture if no saved preference (before toolbar reads state)
    if (!state.hasProperty("highwayTexture") && highwayTextureNames.size() > 0)
    {
        auto rng = juce::Random();
        int idx = rng.nextInt(highwayTextureNames.size());
        state.setProperty("highwayTexture", highwayTextureNames[idx], nullptr);
    }

    toolbar.loadState();

    // Restore render toggle flags (default ON if property missing from saved state)
    sceneRenderer.showGems = !state.hasProperty("showGems") || (bool)state["showGems"];
    sceneRenderer.showBars = !state.hasProperty("showBars") || (bool)state["showBars"];
    sceneRenderer.showSustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
    sceneRenderer.showLanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
    sceneRenderer.showGridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];
    sceneRenderer.showTrack = !state.hasProperty("showTrack") || (bool)state["showTrack"];
    sceneRenderer.showStrikeline = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
    showHighway = !state.hasProperty("showHighway") || (bool)state["showHighway"];

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


juce::ComponentBoundsConstrainer* ChartPreviewAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartPreviewAudioProcessorEditor::parentSizeChanged()
{
    AudioProcessorEditor::parentSizeChanged();
}

void ChartPreviewAudioProcessorEditor::scanBackgrounds()
{
#if JUCE_MAC
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chart Preview/backgrounds");
#elif JUCE_WINDOWS
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chart Preview/backgrounds");
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

void ChartPreviewAudioProcessorEditor::loadBackground(const juce::String& filename)
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

