/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Midi/Pipelines/ReaperMidiPipeline.h"
#include "Midi/Processing/NoteProcessor.h"
#include "Utils/TempoTimeSignatureEventHelper.h"

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
      toolbar(state),
      assetManager()
{
    // Create the default single highway slot
    {
        HighwaySlot slot;
        slot.part = getPartFromState(state);
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway = std::make_unique<HighwayComponent>(state, assetManager);
        slots.push_back(std::move(slot));
    }
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
    debugController.onChartLoaded = [this](const DebugMidiFilePlayer::LoadedChart& chart) {
        rebuildSlots(chart);
    };
    debugController.init(*this, audioProcessor, state, isStandalone);
    frameProfileLogger.start();
#endif

    initAssets();
    initToolbarCallbacks();
    toolbar.setLatencyOffsetRange(CALIBRATION_MIN_MS, CALIBRATION_MAX_MS);

    // Wire and add highway slots to component tree.
    // In standalone debug mode, rebuildSlots may have already replaced
    // the default slot during init — just re-wire to be safe.
    for (auto& slot : slots)
    {
        slot.highway->onOverflowChanged = [this]() { resized(); };
        addAndMakeVisible(*slot.highway);
    }
    addAndMakeVisible(toolbar);
    initBottomBar();

    loadState();
    resized();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartchoticAudioProcessorEditor::~ChartchoticAudioProcessorEditor()
{
#ifdef DEBUG
    frameProfileLogger.stop();
#endif
    setLookAndFeel(nullptr);

    // Clear static typeface refs so JUCE leak detector doesn't fire on shutdown.
    // Safe in plugin mode too — typefaces are lazily recreated on next use.
    Theme::clearTypefaces();
    ChartchoticLogo::clearTypefaces();
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

#ifdef DEBUG
    frameProfileLogger.beginFrame();
#endif

    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    toolbar.setReaperMode(isReaperMode);
    toolbar.updateVisibility();

    // Create InstrumentSession on first REAPER detection
    if (isReaperMode && !audioProcessor.getInstrumentSession())
    {
        audioProcessor.createInstrumentSession();
        if (auto* session = audioProcessor.getInstrumentSession())
            rebuildSlotsFromSession(*session);
    }

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

                    // Poll InstrumentSession for changes (re-fetch affected tracks)
                    if (auto* session = audioProcessor.getInstrumentSession())
                    {
                        if (session->pollForChanges())
                            rebuildSlotsFromSession(*session);
                    }
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

    // Build frame data and push to each highway slot
#ifdef DEBUG
    int debugTrackWinSize = 0, debugSustainWinSize = 0, debugGridCount = 0;
#endif
    for (auto& slot : slots)
    {
        HighwayFrameData frameData;
        if (isReaperMode)
            buildReaperFrameData(frameData, *slot.interpreter);
        else
            buildStandardFrameData(frameData, *slot.interpreter);
#ifdef DEBUG
        if (&slot == &slots[0])
        {
            debugTrackWinSize = (int)frameData.trackWindow.size();
            debugSustainWinSize = (int)frameData.sustainWindow.size();
            debugGridCount = (int)frameData.gridlines.size();
        }
#endif
        slot.highway->setFrameData(frameData);
        slot.highway->repaint();
    }

#ifdef DEBUG
    {
        SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
        Part part = slots.empty() ? getPartFromState(state) : primaryHighway().getActivePart();
        int vpW = slots.empty() ? 0 : primaryHighway().renderWidth;
        int vpH = slots.empty() ? 0 : primaryHighway().renderHeight;
        frameProfileLogger.recordFrameData(
            debugController.frameDelta_us,
            debugController.lockWait_us,
            debugTrackWinSize, debugSustainWinSize, debugGridCount,
            (int)slots.size(), part, skill, vpW, vpH, lastPlayingState);
    }
#endif

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
        // Session mode: rebuild slots with new difficulty
        if (hasActiveSession)
        {
            enabledDifficulties = { (SkillLevel)id };
            toolbar.setEnabledDifficulties(enabledDifficulties);
            rebuildVisibleSlots();
            return;
        }
        refreshNoteData();
    };

    toolbar.onDifficultyClicked = [this](SkillLevel skill, bool modifierHeld) {
        if (!hasActiveSession) return;

        if (modifierHeld)
        {
            if (enabledDifficulties.count(skill))
                enabledDifficulties.erase(skill);
            else
                enabledDifficulties.insert(skill);
        }
        else
        {
            enabledDifficulties.clear();
            enabledDifficulties.insert(skill);
        }

        if (enabledDifficulties.size() > 1)
            forceSingleInstrument();

        if (!enabledDifficulties.empty())
            state.setProperty("skillLevel", (int)*enabledDifficulties.begin(), nullptr);

        toolbar.setEnabledDifficulties(enabledDifficulties);
        rebuildVisibleSlots();
    };

    toolbar.onAllDifficultiesClicked = [this]() {
        if (!hasActiveSession) return;
        enabledDifficulties = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                SkillLevel::HARD, SkillLevel::EXPERT };

        forceSingleInstrument();

        toolbar.setEnabledDifficulties(enabledDifficulties);
        rebuildVisibleSlots();
    };

    toolbar.onInstrumentClicked = [this](Part part, bool modifierHeld) {
        if (!hasActiveSession) return;

        if (modifierHeld)
        {
            if (enabledParts.count(part))
                enabledParts.erase(part);
            else
                enabledParts.insert(part);
        }
        else
        {
            enabledParts.clear();
            enabledParts.insert(part);
        }

        if (enabledParts.size() > 1)
            forceSingleDifficulty();

        toolbar.setEnabledParts(enabledParts);
        rebuildVisibleSlots();
    };

    toolbar.onAllInstrumentsClicked = [this]() {
        if (!hasActiveSession) return;
        enabledParts.clear();
        for (auto p : discoveredParts)
            enabledParts.insert(p);

        forceSingleDifficulty();

        toolbar.setEnabledParts(enabledParts);
        rebuildVisibleSlots();
    };

    toolbar.onPartChanged = [this](int id) {
        if (slots.empty()) return;
        state.setProperty("part", id, nullptr);
        Part newPart = getPartFromState(state);
        primaryInterpreter().instrumentPart = newPart;
        slots[0].part = newPart;

        // Recompute farFadeEnd with new instrument scale (slider value unchanged)
        if (!PositionMath::bemaniMode)
        {
            float userLength = state.hasProperty("highwayLength")
                ? (float)state["highwayLength"] : FAR_FADE_DEFAULT;
            forEachHighway([this, userLength](auto& hw) { hw.setHighwayLength(computeFarFadeEnd(userLength)); });
        }
        updateDisplaySizeFromSpeedSlider();

        forEachHighway([](auto& hw) { hw.onInstrumentChanged(); });
#ifdef DEBUG
        toolbar.getTuningPanel().setDrums(isDrumLike(getPartFromState(state)));
#endif
        refreshNoteData();
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        refreshNoteData();
    };

    toolbar.onAutoHopoChanged = [this](bool enabled) {
        state.setProperty("autoHopo", enabled, nullptr);
        refreshNoteData();
    };
    toolbar.onHopoThresholdChanged = [this](int thresholdIndex) {
        state.setProperty("hopoThreshold", thresholdIndex, nullptr);
        refreshNoteData();
    };

    toolbar.onGemsChanged = [this](bool on) {
        state.setProperty("showGems", on, nullptr);
        forEachHighway([on](auto& hw) { hw.setShowGems(on); });
    };

    toolbar.onBarsChanged = [this](bool on) {
        state.setProperty("showBars", on, nullptr);
        forEachHighway([on](auto& hw) { hw.setShowBars(on); });
    };

    toolbar.onSustainsChanged = [this](bool on) {
        state.setProperty("showSustains", on, nullptr);
        forEachHighway([on](auto& hw) { hw.setShowSustains(on); });
    };

    toolbar.onLanesChanged = [this](bool on) {
        state.setProperty("showLanes", on, nullptr);
        forEachHighway([on](auto& hw) { hw.setShowLanes(on); });
    };

    toolbar.onGridlinesChanged = [this](bool on) {
        state.setProperty("showGridlines", on, nullptr);
        forEachHighway([on](auto& hw) { hw.setShowGridlines(on); });
    };

    toolbar.onHitIndicatorsChanged = [this](bool on) {
        state.setProperty("hitIndicators", on, nullptr);
        refreshNoteData();
    };

    toolbar.onStarPowerChanged = [this](bool on) {
        state.setProperty("starPower", on, nullptr);
        refreshNoteData();
    };

    toolbar.onKick2xChanged = [this](bool on) {
        state.setProperty("kick2x", on, nullptr);
        refreshNoteData();
    };

    toolbar.onDiscoFlipChanged = [this](bool on) {
        state.setProperty("discoFlip", on, nullptr);
        propagateToSlots("discoFlip", on);
    };

    toolbar.onDynamicsChanged = [this](bool on) {
        state.setProperty("dynamics", on, nullptr);
        refreshNoteData();
    };

    toolbar.onTrackChanged = [this](bool on) {
        state.setProperty("showTrack", on, nullptr);
        propagateToSlots("showTrack", on);
        forEachHighway([on](auto& hw) { hw.setShowTrack(on); });
    };

    toolbar.onLaneSeparatorsChanged = [this](bool on) {
        state.setProperty("showLaneSeparators", on, nullptr);
        propagateToSlots("showLaneSeparators", on);
        forEachHighway([on](auto& hw) { hw.setShowLaneSeparators(on); });
    };

    toolbar.onStrikelineChanged = [this](bool on) {
        state.setProperty("showStrikeline", on, nullptr);
        propagateToSlots("showStrikeline", on);
        forEachHighway([on](auto& hw) { hw.setShowStrikeline(on); });
    };

    toolbar.onHighwayChanged = [this](bool on) {
        state.setProperty("showHighway", on, nullptr);
        propagateToSlots("showHighway", on);
        forEachHighway([on](auto& hw) { hw.setShowHighway(on); });
    };

    toolbar.onViewModeChanged = [this](int viewModeId) {
        auto mode = static_cast<ChartchoticAudioProcessor::ReaperViewMode>(viewModeId);
        audioProcessor.setReaperViewMode(mode);
        // Destroy and recreate session with new discovery strategy
        audioProcessor.destroyInstrumentSession();
        audioProcessor.createInstrumentSession();
        if (auto* session = audioProcessor.getInstrumentSession())
            rebuildSlotsFromSession(*session);
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

    toolbar.onShowBackgroundChanged = [this](bool on) {
        showBackground = on;
        repaint();
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
        propagateToSlots("gemScale", scale);
        forEachHighway([scale](auto& hw) { hw.setGemScale(scale); });
    };

    toolbar.onBarScaleChanged = [this](float scale) {
        state.setProperty("barScale", scale, nullptr);
        propagateToSlots("barScale", scale);
        forEachHighway([scale](auto& hw) { hw.setBarScale(scale); });
    };

    toolbar.onHighwayLengthChanged = [this](float length) {
        state.setProperty("highwayLength", length, nullptr);
        if (!PositionMath::bemaniMode)
            forEachHighway([this, length](auto& hw) { hw.setHighwayLength(computeFarFadeEnd(length)); });
        updateDisplaySizeFromSpeedSlider();
    };

    toolbar.onStretchChanged = [this](bool on) {
        forEachHighway([on](auto& hw) { hw.stretchToFill = on; });
        state.setProperty("stretchToFill", on, nullptr);
        resized();
    };

    toolbar.onBemaniModeChanged = [this](bool on) {
        PositionMath::bemaniMode = on;
        PositionMath::bemaniHwyScale = 1.0f;
        state.setProperty("bemaniMode", on, nullptr);
        updateDisplaySizeFromSpeedSlider();
        resized();
        toolbar.resized();
        forEachHighway([](auto& hw) {
            hw.getTrackRenderer().invalidate();
            hw.rebuildTrack();
        });
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty())
            forEachHighway([](auto& hw) { hw.clearTexture(); });
        else
            loadHighwayTexture(textureName);
        repaint();
    };

    toolbar.onTextureScaleChanged = [this](float scale) {
        state.setProperty("textureScale", scale, nullptr);
        forEachHighway([scale](auto& hw) { hw.setTextureScale(scale); });
    };

    toolbar.onTextureOpacityChanged = [this](float opacity) {
        state.setProperty("textureOpacity", opacity, nullptr);
        forEachHighway([opacity](auto& hw) { hw.setTextureOpacity(opacity); });
    };

    toolbar.onOpenBackgroundFolder = [this]() {
        backgroundDirectory.revealToUser();
    };

    toolbar.onOpenTextureFolder = [this]() {
        highwayTextureDirectory.revealToUser();
    };

