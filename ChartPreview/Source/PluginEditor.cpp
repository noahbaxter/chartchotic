/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UpdateChecker.h"

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

    latencyInSeconds = audioProcessor.latencyInSeconds;
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

    startTimerHz(60);
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
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
        int frameRate;
        switch (id)
        {
        case 1: frameRate = 15; break;
        case 2: frameRate = 30; break;
        case 3: frameRate = 60; break;
        case 4: frameRate = 120; break;
        case 5: frameRate = 144; break;
        default: frameRate = 60; break;
        }
        startTimerHz(frameRate);
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

#ifdef DEBUG
    toolbar.onDebugPlayChanged = [this](bool playing) {
        debugPlayActive = playing;
        if (playing)
        {
            debugPlayPPQ = 0.0;
            debugPlayLastTick = juce::Time::getHighResolutionTicks();
        }
    };

    toolbar.onDebugNotesChanged = [this](bool notes) {
        debugNotesActive = notes;
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
    if (debugPlayActive || debugNotesActive)
    {
        constexpr double debugBPM = 120.0;
        bool isDrums = isPart(state, Part::DRUMS);
        double patternLength = isDrums ? 56.0 : 72.0;

        PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
        PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

        TrackWindow ppqTrackWindow;
        SustainWindow ppqSustainWindow;

        // Only generate notes when the Notes toggle is on
        if (debugNotesActive)
        {
            double viewStart = extendedStart.toDouble();
            double viewEnd = trackWindowEndPPQ.toDouble() + 8.0; // 2 extra measures

            int firstCopy = (int)std::floor(viewStart / patternLength);
            int lastCopy = (int)std::floor(viewEnd / patternLength);

            for (int i = firstCopy; i <= lastCopy; ++i)
            {
                PPQ offset(i * patternLength);
                TrackWindow chunk = generateDebugChart(offset);
                ppqTrackWindow.insert(chunk.begin(), chunk.end());

                SustainWindow sustainChunk = generateDebugSustains(offset);
                ppqSustainWindow.insert(ppqSustainWindow.end(), sustainChunk.begin(), sustainChunk.end());
            }
        }

        auto ppqToTime = [debugBPM](double ppq) { return ppq * (60.0 / debugBPM); };

        TempoTimeSignatureMap tempoTimeSigMap;
        tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), debugBPM, 4, 4);

        PPQ cursorPPQ = trackWindowStartPPQ;
        TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
        TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
        TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
            tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

        double windowStartTime = 0.0;
        double windowEndTime = displayWindowTimeSeconds;

        highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, debugPlayActive);
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
    int fr = ((int)state["framerate"] == 1) ? 15 :
             ((int)state["framerate"] == 2) ? 30 :
             ((int)state["framerate"] == 3) ? 60 :
             ((int)state["framerate"] == 4) ? 120 :
             ((int)state["framerate"] == 5) ? 144 : 60;
    startTimerHz(fr);

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

#ifdef DEBUG

// Helper to make a sustain event
static SustainEvent makeSustain(PPQ start, PPQ end, uint col, SustainType type, Gem gem = Gem::NOTE, bool sp = false)
{
    SustainEvent s;
    s.startPPQ = start;
    s.endPPQ = end;
    s.gemColumn = col;
    s.sustainType = type;
    s.gemType = GemWrapper(gem, sp);
    return s;
}

