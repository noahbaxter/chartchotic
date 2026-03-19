/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Midi/Pipelines/ReaperMidiPipeline.h"
#include "Midi/Processing/NoteProcessor.h"
#include "Midi/Utils/MidiConstants.h"
#include "Utils/TempoTimeSignatureEventHelper.h"

#ifdef DEBUG
  #define DEBUG_MEASURE_LOCK_WAIT auto _lm = debug.measureLockWait()
#else
  #define DEBUG_MEASURE_LOCK_WAIT
#endif

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
    // Create scratch renderer for shared track image cache
    cacheRenderer = std::make_unique<TrackRenderer>(state);

    // Pre-allocate all 4 highway slots (pooled, never destroyed)
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway = std::make_unique<HighwayComponent>(state, assetManager);
        slots[i].highway->setTrackImageCache(&trackImageCache);
        slots[i].highway->setVisible(false);
    }

    // Activate slot 0 as default
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;
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
    debug.onChartLoaded = [this](const DebugMidiFilePlayer::LoadedChart& chart) {
        rebuildSlots(chart);
    };
    debug.init(*this, audioProcessor, state, isStandalone);
#endif

    initAssets();
    initToolbarCallbacks();
    toolbar.setLatencyOffsetRange(CALIBRATION_MIN_MS, CALIBRATION_MAX_MS);

    // Wire and add all pooled highway slots to component tree (hidden by default).
    // In standalone debug mode, rebuildSlots may have already replaced
    // the default slot during init — just re-wire to be safe.
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway->onOverflowChanged = [this]() { resized(); };
        addChildComponent(*slots[i].highway);
    }
    // Make active slots visible
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->setVisible(true);
    addAndMakeVisible(toolbar);
    initBottomBar();

    loadState();
    resized();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartchoticAudioProcessorEditor::~ChartchoticAudioProcessorEditor()
{
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

    // Frame delta tracking (used by FPS overlay and debug profiler)
    double frameDelta_us_local = 0.0;
    {
        double now = juce::Time::getHighResolutionTicks()
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (lastFrameTimestamp > 0.0)
        {
            frameDelta_us_local = (now - lastFrameTimestamp) * 1000000.0;
            fpsRing[fpsRingIndex] = frameDelta_us_local;
            fpsRingIndex = (fpsRingIndex + 1) % FPS_RING_SIZE;
        }
        lastFrameTimestamp = now;
    }

#ifdef DEBUG
    debug.beginFrame(frameDelta_us_local);
    debug.onFrame(lastKnownPosition, lastPlayingState);
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

    // Build frame data and push to each active highway slot
#ifdef DEBUG
    auto dataBuildStart = std::chrono::high_resolution_clock::now();
#endif
    HighwayFrameData primaryFrameData;
    if (isReaperMode && activeSlotCount > 1)
    {
        buildReaperFrameDataBatched(primaryFrameData);
    }
    else
    {
        for (int i = 0; i < activeSlotCount; i++)
        {
            auto& slot = slots[i];
            HighwayFrameData frameData;
            if (isReaperMode)
                buildReaperFrameData(frameData, *slot.interpreter);
            else
                buildStandardFrameData(frameData, *slot.interpreter);
            if (i == 0)
                primaryFrameData = frameData;
            slot.highway->setFrameData(frameData);
            slot.highway->repaint();
        }
    }
#ifdef DEBUG
    double dataBuild_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - dataBuildStart).count();
    {
        SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
        Part part = activeSlotCount == 0 ? getPartFromState(state) : primaryHighway().getActivePart();
        int vpW = activeSlotCount == 0 ? 0 : primaryHighway().renderWidth;
        int vpH = activeSlotCount == 0 ? 0 : primaryHighway().renderHeight;
        debug.recordFrameData(primaryFrameData, dataBuild_us,
            activeSlotCount, part, skill, vpW, vpH, lastPlayingState);
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
        propagateToSlots("skillLevel", id);
        if (hasActiveSession)
        {
            enabledDifficulties = { (SkillLevel)id };
            toolbar.setEnabledDifficulties(enabledDifficulties);
            rebuildVisibleSlots();
        }
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
        if (activeSlotCount == 0) return;
        state.setProperty("part", id, nullptr);
        Part newPart = getPartFromState(state);
        primaryInterpreter().instrumentPart = newPart;
        slots[0].part = newPart;

        // Recompute farFadeEnd with new instrument scale (slider value unchanged)
        if (!PositionMath::bemaniMode)
        {
            float userLength = state.hasProperty("highwayLength")
                ? (float)state["highwayLength"] : FAR_FADE_DEFAULT;
            float scaledLength = computeFarFadeEnd(userLength);
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            // Cache already has both guitar and drums baked — if scale changed,
            // invalidate and rebake
            trackImageCache.invalidate();
            rebakeTrackCache();
        }
        updateDisplaySizeFromSpeedSlider();

        // onInstrumentChanged swaps overlay pointers to correct cache entry
        forEachHighway([](auto& hw) { hw.onInstrumentChanged(); });
#ifdef DEBUG
        toolbar.getTuningPanel().setDrums(isDrumLike(getPartFromState(state)));
#endif
        propagateToSlots("part", id);
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        propagateToSlots("drumType", id);
    };

    toolbar.onAutoHopoChanged = [this](bool enabled) {
        state.setProperty("autoHopo", enabled, nullptr);
        propagateToSlots("autoHopo", enabled);
    };
    toolbar.onHopoThresholdChanged = [this](int thresholdIndex) {
        state.setProperty("hopoThreshold", thresholdIndex, nullptr);
        propagateToSlots("hopoThreshold", thresholdIndex);
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
        propagateToSlots("hitIndicators", on);
    };

    toolbar.onStarPowerChanged = [this](bool on) {
        state.setProperty("starPower", on, nullptr);
        propagateToSlots("starPower", on);
    };

    toolbar.onKick2xChanged = [this](bool on) {
        state.setProperty("kick2x", on, nullptr);
        propagateToSlots("kick2x", on);
    };

    toolbar.onDiscoFlipChanged = [this](bool on) {
        state.setProperty("discoFlip", on, nullptr);
        propagateToSlots("discoFlip", on);
    };

    toolbar.onDynamicsChanged = [this](bool on) {
        state.setProperty("dynamics", on, nullptr);
        propagateToSlots("dynamics", on);
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
    };

    toolbar.onLatencyChanged = [this](int id) {
        state.setProperty("latency", id, nullptr);
        applyLatencySetting(id);
    };

    toolbar.onLatencyOffsetChanged = [this](int offsetMs) {
        bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
        if (isReaperMode)
            audioProcessor.invalidateReaperCache();
    };

    toolbar.onBackgroundChanged = [this](const juce::String& bgName) {
        state.setProperty("background", bgName, nullptr);
        loadBackground(bgName);
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
        {
            float scaledLength = computeFarFadeEnd(length);
            // Update farFadeEnd on all highways first (needed for rebake to use correct params)
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            trackImageCache.invalidate();
            rebakeTrackCache();
        }
        else
        {
            forEachHighway([this, length](auto& hw) { hw.setHighwayLength(computeFarFadeEnd(length)); });
        }
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
        trackImageCache.invalidate();
        resized();
        toolbar.resized();
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty())
            forAllHighways([](auto& hw) { hw.clearTexture(); });
        else
            loadHighwayTexture(textureName);
    };

    toolbar.onTextureScaleChanged = [this](float scale) {
        state.setProperty("textureScale", scale, nullptr);
        forAllHighways([scale](auto& hw) { hw.setTextureScale(scale); });
    };

    toolbar.onTextureOpacityChanged = [this](float opacity) {
        state.setProperty("textureOpacity", opacity, nullptr);
        forAllHighways([opacity](auto& hw) { hw.setTextureOpacity(opacity); });
    };

    toolbar.onOpenBackgroundFolder = [this]() {
        backgroundDirectory.revealToUser();
    };

    toolbar.onOpenTextureFolder = [this]() {
        highwayTextureDirectory.revealToUser();
    };