#ifdef DEBUG
    debugController.wireCallbacks(toolbar, primaryHighway(),
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
    g.fillAll(juce::Colours::black);

    if (showBackground)
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
    // "Nothing selected" placeholder
    if (hasActiveSession && slots.empty())
    {
        int tbHeight = toolbar.getHeight();
        auto area = getLocalBounds().withTrimmedTop(tbHeight);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(Theme::getUIFont(16.0f));
        juce::String msg = enabledParts.empty() ? "Select an instrument"
                         : enabledDifficulties.empty() ? "Select a difficulty"
                         : "No data";
        g.drawText(msg, area, juce::Justification::centred);
    }

    if (showFps)
        drawFpsOverlay(g);

#ifdef DEBUG
    if (!slots.empty() && debugController.showProfilerOverlay)
        debugController.drawProfilerOverlay(g, primaryHighway().getSceneRenderer());

    if (!slots.empty())
    {
        frameProfileLogger.recordPaintData(
            primaryHighway().getSceneRenderer().lastPhaseTiming,
            primaryHighway().debugTrackRender_us,
            debugController.textureRender_us,
            primaryHighway().debugHighwayPaint_us);
    }
    else
    {
        PhaseTiming empty;
        frameProfileLogger.recordPaintData(empty, 0.0, 0.0, 0.0);
    }
#endif
}