TrackWindow ChartPreviewAudioProcessorEditor::generateDebugChart(PPQ startPPQ)
{
    TrackWindow tw;
    bool isDrums = isPart(state, Part::DRUMS);
    auto b = [&](double beat) { return startPPQ + PPQ(beat); };
    auto N  = [](Gem g = Gem::NOTE, bool sp = false) { return GemWrapper(g, sp); };
    auto _  = []() { return GemWrapper(); };

    // Helper: build a frame from column+gem pairs
    auto frame = [&](std::initializer_list<std::pair<uint, GemWrapper>> gems) {
        TrackFrame f = {_(), _(), _(), _(), _(), _(), _()};
        for (auto& [col, g] : gems) f[col] = g;
        return f;
    };

    if (isDrums)
    {
        // Drum lanes: 0=kick, 1=red/snare, 2=yellow, 3=blue, 4=green, 6=2xkick
        // Pads use NOTE/HOPO_GHOST/TAP_ACCENT; cymbals use CYM/CYM_GHOST/CYM_ACCENT

        // ── Individual pads (0-5) ──
        tw[b(0)] = frame({{0, N()}});                 // Kick
        tw[b(1)] = frame({{1, N()}});                 // Red (snare)
        tw[b(2)] = frame({{2, N()}});                 // Yellow tom
        tw[b(3)] = frame({{3, N()}});                 // Blue tom
        tw[b(4)] = frame({{4, N()}});                 // Green tom
        tw[b(5)] = frame({{6, N()}});                 // 2x Kick

        // ── Individual cymbals (6-8) ──
        tw[b(6)] = frame({{2, N(Gem::CYM)}});        // Yellow cymbal
        tw[b(7)] = frame({{3, N(Gem::CYM)}});        // Blue cymbal
        tw[b(8)] = frame({{4, N(Gem::CYM)}});        // Green cymbal

        // ── All pads — ghost / normal / accent (9-20) ──
        for (int i = 0; i < 4; ++i)
            tw[b(9 + i)]  = frame({{1, N(Gem::HOPO_GHOST)}, {2, N(Gem::HOPO_GHOST)}, {3, N(Gem::HOPO_GHOST)}, {4, N(Gem::HOPO_GHOST)}});
        for (int i = 0; i < 4; ++i)
            tw[b(13 + i)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}});
        for (int i = 0; i < 4; ++i)
            tw[b(17 + i)] = frame({{1, N(Gem::TAP_ACCENT)}, {2, N(Gem::TAP_ACCENT)}, {3, N(Gem::TAP_ACCENT)}, {4, N(Gem::TAP_ACCENT)}});

        // ── All cymbals — ghost / normal / accent (21-32) ──
        for (int i = 0; i < 4; ++i)
            tw[b(21 + i)] = frame({{2, N(Gem::CYM_GHOST)}, {3, N(Gem::CYM_GHOST)}, {4, N(Gem::CYM_GHOST)}});
        for (int i = 0; i < 4; ++i)
            tw[b(25 + i)] = frame({{2, N(Gem::CYM)}, {3, N(Gem::CYM)}, {4, N(Gem::CYM)}});
        for (int i = 0; i < 4; ++i)
            tw[b(29 + i)] = frame({{2, N(Gem::CYM_ACCENT)}, {3, N(Gem::CYM_ACCENT)}, {4, N(Gem::CYM_ACCENT)}});

        // ── Double bass 16ths (33-36) ──
        for (int i = 0; i < 16; ++i)
            tw[b(33.0 + i * 0.25)] = frame({{(uint)(i % 2 == 0 ? 0 : 6), N()}});

        // ── Snare lane with snare notes (37-42) ──
        for (int i = 0; i < 6; ++i)
            tw[b(37 + i)] = frame({{1, N()}});

        // ── Yellow cymbal swell with lane (43-48) ──
        tw[b(43)] = frame({{2, N(Gem::CYM)}});       // Quarter
        tw[b(44)] = frame({{2, N(Gem::CYM)}});
        tw[b(45)]   = frame({{2, N(Gem::CYM)}});     // Eighths
        tw[b(45.5)] = frame({{2, N(Gem::CYM)}});
        for (int i = 0; i < 6; ++i)                  // Triplets
            tw[b(46.0 + i * (1.0 / 3.0))] = frame({{2, N(Gem::CYM)}});
        for (int i = 0; i < 4; ++i)                  // Sixteenths
            tw[b(48.0 + i * 0.25)] = frame({{2, N(Gem::CYM)}});

        // ── Dual cymbal lane alternating blue+green (49-54) ──
        for (int i = 0; i < 12; ++i)
            tw[b(49.0 + i * 0.5)] = frame({{(uint)(i % 2 == 0 ? 3 : 4), N(Gem::CYM)}});
    }
    else
    {
        // Guitar lanes: 0=open, 1=green, 2=red, 3=yellow, 4=blue, 5=orange

        // ── Single notes (0-5) ──
        tw[b(0)] = frame({{0, N()}});                 // Open
        tw[b(1)] = frame({{1, N()}});                 // Green
        tw[b(2)] = frame({{2, N()}});                 // Red
        tw[b(3)] = frame({{3, N()}});                 // Yellow
        tw[b(4)] = frame({{4, N()}});                 // Blue
        tw[b(5)] = frame({{5, N()}});                 // Orange

        // ── All 2-note chords (6-15) ──
        tw[b(6)]  = frame({{1, N()}, {2, N()}});      // GR
        tw[b(7)]  = frame({{1, N()}, {3, N()}});      // GY
        tw[b(8)]  = frame({{1, N()}, {4, N()}});      // GB
        tw[b(9)]  = frame({{1, N()}, {5, N()}});      // GO
        tw[b(10)] = frame({{2, N()}, {3, N()}});      // RY
        tw[b(11)] = frame({{2, N()}, {4, N()}});      // RB
        tw[b(12)] = frame({{2, N()}, {5, N()}});      // RO
        tw[b(13)] = frame({{3, N()}, {4, N()}});      // YB
        tw[b(14)] = frame({{3, N()}, {5, N()}});      // YO
        tw[b(15)] = frame({{4, N()}, {5, N()}});      // BO

        // ── All 3-note chords (16-25) ──
        tw[b(16)] = frame({{1, N()}, {2, N()}, {3, N()}});       // GRY
        tw[b(17)] = frame({{1, N()}, {2, N()}, {4, N()}});       // GRB
        tw[b(18)] = frame({{1, N()}, {2, N()}, {5, N()}});       // GRO
        tw[b(19)] = frame({{1, N()}, {3, N()}, {4, N()}});       // GYB
        tw[b(20)] = frame({{1, N()}, {3, N()}, {5, N()}});       // GYO
        tw[b(21)] = frame({{1, N()}, {4, N()}, {5, N()}});       // GBO
        tw[b(22)] = frame({{2, N()}, {3, N()}, {4, N()}});       // RYB
        tw[b(23)] = frame({{2, N()}, {3, N()}, {5, N()}});       // RYO
        tw[b(24)] = frame({{2, N()}, {4, N()}, {5, N()}});       // RBO
        tw[b(25)] = frame({{3, N()}, {4, N()}, {5, N()}});       // YBO

        // ── All 4-note chords + 5-note chord (26-31) ──
        tw[b(26)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}});              // GRYB
        tw[b(27)] = frame({{1, N()}, {2, N()}, {3, N()}, {5, N()}});              // GRYO
        tw[b(28)] = frame({{1, N()}, {2, N()}, {4, N()}, {5, N()}});              // GRBO
        tw[b(29)] = frame({{1, N()}, {3, N()}, {4, N()}, {5, N()}});              // GYBO
        tw[b(30)] = frame({{2, N()}, {3, N()}, {4, N()}, {5, N()}});              // RYBO
        tw[b(31)] = frame({{1, N()}, {2, N()}, {3, N()}, {4, N()}, {5, N()}});    // GRYBO

        // ── Open note chords (32-36) ──
        tw[b(32)] = frame({{0, N()}, {1, N()}});      // Open+G
        tw[b(33)] = frame({{0, N()}, {2, N()}});      // Open+R
        tw[b(34)] = frame({{0, N()}, {3, N()}});      // Open+Y
        tw[b(35)] = frame({{0, N()}, {4, N()}});      // Open+B
        tw[b(36)] = frame({{0, N()}, {5, N()}});      // Open+O

        // ── Single sustains (37-48, 2 beats each) ──
        tw[b(37)] = frame({{0, N()}});                // Open sustain
        tw[b(39)] = frame({{1, N()}});                // Green sustain
        tw[b(41)] = frame({{2, N()}});                // Red sustain
        tw[b(43)] = frame({{3, N()}});                // Yellow sustain
        tw[b(45)] = frame({{4, N()}});                // Blue sustain
        tw[b(47)] = frame({{5, N()}});                // Orange sustain

        // ── HOPOs (49-53) ──
        tw[b(49)] = frame({{1, N(Gem::HOPO_GHOST)}});   // Green HOPO
        tw[b(50)] = frame({{2, N(Gem::HOPO_GHOST)}});   // Red HOPO
        tw[b(51)] = frame({{3, N(Gem::HOPO_GHOST)}});   // Yellow HOPO
        tw[b(52)] = frame({{4, N(Gem::HOPO_GHOST)}});   // Blue HOPO
        tw[b(53)] = frame({{5, N(Gem::HOPO_GHOST)}});   // Orange HOPO

        // ── Tap notes (54-58) ──
        tw[b(54)] = frame({{1, N(Gem::TAP_ACCENT)}});   // Green tap
        tw[b(55)] = frame({{2, N(Gem::TAP_ACCENT)}});   // Red tap
        tw[b(56)] = frame({{3, N(Gem::TAP_ACCENT)}});   // Yellow tap
        tw[b(57)] = frame({{4, N(Gem::TAP_ACCENT)}});   // Blue tap
        tw[b(58)] = frame({{5, N(Gem::TAP_ACCENT)}});   // Orange tap

        // ── Star Power (59-63) ──
        tw[b(59)] = frame({{1, N(Gem::NOTE, true)}});    // Green SP
        tw[b(60)] = frame({{2, N(Gem::NOTE, true)}});    // Red SP
        tw[b(61)] = frame({{3, N(Gem::NOTE, true)}});    // Yellow SP
        tw[b(62)] = frame({{4, N(Gem::NOTE, true)}});    // Blue SP
        tw[b(63)] = frame({{5, N(Gem::NOTE, true)}});    // Orange SP

        // ── Lanes with notes (64-71) ──
        tw[b(64)] = frame({{1, N()}});
        tw[b(65)] = frame({{2, N()}});
        tw[b(66)] = frame({{3, N()}});
        tw[b(67)] = frame({{4, N()}});
        tw[b(68)] = frame({{5, N()}});
        tw[b(69)] = frame({{3, N()}});
        tw[b(70)] = frame({{2, N()}});
        tw[b(71)] = frame({{1, N()}});
    }

    return tw;
}