#ifdef DEBUG
    debug.wireCallbacks(toolbar, primaryHighway(),
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
    if (hasActiveSession && activeSlotCount == 0)
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
    {
        HighwayComponent* hwPtrs[MAX_PROFILED_HIGHWAYS] = {};
        int hwCount = std::min(activeSlotCount, (int)MAX_PROFILED_HIGHWAYS);
        for (int i = 0; i < hwCount; ++i)
            hwPtrs[i] = slots[i].highway.get();
        debug.paintOverChildren(g,
            activeSlotCount == 0 ? nullptr : &primaryHighway(),
            hwPtrs, hwCount, activeSlotCount > 0);
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

    // Resolve all 4 difficulties in one lock — extract the active one
    PartWindow partWindow;
    {
        DEBUG_MEASURE_LOCK_WAIT;
        partWindow = interpreter.resolveAllDifficulties(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    }
    SkillLevel activeSkill = (SkillLevel)((int)interpreter.getState().getProperty("skillLevel"));
    auto& diffWindow = partWindow.forSkill(activeSkill);
    TrackWindow& ppqTrackWindow = diffWindow.trackWindow;
    SustainWindow& ppqSustainWindow = diffWindow.sustainWindow;

    // Create a lambda that uses REAPER's timeline conversion (handles ALL tempo changes)
    auto& reaperProvider = audioProcessor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    // Get tempo/timesig map from MidiProcessor (make a copy to avoid holding lock)
    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(audioProcessor.getTempoLock());
        tempoTimeSigMap = audioProcessor.getTempoTimeSignatureMap();
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

void ChartchoticAudioProcessorEditor::buildReaperFrameDataBatched(HighwayFrameData& primaryOut)
{
    // === Phase A: Frame-level shared data (computed once) ===

    PPQ trackWindowStartPPQ = lastKnownPosition;

    auto* playHead = audioProcessor.getPlayHead();
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    if (playHead)
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            double bpm = positionInfo->getBpm().orFallback(120.0);
            double latencyOffsetSeconds = latencyOffsetMs / 1000.0;
            double latencyOffsetBeats = latencyOffsetSeconds * (bpm / 60.0);
            trackWindowStartPPQ = trackWindowStartPPQ - PPQ(latencyOffsetBeats);
        }
    }

    PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ;
    PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;
    PPQ cursorPPQ = trackWindowStartPPQ;

    auto& reaperProvider = audioProcessor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    // Tempo map — one lock acquisition
    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(audioProcessor.getTempoLock());
        tempoTimeSigMap = audioProcessor.getTempoTimeSignatureMap();
    }

    // Gridlines + event markers — computed once for all slots
    TimeBasedGridlineMap sharedGridlines = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

    TimeBasedEventMarkers sharedMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(
        tempoTimeSigMap, cursorPPQ, ppqToTime);

    float scrollOffset = computeScrollOffset();
    bool isPlaying = audioProcessor.isPlaying;

    // === Phase B: Group slots by lock pointer ===

    std::unordered_map<juce::CriticalSection*, std::vector<int>> lockGroups;
    for (int i = 0; i < activeSlotCount; i++)
        lockGroups[&slots[i].interpreter->noteStateMapLock].push_back(i);

    // === Phase C + D: Per group: extract once, resolve once, then per-slot distribute ===

    for (auto& [lockPtr, slotIndices] : lockGroups)
    {
        // Use first slot's interpreter to build Config (shared across group — only skillLevel differs)
        auto& firstInterp = *slots[slotIndices[0]].interpreter;

        TrackResolver::Config cfg;
        cfg.part = firstInterp.instrumentPart;
        cfg.autoHopo = (bool)firstInterp.getState().getProperty("autoHopo", false);
        cfg.dynamics = (bool)firstInterp.getState().getProperty("dynamics");
        cfg.proDrums = (DrumType)((int)firstInterp.getState().getProperty("drumType")) == DrumType::PRO;
        cfg.kick2x = (bool)firstInterp.getState().getProperty("kick2x");
        cfg.discoFlip = (bool)firstInterp.getState().getProperty("discoFlip");
        cfg.starPower = (bool)firstInterp.getState().getProperty("starPower");
        cfg.bemaniMode = PositionMath::bemaniMode;
        cfg.discoFlipState = firstInterp.getDiscoFlipState();

        int thresholdIndex = (int)firstInterp.getState().getProperty("hopoThreshold", 2);
        if (cfg.autoHopo)
        {
            switch (thresholdIndex)
            {
                case 0: cfg.hopoThreshold = MIDI_HOPO_SIXTEENTH;     break;
                case 1: cfg.hopoThreshold = MIDI_HOPO_SIXTEENTH_DOT; break;
                case 2: cfg.hopoThreshold = MIDI_HOPO_CLASSIC_170;   break;
                case 3: cfg.hopoThreshold = MIDI_HOPO_EIGHTH;        break;
            }
            cfg.hopoThreshold += MIDI_HOPO_THRESHOLD_BUFFER;
        }

        // ONE lock acquisition, ONE extract
        SharedWindow shared;
        {
            DEBUG_MEASURE_LOCK_WAIT;
            const juce::ScopedLock lock(*lockPtr);
            shared = TrackResolver::extract(firstInterp.noteStateMapArray,
                                            extendedStart, trackWindowEndPPQ, latencyBufferEnd,
                                            cfg.bemaniMode);
        }

        // ONE resolve → all 4 difficulties
        PartWindow partWindow = TrackResolver::resolve(shared, cfg);

        // Per-slot: pick difficulty, convert, populate
        for (int idx : slotIndices)
        {
            auto& slot = slots[idx];
            auto& interp = *slot.interpreter;

            SkillLevel activeSkill = (SkillLevel)((int)interp.getState().getProperty("skillLevel"));
            auto& diffWindow = partWindow.forSkill(activeSkill);

            TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(
                diffWindow.trackWindow, cursorPPQ, ppqToTime);
            TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(
                diffWindow.sustainWindow, cursorPPQ, ppqToTime);

            HighwayFrameData frameData;
            frameData.trackWindow = timeTrackWindow;
            frameData.sustainWindow = timeSustainWindow;
            frameData.gridlines = sharedGridlines;
            frameData.eventMarkers = sharedMarkers;
            frameData.windowStartTime = 0.0;
            frameData.windowEndTime = displayWindowTimeSeconds;
            frameData.isPlaying = isPlaying;
            frameData.scrollOffset = scrollOffset;

            // Disco flip indicator + region markers (per-slot: reads per-interpreter state)
            {
                const DiscoFlipState* flipState = interp.getDiscoFlipState();
                const auto& interpState = interp.getState();
                bool isProDrums = (int)interpState.getProperty("drumType") == 2;
                bool discoEnabled = (bool)interpState.getProperty("discoFlip");
                int midiDiff = (int)interpState.getProperty("skillLevel") - 1;
                frameData.discoFlipActive = flipState != nullptr && isProDrums && discoEnabled
                                            && flipState->isFlipped(trackWindowStartPPQ, midiDiff);

                if (flipState != nullptr && isProDrums && discoEnabled)
                {
                    double cursorTime = ppqToTime(cursorPPQ.toDouble());
                    for (const auto& r : flipState->getRegions(midiDiff))
                    {
                        double startTime = ppqToTime(r.start.toDouble()) - cursorTime;
                        double endTime   = ppqToTime(r.end.toDouble())   - cursorTime;
                        if (startTime > 0.0 || endTime > -displayWindowTimeSeconds)
                            frameData.flipRegions.push_back({startTime, endTime});
                    }
                }
            }

            if (idx == 0)
                primaryOut = frameData;
            slot.highway->setFrameData(frameData);
            slot.highway->repaint();
        }
    }
}