void ChartchoticAudioProcessorEditor::buildReaperFrameData(HighwayFrameData& out, MidiInterpreter& interpreter)
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

    // Wire disco flip state — prefer InstrumentSession (multi-track), fall back to pipeline (single-track)
    const DiscoFlipState* flipState = nullptr;
    auto* reaperPipeline = dynamic_cast<ReaperMidiPipeline*>(audioProcessor.getMidiPipeline());
    if (!audioProcessor.getInstrumentSession())
    {
        // Local mode without session: use pipeline's disco flip
        if (reaperPipeline)
            flipState = reaperPipeline->getDiscoFlipState();
        interpreter.setDiscoFlipState(flipState);
    }
    else
    {
        // Session mode: disco flip already set per-slot in rebuildSlotsFromSession
        flipState = interpreter.getDiscoFlipState();
    }

    // Generate PPQ-based windows from MidiInterpreter
    TrackWindow ppqTrackWindow;
    SustainWindow ppqSustainWindow;
    {
#ifdef DEBUG
        ScopedPhaseMeasure lm(debugController.lockWait_us, true);
#endif
        ppqTrackWindow = interpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
        ppqSustainWindow = interpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
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

    out.eventMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(
        tempoTimeSigMap, cursorPPQ, ppqToTime);

    out.trackWindow = timeTrackWindow;
    out.sustainWindow = timeSustainWindow;
    out.gridlines = timeGridlineMap;
    out.windowStartTime = 0.0;
    out.windowEndTime = displayWindowTimeSeconds;
    out.isPlaying = audioProcessor.isPlaying;
    out.scrollOffset = computeScrollOffset();

    // Disco flip indicator + region markers
    {
        const auto& interpState = interpreter.getState();
        bool isProDrums = (int)interpState.getProperty("drumType") == 2;
        bool discoEnabled = (bool)interpState.getProperty("discoFlip");
        int midiDiff = (int)interpState.getProperty("skillLevel") - 1;
        out.discoFlipActive = flipState != nullptr && isProDrums && discoEnabled
                              && flipState->isFlipped(trackWindowStartPPQ, midiDiff);

        // Convert flip region boundaries to time-relative for highway markers
        if (flipState != nullptr && isProDrums && discoEnabled)
        {
            double cursorTime = ppqToTime(cursorPPQ.toDouble());
            for (const auto& r : flipState->getRegions(midiDiff))
            {
                double startTime = ppqToTime(r.start.toDouble()) - cursorTime;
                double endTime   = ppqToTime(r.end.toDouble())   - cursorTime;
                if (startTime > 0.0 || endTime > -displayWindowTimeSeconds)
                    out.flipRegions.push_back({startTime, endTime});
            }
        }
    }
}

