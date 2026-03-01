/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UpdateChecker.h"
#ifdef DEBUG
#include "DebugTools/DebugChartGenerator.h"
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
      highwayRenderer(state, midiInterpreter),
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

    // Default to first highway texture if none saved
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isEmpty() && highwayTextureNames.size() > 0)
        loadHighwayTexture(highwayTextureNames[0]);
    else if (savedTexture.isNotEmpty())
        loadHighwayTexture(savedTexture);

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
        lastKnownPosition = debugController.getCurrentPPQ();
        if (debugController.isPlaying())
            lastPlayingState = true;
    }
#endif

    // Update highway texture scroll offset
    highwayRenderer.setScrollOffset(computeScrollOffset());

    repaint();
}

void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImageDefault = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    backgroundImageRed = juce::ImageCache::getFromMemory(BinaryData::background_red_jpg, BinaryData::background_red_jpgSize);
    trackDrumImage = juce::ImageCache::getFromMemory(BinaryData::track_drum_png, BinaryData::track_drum_pngSize);
    trackGuitarImage = juce::ImageCache::getFromMemory(BinaryData::track_guitar_png, BinaryData::track_guitar_pngSize);

    // Load REAPER logo SVG
    reaperLogo = juce::Drawable::createFromImageData(BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);

    // Scan highway textures
    scanHighwayTextures();
    toolbar.setHighwayTextureList(highwayTextureNames);

#ifdef DEBUG
    if (debugStandalone && highwayTextureNames.size() > 0)
    {
        auto& rng = juce::Random::getSystemRandom();
        auto name = highwayTextureNames[rng.nextInt(highwayTextureNames.size())];
        loadHighwayTexture(name);
    }
#endif
}

void ChartPreviewAudioProcessorEditor::scanHighwayTextures()
{
    // Look for highways in Application Support first, fall back to source assets dir for dev
#if JUCE_MAC
    highwayTextureDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chart Preview").getChildFile("highways");
    if (!highwayTextureDir.isDirectory())
        highwayTextureDir = juce::File("/Users/noahbaxter/Code/personal/plugins/chart-preview/ChartPreview/Assets/highways");
#elif JUCE_WINDOWS
    highwayTextureDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chart Preview").getChildFile("highways");
#endif

    highwayTextureNames.clear();
    if (highwayTextureDir.isDirectory())
    {
        auto files = highwayTextureDir.findChildFiles(juce::File::findFiles, false, "*.png");
        files.sort();
        for (const auto& f : files)
            highwayTextureNames.add(f.getFileNameWithoutExtension());
    }
}

void ChartPreviewAudioProcessorEditor::loadHighwayTexture(const juce::String& filename)
{
    if (filename.isEmpty())
    {
        highwayRenderer.clearHighwayTexture();
        state.setProperty("highwayTexture", "", nullptr);
        return;
    }

    auto file = highwayTextureDir.getChildFile(filename + ".png");
    if (file.existsAsFile())
    {
        auto img = juce::ImageFileFormat::loadFrom(file);
        if (img.isValid())
        {
            highwayRenderer.setHighwayTexture(img);
            state.setProperty("highwayTexture", filename, nullptr);
        }
    }
}

double ChartPreviewAudioProcessorEditor::computeScrollOffset()
{
    // Convert lastKnownPosition (PPQ) to absolute time in seconds
    double bpm = 120.0;

#ifdef DEBUG
    if (debugStandalone)
        bpm = debugController.getBPM();
    else
#endif
    if (auto* playHead = audioProcessor.getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            bpm = positionInfo->getBpm().orFallback(120.0);
    }

    double absoluteTimeSeconds = lastKnownPosition.toDouble() * (60.0 / bpm);

    // Scroll rate derived from note speed: one full texture tile per displayWindowTimeSeconds
    // Negate so the texture scrolls toward the player (matching note direction)
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    return std::fmod(-absoluteTimeSeconds * scrollRate, 1.0) + 1.0;
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
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        audioProcessor.refreshMidiDisplay();
    };

    toolbar.onAutoHopoChanged = [this](int id) {
        state.setProperty("autoHopo", id, nullptr);
        audioProcessor.refreshMidiDisplay();
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

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        loadHighwayTexture(textureName);
        repaint();
    };

    toolbar.onGemScaleChanged = [this](float scale) {
        state.setProperty("gemScale", scale, nullptr);
        repaint();
    };