SustainWindow ChartPreviewAudioProcessorEditor::generateDebugSustains(PPQ startPPQ)
{
    SustainWindow sw;
    bool isDrums = isPart(state, Part::DRUMS);
    auto b = [&](double beat) { return startPPQ + PPQ(beat); };

    if (isDrums)
    {
        // Snare lane (37-42)
        sw.push_back(makeSustain(b(37), b(42), 1, SustainType::LANE));
        // Yellow cymbal swell lane (43-48)
        sw.push_back(makeSustain(b(43), b(49), 2, SustainType::LANE));
        // Dual cymbal lane (49-54)
        sw.push_back(makeSustain(b(49), b(55), 3, SustainType::LANE));
        sw.push_back(makeSustain(b(49), b(55), 4, SustainType::LANE));
    }
    else
    {
        // Single sustains (37-48, 2 beats each)
        sw.push_back(makeSustain(b(37), b(39), 0, SustainType::SUSTAIN));  // Open
        sw.push_back(makeSustain(b(39), b(41), 1, SustainType::SUSTAIN));  // Green
        sw.push_back(makeSustain(b(41), b(43), 2, SustainType::SUSTAIN));  // Red
        sw.push_back(makeSustain(b(43), b(45), 3, SustainType::SUSTAIN));  // Yellow
        sw.push_back(makeSustain(b(45), b(47), 4, SustainType::SUSTAIN));  // Blue
        sw.push_back(makeSustain(b(47), b(49), 5, SustainType::SUSTAIN));  // Orange

        // Lane markers (64-71)
        sw.push_back(makeSustain(b(64), b(68), 1, SustainType::LANE));  // Green lane
        sw.push_back(makeSustain(b(64), b(68), 2, SustainType::LANE));  // Red lane
        sw.push_back(makeSustain(b(64), b(68), 3, SustainType::LANE));  // Yellow lane
        sw.push_back(makeSustain(b(68), b(71), 4, SustainType::LANE));  // Blue lane
        sw.push_back(makeSustain(b(68), b(71), 5, SustainType::LANE));  // Orange lane
    }

    return sw;
}
#endif