void ChartchoticAudioProcessorEditor::buildStandardFrameData(HighwayFrameData& out, MidiInterpreter& interpreter)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

#ifdef DEBUG
    if (debugController.isStandalone() && debugController.isNotesActive())
    {
        debugController.buildStandaloneFrameData(out, primaryHighway().getSceneRenderer(), interpreter,
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
        ScopedPhaseMeasure lm(debugController.lockWait_us, true);
#endif
        ppqTrackWindow = interpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
        ppqSustainWindow = interpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
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

    out.eventMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(
        tempoTimeSigMap, cursorPPQ, ppqToTime);

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

    // Virtual scene dimensions
    sceneWidth = getWidth();
    int tbHeight = juce::roundToInt(getWidth() * ToolbarComponent::toolbarRatio);
    if (PositionMath::bemaniMode)
    {
        // Bemani: minimum 1:2 aspect, but grow taller to fill available space
        int minH = sceneWidth * 2;
        int available = getHeight() - tbHeight;
        sceneHeight = std::max(minH, available);
    }
    else
    {
        sceneHeight = juce::roundToInt(sceneWidth / sceneAspectRatio);
    }

    const int margin = 10;

    // Toolbar at top — scales with editor width
    toolbar.setBounds(0, 0, getWidth(), tbHeight);

    // Layout highway slots below toolbar
    if (!slots.empty())
    {
        int contentW = getWidth();
        int contentH = getHeight() - tbHeight;
        int numSlots = (int)slots.size();

        if (numSlots == 1)
        {
            auto& hw = *slots[0].highway;
            hw.renderWidth = sceneWidth;
            hw.renderHeight = sceneHeight;
            hw.updateOverflow();
            hw.setBounds(0, tbHeight, contentW, contentH);
        }
        else
        {
            // Determine grid layout based on aspect ratio and slot count
            //   wide  (aspect > 1.5): single row
            //   tall  (aspect < 0.7): single column
            //   square-ish:           grid (2x2 for 3-4 slots, row for 2)
            float aspect = (float)contentW / (float)std::max(1, contentH);
            int cols, rows;

            if (numSlots <= 2)
            {
                if (aspect > 1.0f) { cols = numSlots; rows = 1; }
                else               { cols = 1; rows = numSlots; }
            }
            else
            {
                // 3-4 slots
                if (aspect > 1.5f)      { cols = numSlots; rows = 1; }
                else if (aspect < 0.7f) { cols = 1; rows = numSlots; }
                else                    { cols = 2; rows = (numSlots + 1) / 2; }
            }

            int pad = highwayGridPadding;
            for (int i = 0; i < numSlots; ++i)
            {
                auto& hw = *slots[i].highway;
                int col = i % cols;
                int row = i / cols;

                // Full cell size
                int cellW = contentW / cols;
                int cellH = contentH / rows;
                int cellX = col * cellW;
                int cellY = tbHeight + row * cellH;

                // Apply padding (half on each side, so adjacent slots share the gap)
                int padL = (col > 0)        ? pad / 2 : 0;
                int padR = (col < cols - 1) ? pad / 2 : 0;
                int padT = (row > 0)        ? pad / 2 : 0;
                int padB = (row < rows - 1) ? pad / 2 : 0;

                int slotX = cellX + padL;
                int slotY = cellY + padT;
                int slotW = cellW - padL - padR;
                int slotH = cellH - padT - padB;

                // Each slot gets its own scene dimensions based on its bounds
                if (PositionMath::bemaniMode)
                {
                    hw.renderWidth = slotW;
                    int minH = slotW * 2;
                    hw.renderHeight = std::max(minH, slotH);
                }
                else
                {
                    hw.renderWidth = slotW;
                    hw.renderHeight = juce::roundToInt(slotW / sceneAspectRatio);
                }

                hw.updateOverflow();
                hw.setBounds(slotX, slotY, slotW, slotH);
            }
        }
    }

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
    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;

    // Same formula for both modes; Bemani applies ratio so same speed value feels equivalent.
    // Flat viewport has no foreshortening, so it needs a longer time window to match
    // the readable note density of perspective mode.
    displayWindowTimeSeconds = 7.87 / (double)noteSpeed;
    if (PositionMath::bemaniMode)
        displayWindowTimeSeconds *= (double)NOTE_SPEED_BEMANI_RATIO;

    // PPQ window must cover the full visible highway (farFadeEnd * window time) at worst-case tempo.
    // At 300 BPM, 1 second = 5 quarter notes. Base window of 30 PPQ scaled by highway length.
    const double BASE_PPQ_WINDOW = 30.0;
    double hwScale = slots.empty() ? 1.0 : std::max(1.0, (double)primaryHighway().getSceneRenderer().farFadeEnd);
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

    // Background visibility (default off)
    showBackground = state.hasProperty("showBackground") && (bool)state["showBackground"];

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
    {
        bool gems = !state.hasProperty("showGems") || (bool)state["showGems"];
        bool bars = !state.hasProperty("showBars") || (bool)state["showBars"];
        bool sustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
        bool lanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
        bool gridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];
        bool track = !state.hasProperty("showTrack") || (bool)state["showTrack"];
        bool laneSeps = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
        bool strikeline = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
        bool hwy = !state.hasProperty("showHighway") || (bool)state["showHighway"];

        forEachHighway([=](auto& hw) {
            hw.getSceneRenderer().showGems = gems;
            hw.getSceneRenderer().showBars = bars;
            hw.getSceneRenderer().showSustains = sustains;
            hw.getSceneRenderer().showLanes = lanes;
            hw.getSceneRenderer().showGridlines = gridlines;
            hw.getSceneRenderer().showTrack = track;
            hw.getSceneRenderer().showLaneSeparators = laneSeps;
            hw.getSceneRenderer().showStrikeline = strikeline;
            hw.showHighway = hwy;
        });
    }

    // Restore highway length (apply per-instrument scale)
    {
        float userLength = state.hasProperty("highwayLength")
            ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"])
            : FAR_FADE_DEFAULT;
        float scaledLength = computeFarFadeEnd(userLength);
        forEachHighway([scaledLength](auto& hw) {
            hw.getSceneRenderer().farFadeEnd = scaledLength;
        });
        PositionMath::bemaniHwyScale = scaledLength;
    }

    // Load highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty() && highwayTextureNames.contains(savedTexture))
        loadHighwayTexture(savedTexture);

    // Restore texture parameters
    if (state.hasProperty("textureScale"))
    {
        float ts = (float)state["textureScale"];
        forEachHighway([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state.hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);
        forEachHighway([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
    }

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

    // Restore stretch mode
    {
        bool stretch = state.hasProperty("stretchToFill") && (bool)state["stretchToFill"];
        forEachHighway([stretch](auto& hw) { hw.stretchToFill = stretch; });
    }

    // Restore Bemani mode
    PositionMath::bemaniMode = state.hasProperty("bemaniMode") && (bool)state["bemaniMode"];
    PositionMath::bemaniHwyScale = 1.0f;

    updateDisplaySizeFromSpeedSlider();
    forEachHighway([](auto& hw) { hw.rebuildTrack(); });
}

void ChartchoticAudioProcessorEditor::rebuildSlotsFromSession(InstrumentSession& session)
{
    updateSessionData(session);
    rebuildVisibleSlots();
}

void ChartchoticAudioProcessorEditor::updateSessionData(InstrumentSession& session)
{
    sessionSlotCache.clear();
    discoveredParts.clear();
    hasActiveSession = !session.isEmpty();

    if (!hasActiveSession)
        return;

    const auto& tracks = session.getTracks();

    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        Part part = tracks[i].part;
        if (!isHighwayRenderable(part))
            continue;

        SessionSlotData data;
        data.part = part;
        data.trackIdx = i;

        const auto& notes = session.getNotes(i);

        // Process all 4 difficulties separately — each gets correct modifier context
        for (auto skill : { SkillLevel::EASY, SkillLevel::MEDIUM, SkillLevel::HARD, SkillLevel::EXPERT })
        {
            DifficultyData dd;

            if (!notes.empty())
            {
                const juce::ScopedLock lock(*dd.noteStateMapLock);
                for (auto& map : *dd.noteStateMapArray)
                    map.clear();

                juce::ValueTree tempState = state.createCopy();
                tempState.setProperty("part", (int)part, nullptr);
                tempState.setProperty("skillLevel", (int)skill, nullptr);

                NoteProcessor noteProcessor;
                noteProcessor.processModifierNotes(notes, *dd.noteStateMapArray,
                                                    *dd.noteStateMapLock, tempState);
                noteProcessor.processPlayableNotes(notes, *dd.noteStateMapArray,
                                                    *dd.noteStateMapLock, audioProcessor.getMidiProcessor(),
                                                    tempState, 120.0, 44100.0);
            }

            data.difficulties[skill] = std::move(dd);
        }

        const auto& discoFlip = session.getDiscoFlipState(i);
        data.discoFlipState = discoFlip.hasRegions() ? &discoFlip : nullptr;

        if (std::find(discoveredParts.begin(), discoveredParts.end(), part) == discoveredParts.end())
            discoveredParts.push_back(part);

        sessionSlotCache.push_back(std::move(data));
    }

    // Sort discovered parts: Guitar, guitar alts, Bass, Keys, Drums, Vocals
    std::sort(discoveredParts.begin(), discoveredParts.end(),
              [](Part a, Part b) { return getPartSortOrder(a) < getPartSortOrder(b); });

    // First discovery: enable all parts
    if (enabledParts.empty())
    {
        for (auto p : discoveredParts)
            enabledParts.insert(p);
    }
    else
    {
        // Remove enabled parts that are no longer discovered
        for (auto it = enabledParts.begin(); it != enabledParts.end(); )
        {
            if (std::find(discoveredParts.begin(), discoveredParts.end(), *it) == discoveredParts.end())
                it = enabledParts.erase(it);
            else
                ++it;
        }
    }

    // Update toolbar toggles
    // Default to current skill level if no difficulties enabled yet
    if (enabledDifficulties.empty())
    {
        SkillLevel current = (SkillLevel)((int)state.getProperty("skillLevel"));
        enabledDifficulties.insert(current);
    }

    // Enable multi-select mode for difficulty in any session mode
    toolbar.enableMultiDifficultyMode(true);

    toolbar.setDiscoveredParts(discoveredParts);
    toolbar.setEnabledParts(enabledParts);
    toolbar.setEnabledDifficulties(enabledDifficulties);
}

void ChartchoticAudioProcessorEditor::forceSingleInstrument()
{
    if (enabledParts.size() <= 1) return;
    Part first = discoveredParts.empty() ? *enabledParts.begin() : discoveredParts[0];
    for (auto p : discoveredParts)
        if (enabledParts.count(p)) { first = p; break; }
    enabledParts.clear();
    enabledParts.insert(first);
    toolbar.setEnabledParts(enabledParts);
}

void ChartchoticAudioProcessorEditor::forceSingleDifficulty()
{
    if (enabledDifficulties.size() <= 1) return;
    SkillLevel first = *enabledDifficulties.begin();
    enabledDifficulties.clear();
    enabledDifficulties.insert(first);
    toolbar.setEnabledDifficulties(enabledDifficulties);
    state.setProperty("skillLevel", (int)first, nullptr);
}

void ChartchoticAudioProcessorEditor::rebuildVisibleSlots()
{
    // Remove old highway components
    for (auto& slot : slots)
        removeChildComponent(slot.highway.get());
    slots.clear();

    if (!hasActiveSession || sessionSlotCache.empty())
    {
        // Fallback: single default slot
        HighwaySlot slot;
        slot.part = getPartFromState(state);
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway = std::make_unique<HighwayComponent>(state, assetManager);
        slot.highway->onOverflowChanged = [this]() { resized(); };
        slots.push_back(std::move(slot));

        for (auto& s : slots)
            addAndMakeVisible(*s.highway);
        resized();
        loadState();
        return;
    }

    // Difficulty order for consistent grid layout (E/M/H/X = top-left → bottom-right)
    static const SkillLevel diffOrder[] = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                             SkillLevel::HARD, SkillLevel::EXPERT };

    // Build one slot per (enabled instrument × enabled difficulty)
    for (auto& cached : sessionSlotCache)
    {
        if (enabledParts.count(cached.part) == 0)
            continue;

        for (auto level : diffOrder)
        {
            if (enabledDifficulties.count(level) == 0)
                continue;

            auto it = cached.difficulties.find(level);
            if (it == cached.difficulties.end()) continue;

            HighwaySlot slot;
            slot.part = cached.part;
            slot.skillLevel = level;

            // Per-slot state with baked skillLevel for interpreter
            slot.ownedState = std::make_unique<juce::ValueTree>(state.createCopy());
            slot.ownedState->setProperty("part", (int)cached.part, nullptr);
            slot.ownedState->setProperty("skillLevel", (int)level, nullptr);

            slot.noteStateMapArray = it->second.noteStateMapArray;
            slot.noteStateMapLock = it->second.noteStateMapLock;

            slot.interpreter = std::make_unique<MidiInterpreter>(
                cached.part, *slot.ownedState, *slot.noteStateMapArray, *slot.noteStateMapLock);

            if (cached.discoFlipState)
                slot.interpreter->setDiscoFlipState(cached.discoFlipState);

            slot.highway = std::make_unique<HighwayComponent>(*slot.ownedState, assetManager);
            slot.highway->setActivePart(cached.part);
            slot.highway->onOverflowChanged = [this]() { resized(); };
            slots.push_back(std::move(slot));
        }
    }

    // Multi-slot labels — show part labels when multiple instruments, difficulty when multiple difficulties
    bool multiInst = enabledParts.size() > 1;
    bool multiDiff = enabledDifficulties.size() > 1;
    for (auto& slot : slots)
    {
        slot.highway->showPartLabel = multiInst;
        slot.highway->showDifficultyLabel = multiDiff;
        slot.highway->displaySkillLevel = slot.skillLevel;
    }
    toolbar.setMultiInstrumentMode(slots.size() > 1);

    for (auto& slot : slots)
        addAndMakeVisible(*slot.highway);
    resized();
    loadState();
}

#ifdef DEBUG
void ChartchoticAudioProcessorEditor::rebuildSlots(const DebugMidiFilePlayer::LoadedChart& chart)
{
    // Map Part enum to MIDI track name
    auto trackNameForPart = [](Part p) -> juce::String {
        switch (p) {
            case Part::GUITAR:     return "PART GUITAR";
            case Part::BASS:       return "PART BASS";
            case Part::KEYS:       return "PART KEYS";
            case Part::DRUMS:      return "PART DRUMS";
            case Part::VOCALS:     return "PART VOCALS";
            default:               return "PART GUITAR";
        }
    };

    // Remove old highway components
    for (auto& slot : slots)
        removeChildComponent(slot.highway.get());
    slots.clear();

    // If no parts found, create a single default slot
    if (chart.foundParts.empty())
    {
        HighwaySlot slot;
        slot.part = getPartFromState(state);
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway = std::make_unique<HighwayComponent>(state, assetManager);
        slot.highway->onOverflowChanged = [this]() { resized(); };
        slots.push_back(std::move(slot));

        for (auto& s : slots)
            addAndMakeVisible(*s.highway);
        resized();
        loadState();
        return;
    }

    // Create one slot per discovered part, each with its own NoteStateMapArray
    for (Part part : chart.foundParts)
    {
        HighwaySlot slot;
        slot.part = part;

        // Process this instrument's notes into the slot's own NoteStateMapArray
        juce::String tName = trackNameForPart(part);
        auto notesIt = chart.trackNotes.find(tName);
        if (notesIt != chart.trackNotes.end())
        {
            const juce::ScopedLock lock(*slot.noteStateMapLock);
            for (auto& map : *slot.noteStateMapArray)
                map.clear();

            // Use a temporary state copy to avoid mutating the shared state (which fires listeners)
            juce::ValueTree tempState = state.createCopy();
            tempState.setProperty("part", (int)part, nullptr);

            NoteProcessor noteProcessor;
            noteProcessor.processModifierNotes(notesIt->second, *slot.noteStateMapArray,
                                                *slot.noteStateMapLock, tempState);
            noteProcessor.processPlayableNotes(notesIt->second, *slot.noteStateMapArray,
                                                *slot.noteStateMapLock, audioProcessor.getMidiProcessor(),
                                                tempState, chart.initialBPM, 44100.0);
        }

        slot.interpreter = std::make_unique<MidiInterpreter>(
            part, state, *slot.noteStateMapArray, *slot.noteStateMapLock);
        slot.highway = std::make_unique<HighwayComponent>(state, assetManager);
        slot.highway->setActivePart(part);
        slot.highway->onOverflowChanged = [this]() { resized(); };
        slots.push_back(std::move(slot));
    }

    // Multi-highway mode: show part labels and all toolbar options
    bool multiSlot = slots.size() > 1;
    forEachHighway([multiSlot](auto& hw) { hw.showPartLabel = multiSlot; });
    toolbar.setMultiInstrumentMode(multiSlot);

    // Add to component tree and size before loadState,
    // so rebuildTrack() has valid renderWidth/renderHeight
    for (auto& slot : slots)
        addAndMakeVisible(*slot.highway);
    resized();

    // Now apply visual state (triggers rebuildTrack with valid dimensions)
    loadState();
}
#endif

void ChartchoticAudioProcessorEditor::refreshNoteData()
{
#ifdef DEBUG
    // In standalone multi-slot mode, toggle changes need to reprocess per-slot note data
    if (debugController.isStandalone() && slots.size() > 1)
    {
        debugController.reloadCurrentChart();
        return;
    }
#endif

    // Session mode: reprocess all difficulties then rebuild slots
    if (hasActiveSession)
    {
        if (auto* session = audioProcessor.getInstrumentSession())
            updateSessionData(*session);
        rebuildVisibleSlots();
        return;
    }

    audioProcessor.refreshMidiDisplay();
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
        forEachHighway([](auto& hw) { hw.clearTexture(); });
        return;
    }

    auto img = juce::ImageFileFormat::loadFrom(file);
    if (img.isValid())
        forEachHighway([&img](auto& hw) { hw.setTexture(img); });
    else
        forEachHighway([](auto& hw) { hw.clearTexture(); });
}

float ChartchoticAudioProcessorEditor::computeScrollOffset()
{
#ifdef DEBUG
    {
        float debugOffset;
        if (debugController.computeScrollOffset(debugOffset, displayWindowTimeSeconds))
            return debugOffset;
    }
#endif

    // Use absolute time from the playhead — immune to BPM/time sig changes
    double absoluteTime = 0.0;
    if (auto* playHead = audioProcessor.getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            absoluteTime = positionInfo->getTimeInSeconds().orFallback(0.0);
    }

    // Apply latency offset to match note rendering position
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    absoluteTime -= latencyOffsetMs / 1000.0;

    // Raw continuous scroll value — consumers do their own modulo against tile size.
    // No wrapping here avoids double-modulo precision snaps.
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    return (float)(-absoluteTime * scrollRate);
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