#ifdef DEBUG
    toolbar.onDebugPlayChanged = [this](bool playing) {
        debugController.setPlaying(playing);
    };

    toolbar.onDebugBpmChanged = [this](int bpm) {
        debugController.setBPM((double)bpm);
    };

    toolbar.onDebugNotesChanged = [this](bool notes) {
        debugController.setNotesActive(notes);
    };

    toolbar.onDebugConsoleChanged = [this](bool show) {
        consoleOutput.setVisible(show);
        clearLogsButton.setVisible(show);
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

    // Draw the track
    if (isPart(state, Part::DRUMS))
    {
        g.drawImage(trackDrumImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }
    else if (isPart(state, Part::GUITAR))
    {
        g.drawImage(trackGuitarImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
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

    highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::paintStandardMode(juce::Graphics& g)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

#ifdef DEBUG
    if (debugStandalone && (debugController.isPlaying() || debugController.isNotesActive()))
    {
        bool isDrums = isPart(state, Part::DRUMS);
        double patternLength = isDrums ? 56.0 : 72.0;

        PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
        PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

        TrackWindow ppqTrackWindow;
        SustainWindow ppqSustainWindow;

        if (debugController.isNotesActive())
        {
            double viewStart = extendedStart.toDouble();
            double viewEnd = trackWindowEndPPQ.toDouble() + 8.0;

            int firstCopy = (int)std::floor(viewStart / patternLength);
            int lastCopy = (int)std::floor(viewEnd / patternLength);

            for (int i = firstCopy; i <= lastCopy; ++i)
            {
                PPQ offset(i * patternLength);
                TrackWindow chunk = DebugChartGenerator::generateDebugChart(offset, isDrums);
                ppqTrackWindow.insert(chunk.begin(), chunk.end());

                SustainWindow sustainChunk = DebugChartGenerator::generateDebugSustains(offset, isDrums);
                ppqSustainWindow.insert(ppqSustainWindow.end(), sustainChunk.begin(), sustainChunk.end());
            }
        }

        double bpm = debugController.getBPM();
        auto ppqToTime = [bpm](double ppq) { return ppq * (60.0 / bpm); };

        TempoTimeSignatureMap tempoTimeSigMap;
        tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), bpm, 4, 4);

        PPQ cursorPPQ = trackWindowStartPPQ;
        TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
        TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
        TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
            tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

        double windowStartTime = 0.0;
        double windowEndTime = displayWindowTimeSeconds;

        highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, debugController.isPlaying());
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

    highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
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
}

void ChartPreviewAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    displayWindowTimeSeconds = 7.87 / toolbar.getChartSpeedSlider().getValue();

    // Use a generous worst-case PPQ window for MIDI fetching to prevent pop-in at extreme tempos
    const double WORST_CASE_PPQ_WINDOW = 30.0;  // quarter notes
    displaySizeInPPQ = PPQ(WORST_CASE_PPQ_WINDOW);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartPreviewAudioProcessorEditor::loadState()
{
    toolbar.loadState();

    // Apply side-effects that listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int frId = (int)state["framerate"];
    targetFrameInterval = (frId == 1) ? 1.0 / 15.0 :
                          (frId == 2) ? 1.0 / 30.0 :
                          (frId == 3) ? 1.0 / 60.0 : 0.0;

    updateDisplaySizeFromSpeedSlider();
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
    // This method is called when the parent component size changes
    // We can use this to handle host-specific resize behavior if needed
    AudioProcessorEditor::parentSizeChanged();
}