void ChartchoticAudioProcessorEditor::buildStandardFrameData(HighwayFrameData& out, MidiInterpreter& interpreter)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

#ifdef DEBUG
    if (debug.isStandalone() && debug.isNotesActive())
    {
        debug.buildStandaloneFrameData(out, primaryHighway().getSceneRenderer(), interpreter,
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

    // Resolve all 4 difficulties in one lock — extract the active one
    PartWindow partWindow;
    {
        DEBUG_MEASURE_LOCK_WAIT;
        partWindow = interpreter.resolveAllDifficulties(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    }
    SkillLevel activeSkill = (SkillLevel)((int)interpreter.getState().getProperty("skillLevel"));
    auto& diffWindow = partWindow.forSkill(activeSkill);
    TrackWindow& ppqTrackWindow = diffWindow.trackWindow;
    SustainWindow& ppqSustainWindow = diffWindow.sustainWindow;

    // Simple constant BPM conversion for non-REAPER mode
    auto ppqToTime = [currentBPM](double ppq) { return ppq * (60.0 / currentBPM); };

    // Create a simple tempo/timesig map for standard mode (default 120 BPM, 4/4)
    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(audioProcessor.getTempoLock());
        tempoTimeSigMap = audioProcessor.getTempoTimeSignatureMap();

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
    if (activeSlotCount > 0)
    {
        int contentW = getWidth();
        int contentH = getHeight() - tbHeight;
        int numSlots = activeSlotCount;

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

    // Rebake shared track cache when scene dimensions change or cache was externally invalidated
    if (trackImageCache.isDirty() ||
        sceneWidth != trackImageCache.getBakedWidth() ||
        sceneHeight != trackImageCache.getBakedHeight())
    {
        trackImageCache.invalidate();
        rebakeTrackCache();
    }

    #ifdef DEBUG
    int stripH = toolbar.getStripHeight();
    debug.getClearButton().setBounds(margin, stripH + 4, 100, 20);
    debug.getConsole().setBounds(margin, stripH + 28, getWidth() - (2 * margin), getHeight() - stripH - 38);
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
    double hwScale = activeSlotCount == 0 ? 1.0 : std::max(1.0, (double)primaryHighway().getSceneRenderer().farFadeEnd);
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

    // Restore texture parameters (all pooled highways, not just active)
    if (state.hasProperty("textureScale"))
    {
        float ts = (float)state["textureScale"];
        forAllHighways([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state.hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);
        forAllHighways([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
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
    trackImageCache.invalidate();
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

        if (!notes.empty())
        {
            const juce::ScopedLock lock(*data.noteStateMapLock);
            for (auto& map : *data.noteStateMapArray)
                map.clear();

            NoteProcessor noteProcessor;
            noteProcessor.processAllNotes(notes, *data.noteStateMapArray);
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
    updateVisibleSlots();
}

void ChartchoticAudioProcessorEditor::updateVisibleSlots()
{
    // Hide all pooled slots
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].active = false;
        slots[i].highway->setVisible(false);
    }

    if (!hasActiveSession || sessionSlotCache.empty())
    {
        // Fallback: single default slot using processor's shared note state
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.skillLevel = (SkillLevel)(int)state.getProperty("skillLevel");
        slot.active = true;
        slot.ownedState.reset();
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.noteStateMapArray = std::make_shared<NoteStateMapArray>();
        slot.noteStateMapLock = std::make_shared<juce::CriticalSection>();
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;

        toolbar.setMultiInstrumentMode(false);
        resized();
        loadState();
        return;
    }

    // Difficulty order for consistent grid layout (E/M/H/X = top-left → bottom-right)
    static const SkillLevel diffOrder[] = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                             SkillLevel::HARD, SkillLevel::EXPERT };

    // Assign slots from session cache (up to MAX_HIGHWAY_SLOTS)
    int slotIdx = 0;
    for (auto& cached : sessionSlotCache)
    {
        if (enabledParts.count(cached.part) == 0)
            continue;

        for (auto level : diffOrder)
        {
            if (enabledDifficulties.count(level) == 0)
                continue;
            if (slotIdx >= MAX_HIGHWAY_SLOTS)
                break;

            auto& slot = slots[slotIdx];
            slot.part = cached.part;
            slot.skillLevel = level;
            slot.active = true;

            // Per-slot state with baked skillLevel for interpreter
            slot.ownedState = std::make_unique<juce::ValueTree>(state.createCopy());
            slot.ownedState->setProperty("part", (int)cached.part, nullptr);
            slot.ownedState->setProperty("skillLevel", (int)level, nullptr);

            // All difficulties share the same raw note data
            slot.noteStateMapArray = cached.noteStateMapArray;
            slot.noteStateMapLock = cached.noteStateMapLock;

            slot.interpreter = std::make_unique<MidiInterpreter>(
                cached.part, *slot.ownedState, *slot.noteStateMapArray, *slot.noteStateMapLock);

            if (cached.discoFlipState)
                slot.interpreter->setDiscoFlipState(cached.discoFlipState);

            slot.highway->setActivePart(cached.part);
            slot.highway->setVisible(true);
            slotIdx++;
        }
    }

    activeSlotCount = slotIdx;

    // Multi-slot labels
    bool multiInst = enabledParts.size() > 1;
    bool multiDiff = enabledDifficulties.size() > 1;
    for (int i = 0; i < activeSlotCount; i++)
    {
        slots[i].highway->showPartLabel = multiInst;
        slots[i].highway->showDifficultyLabel = multiDiff;
        slots[i].highway->displaySkillLevel = slots[i].skillLevel;
    }
    toolbar.setMultiInstrumentMode(activeSlotCount > 1);

    // Layout only — no loadState, no rebuildTrack
    resized();

    // Apply visual settings directly (NOT via loadState which triggers disk I/O + full rebuild)
    applyVisualState();
}

void ChartchoticAudioProcessorEditor::applyVisualState()
{
    // Propagate render toggle flags from state to active highways
    bool gems = !state.hasProperty("showGems") || (bool)state["showGems"];
    bool bars = !state.hasProperty("showBars") || (bool)state["showBars"];
    bool sustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
    bool lanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
    bool gridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];
    bool track = !state.hasProperty("showTrack") || (bool)state["showTrack"];
    bool laneSeps = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
    bool strikeline = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
    bool hwy = !state.hasProperty("showHighway") || (bool)state["showHighway"];
    bool stretch = state.hasProperty("stretchToFill") && (bool)state["stretchToFill"];

    float userLength = state.hasProperty("highwayLength")
        ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"])
        : FAR_FADE_DEFAULT;
    float scaledLength = computeFarFadeEnd(userLength);

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
        hw.stretchToFill = stretch;
        hw.getSceneRenderer().farFadeEnd = scaledLength;
    });

    // Texture settings (all pooled highways, not just active)
    if (state.hasProperty("textureScale"))
    {
        float ts = (float)state["textureScale"];
        forAllHighways([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state.hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);
        forAllHighways([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
    }

    // Point overlays at cache — rebuildTrack handles this via the cache path
    forEachHighway([](auto& hw) { hw.rebuildTrack(); });
}

void ChartchoticAudioProcessorEditor::rebakeTrackCache()
{
    if (!trackImageCache.isDirty() || activeSlotCount == 0)
        return;

    // Always bake at full single-highway resolution (sceneWidth × sceneHeight),
    // NOT at per-slot dimensions which are smaller in multi-highway mode.
    int w = sceneWidth;
    int h = sceneHeight;
    if (w <= 0 || h <= 0) return;

    // Compute overflow at full resolution
    auto& sr = slots[0].highway->getSceneRenderer();
    auto farEdgeGuitar = PositionMath::getFretboardEdge(
        false, sr.farFadeEnd, w, h,
        PositionConstants::HIGHWAY_POS_START, sr.highwayPosEnd);
    auto farEdgeDrums = PositionMath::getFretboardEdge(
        true, sr.farFadeEnd, w, h,
        PositionConstants::HIGHWAY_POS_START, sr.highwayPosEnd);
    int overflow = std::max(0, (int)std::ceil(std::max(-farEdgeGuitar.centerY, -farEdgeDrums.centerY)));

    // Copy texture settings to scratch renderer
    auto& hw = *slots[0].highway;
    cacheRenderer->textureScale = hw.getTrackRenderer().textureScale;
    cacheRenderer->textureOpacity = hw.getTrackRenderer().textureOpacity;

    // Bake both guitar and drums at full single-highway resolution
    trackImageCache.rebake(*cacheRenderer, w, h, overflow,
        sr.farFadeEnd, sr.farFadeLen, sr.farFadeCurve, sr.highwayPosEnd,
        sr.guitarLaneCoordsLocal, (int)PositionConstants::GUITAR_LANE_COUNT,
        sr.drumLaneCoordsLocal, (int)PositionConstants::DRUM_LANE_COUNT);

    // All active highways now pull overlays from the fresh cache
    forEachHighway([](auto& hw) { hw.rebuildTrack(); });
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

    // Hide all pooled slots
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].active = false;
        slots[i].highway->setVisible(false);
    }

    // If no parts found, configure slot 0 as default
    if (chart.foundParts.empty())
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.ownedState.reset();
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;

        resized();
        loadState();
        return;
    }

    // Configure one slot per discovered part (up to MAX_HIGHWAY_SLOTS)
    int slotIdx = 0;
    for (Part part : chart.foundParts)
    {
        if (slotIdx >= MAX_HIGHWAY_SLOTS) break;

        auto& slot = slots[slotIdx];
        slot.part = part;
        slot.active = true;
        slot.ownedState.reset();

        slot.noteStateMapArray = std::make_shared<NoteStateMapArray>();
        slot.noteStateMapLock = std::make_shared<juce::CriticalSection>();

        // Process this instrument's notes into the slot's own NoteStateMapArray
        juce::String tName = trackNameForPart(part);
        auto notesIt = chart.trackNotes.find(tName);
        if (notesIt != chart.trackNotes.end())
        {
            const juce::ScopedLock lock(*slot.noteStateMapLock);
            for (auto& map : *slot.noteStateMapArray)
                map.clear();

            NoteProcessor noteProcessor;
            noteProcessor.processAllNotes(notesIt->second, *slot.noteStateMapArray);
        }

        slot.interpreter = std::make_unique<MidiInterpreter>(
            part, state, *slot.noteStateMapArray, *slot.noteStateMapLock);
        slot.highway->setActivePart(part);
        slot.highway->setVisible(true);
        slotIdx++;
    }

    activeSlotCount = slotIdx;

    // Multi-highway mode: show part labels and all toolbar options
    bool multiSlot = activeSlotCount > 1;
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->showPartLabel = multiSlot;
    toolbar.setMultiInstrumentMode(multiSlot);

    resized();
    loadState();
}
#endif

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
        forAllHighways([](auto& hw) { hw.clearTexture(); });
        return;
    }

    auto img = juce::ImageFileFormat::loadFrom(file);
    if (img.isValid())
        forAllHighways([&img](auto& hw) { hw.setTexture(img); });
    else
        forAllHighways([](auto& hw) { hw.clearTexture(); });
}

float ChartchoticAudioProcessorEditor::computeScrollOffset()
{
#ifdef DEBUG
    {
        float debugOffset;
        if (debug.computeScrollOffset(debugOffset, displayWindowTimeSeconds))
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

