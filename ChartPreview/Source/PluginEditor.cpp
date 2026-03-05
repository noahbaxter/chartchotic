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
        toolbar.getDebugPanel().setDrums(isPart(state, Part::DRUMS));
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
    dbg.initDefaults(trackRenderer);

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

    dbg.onLayerChanged = [this](int layer, float scale, float x, float y) {
        bool isDrums = isPart(state, Part::DRUMS);
        auto* layers = isDrums ? trackRenderer.layersDrums : trackRenderer.layersGuitar;
        layers[layer] = {scale, x, y};
        rebuildFadedTrackImage();
    };

    dbg.onTileStepChanged = [this](float step) {
        trackRenderer.tileStep = step;
        rebuildFadedTrackImage();
    };

    dbg.onTileScaleStepChanged = [this](float step) {
        trackRenderer.tileScaleStep = step;
        rebuildFadedTrackImage();
    };

