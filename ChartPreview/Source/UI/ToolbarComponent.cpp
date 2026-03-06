#include "ToolbarComponent.h"

static const juce::StringArray framerateLabels = { "15 FPS", "30 FPS", "60 FPS", "Native" };
static const juce::StringArray latencyLabels = { "250ms", "500ms", "750ms", "1000ms", "1500ms" };

ToolbarComponent::ToolbarComponent(juce::ValueTree& state)
    : state(state)
{
    initTopBar();
    initChartPanel();
    initViewPanel();
    initSettingsPanel();
}

ToolbarComponent::~ToolbarComponent()
{
    chartButton.dismissPanel();
    viewButton.dismissPanel();
    settingsButton.dismissPanel();
}

//==============================================================================
// Top bar controls

void ToolbarComponent::initTopBar()
{
    // Title
    titleLabel.setText("CHARTCHOTIC", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::coral));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // Difficulty buttons: E M H X
    skillButtons.setItems({"E", "M", "H", "X"});
    skillButtons.onSelectionChanged = [this](int index) {
        if (onSkillChanged) onSkillChanged(index + 1); // 1-based ID
    };
    addAndMakeVisible(skillButtons);

    // Instrument buttons: Guitar Drums
    partButtons.setItems({"Guitar", "Drums"});
    partButtons.onSelectionChanged = [this](int index) {
        if (onPartChanged) onPartChanged(index + 1);
        updateVisibility();
        // Re-layout chart panel if open
        if (chartButton.isPanelVisible())
        {
            chartButton.dismissPanel();
            chartButton.showPanel();
        }
    };
    addAndMakeVisible(partButtons);

    // Note Speed stepper
    noteSpeedStepper.setDisplayValue("7");
    noteSpeedStepper.setLabelRatio(0.52f);
    noteSpeedStepper.onStep = [this](int delta) {
        noteSpeed = juce::jlimit(2, 20, noteSpeed + delta);
        noteSpeedStepper.setDisplayValue(juce::String(noteSpeed));
        if (onNoteSpeedChanged) onNoteSpeedChanged(noteSpeed);
    };
    addAndMakeVisible(noteSpeedStepper);
}

//==============================================================================
// Chart panel — Modifiers + Elements

void ToolbarComponent::initChartPanel()
{
    // --- Modifiers ---

    starPowerToggle.onClick = [this]() {
        if (onStarPowerChanged) onStarPowerChanged(starPowerToggle.getToggleState());
    };

    // Auto HOPO stepper (guitar only)
    autoHopoStepper.setDisplayValue(hopoModeLabels[0]);
    autoHopoStepper.onStep = [this](int delta) {
        int count = hopoModeLabels.size();
        autoHopoIndex = juce::jlimit(0, count - 1, autoHopoIndex + delta);
        autoHopoStepper.setDisplayValue(hopoModeLabels[autoHopoIndex]);
        if (onAutoHopoChanged) onAutoHopoChanged(autoHopoIndex + 1);
    };

    // Drum modifiers
    dynamicsToggle.onClick = [this]() {
        if (onDynamicsChanged) onDynamicsChanged(dynamicsToggle.getToggleState());
    };

    kick2xToggle.onClick = [this]() {
        if (onKick2xChanged) onKick2xChanged(kick2xToggle.getToggleState());
    };

    cymbalsToggle.onClick = [this]() {
        // Cymbals ON = Pro (id 2), OFF = Normal (id 1)
        if (onDrumTypeChanged) onDrumTypeChanged(cymbalsToggle.getToggleState() ? 2 : 1);
    };

    // --- Elements ---

    gemsToggle.onClick = [this]() {
        if (onGemsChanged) onGemsChanged(gemsToggle.getToggleState());
    };

    barsToggle.onClick = [this]() {
        if (onBarsChanged) onBarsChanged(barsToggle.getToggleState());
    };

    sustainsToggle.onClick = [this]() {
        if (onSustainsChanged) onSustainsChanged(sustainsToggle.getToggleState());
    };

    lanesToggle.onClick = [this]() {
        if (onLanesChanged) onLanesChanged(lanesToggle.getToggleState());
    };

    gridlinesToggle.onClick = [this]() {
        if (onGridlinesChanged) onGridlinesChanged(gridlinesToggle.getToggleState());
    };

    hitIndicatorsToggle.onClick = [this]() {
        if (onHitIndicatorsChanged) onHitIndicatorsChanged(hitIndicatorsToggle.getToggleState());
    };

    // Register all children
    chartButton.addPanelChild(&modifiersHeader);
    chartButton.addPanelChild(&starPowerToggle);
    chartButton.addPanelChild(&autoHopoStepper);
    chartButton.addPanelChild(&dynamicsToggle);
    chartButton.addPanelChild(&kick2xToggle);
    chartButton.addPanelChild(&cymbalsToggle);
    chartButton.addPanelChild(&elementsHeader);
    chartButton.addPanelChild(&gemsToggle);
    chartButton.addPanelChild(&barsToggle);
    chartButton.addPanelChild(&sustainsToggle);
    chartButton.addPanelChild(&lanesToggle);
    chartButton.addPanelChild(&gridlinesToggle);
    chartButton.addPanelChild(&hitIndicatorsToggle);
    chartButton.setPanelSize(200, 300);
    chartButton.onLayoutPanel = [this](juce::Component* panel) { layoutChartPanel(panel); };
    addAndMakeVisible(chartButton);
}

//==============================================================================
// View panel — Playback, Style, Performance

void ToolbarComponent::initViewPanel()
{
    // --- Playback ---

    highwayLengthStepper.setDisplayValue(juce::String(highwayLengthPct) + "%");
    highwayLengthStepper.onStep = [this](int delta) {
        highwayLengthPct = juce::jlimit(hwLenMinPct, hwLenMaxPct,
            highwayLengthPct + delta * hwLenStepPct);
        highwayLengthStepper.setDisplayValue(juce::String(highwayLengthPct) + "%");
        if (onHighwayLengthChanged) onHighwayLengthChanged(highwayLengthPct / 100.0f);
    };

    // --- Style ---

    backgroundStepper.setDisplayValue("Default");
    backgroundStepper.onStep = [this](int delta) {
        // Built-in options + folder entries
        // Index 0 = Default, 1+ = folder backgrounds
        int totalCount = 1 + backgroundNames.size(); // "Default" + folder entries
        backgroundIndex += delta;
        if (backgroundIndex < 0) backgroundIndex = totalCount - 1;
        if (backgroundIndex >= totalCount) backgroundIndex = 0;

        juce::String name = (backgroundIndex == 0) ? "Default" : backgroundNames[backgroundIndex - 1];
        backgroundStepper.setDisplayValue(name);

        if (onBackgroundChanged)
            onBackgroundChanged(backgroundIndex == 0 ? "" : backgroundNames[backgroundIndex - 1]);
    };

    gemScaleStepper.setDisplayValue("100%");
    gemScaleStepper.onStep = [this](int delta) {
        int count = (int)std::size(gemScaleValues);
        gemScaleIndex = juce::jlimit(0, count - 1, gemScaleIndex + delta);
        float scale = gemScaleValues[gemScaleIndex];
        gemScaleStepper.setDisplayValue(juce::String((int)(scale * 100)) + "%");
        if (onGemScaleChanged) onGemScaleChanged(scale);
    };

    highwayTextureStepper.setDisplayValue("None");
    highwayTextureStepper.onStep = [this](int delta) {
        int count = highwayTextureNames.size();
        if (count == 0) return;

        highwayTextureIndex += delta;
        if (highwayTextureIndex < -1) highwayTextureIndex = count - 1;
        if (highwayTextureIndex >= count) highwayTextureIndex = -1;

        juce::String name = (highwayTextureIndex < 0) ? "None" : highwayTextureNames[highwayTextureIndex];
        highwayTextureStepper.setDisplayValue(name);

        if (onHighwayTextureChanged)
            onHighwayTextureChanged(highwayTextureIndex < 0 ? "" : highwayTextureNames[highwayTextureIndex]);
    };

    textureScaleLabel.setText("Scale", juce::dontSendNotification);
    textureScaleLabel.setFont(juce::Font(Theme::fontSize));
    textureScaleLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::textDim));
    textureScaleLabel.setJustificationType(juce::Justification::centred);

    textureOpacityLabel.setText("Opacity", juce::dontSendNotification);
    textureOpacityLabel.setFont(juce::Font(Theme::fontSize));
    textureOpacityLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::textDim));
    textureOpacityLabel.setJustificationType(juce::Justification::centred);

    textureScaleStepper.setLabelRatio(0.0f);
    textureScaleStepper.setDisplayValue("100%");
    textureScaleStepper.onStep = [this](int delta) {
        int count = (int)std::size(texScaleValues);
        texScaleIndex = juce::jlimit(0, count - 1, texScaleIndex + delta);
        int pct = texScaleValues[texScaleIndex];
        textureScaleStepper.setDisplayValue(juce::String(pct) + "%");
        if (onTextureScaleChanged) onTextureScaleChanged(pct / 100.0f);
    };

    textureOpacityStepper.setLabelRatio(0.0f);
    textureOpacityStepper.setDisplayValue("45%");
    textureOpacityStepper.onStep = [this](int delta) {
        textureOpacityPct = juce::jlimit(5, 100, textureOpacityPct + delta * 5);
        textureOpacityStepper.setDisplayValue(juce::String(textureOpacityPct) + "%");
        if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityPct / 100.0f);
    };

    // --- Performance ---

    framerateStepper.setDisplayValue(framerateLabels[framerateIndex]);
    framerateStepper.onStep = [this](int delta) {
        int count = framerateLabels.size();
        framerateIndex = juce::jlimit(0, count - 1, framerateIndex + delta);
        framerateStepper.setDisplayValue(framerateLabels[framerateIndex]);
        if (onFramerateChanged) onFramerateChanged(framerateIndex + 1);
    };

    // Register all children
    viewButton.addPanelChild(&playbackHeader);
    viewButton.addPanelChild(&highwayLengthStepper);
    viewButton.addPanelChild(&styleHeader);
    viewButton.addPanelChild(&backgroundStepper);
    viewButton.addPanelChild(&gemScaleStepper);
    viewButton.addPanelChild(&highwayTextureStepper);
    viewButton.addPanelChild(&textureScaleLabel);
    viewButton.addPanelChild(&textureOpacityLabel);
    viewButton.addPanelChild(&textureScaleStepper);
    viewButton.addPanelChild(&textureOpacityStepper);
    viewButton.addPanelChild(&performanceHeader);
    viewButton.addPanelChild(&framerateStepper);
    viewButton.setPanelSize(200, 300);
    viewButton.onLayoutPanel = [this](juce::Component* panel) { layoutViewPanel(panel); };
    addAndMakeVisible(viewButton);
}

//==============================================================================
// Settings panel (gear) — Sync

void ToolbarComponent::initSettingsPanel()
{
    latencyStepper.setDisplayValue(latencyLabels[0]);
    latencyStepper.onStep = [this](int delta) {
        int count = latencyLabels.size();
        latencyIndex = juce::jlimit(0, count - 1, latencyIndex + delta);
        latencyStepper.setDisplayValue(latencyLabels[latencyIndex]);
        if (onLatencyChanged) onLatencyChanged(latencyIndex + 1);
    };

    latencyOffsetLabel.setText("Offset", juce::dontSendNotification);
    latencyOffsetLabel.setFont(juce::Font(Theme::fontSize));
    latencyOffsetLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::textDim));

    latencyOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    latencyOffsetSlider.setRange(0.0, 2000.0, 5.0);
    latencyOffsetSlider.setValue(0.0, juce::dontSendNotification);
    latencyOffsetSlider.setTextValueSuffix(" ms");
    latencyOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
    latencyOffsetSlider.onValueChange = [this]() {
        int offsetMs = (int)latencyOffsetSlider.getValue();
        state.setProperty("latencyOffsetMs", offsetMs, nullptr);
        if (onLatencyOffsetChanged) onLatencyOffsetChanged(offsetMs);
    };

    settingsButton.addPanelChild(&latencyStepper);
    settingsButton.addPanelChild(&latencyOffsetLabel);
    settingsButton.addPanelChild(&latencyOffsetSlider);
    settingsButton.setPanelSize(200, 100);
    settingsButton.onLayoutPanel = [this](juce::Component* panel) { layoutSettingsPanel(panel); };
    addAndMakeVisible(settingsButton);

    // Only one panel open at a time
    std::vector<PopupMenuButton*> group = { &chartButton, &viewButton, &settingsButton };
    chartButton.setExclusiveGroup(group);
    viewButton.setExclusiveGroup(group);
    settingsButton.setExclusiveGroup(group);

#ifdef DEBUG
    addAndMakeVisible(debugPanel.getButton());
    addAndMakeVisible(tuningPanel.getButton());
#endif
}

//==============================================================================
// Paint & Layout

void ToolbarComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(getLocalBounds());
}

void ToolbarComponent::resized()
{
    if (getHeight() == 0) return;
    float scale = (float)getHeight() / (float)referenceHeight;

    int h = juce::roundToInt(28.0f * scale);
    int gap = juce::roundToInt(6.0f * scale);
    int y = (getHeight() - h) / 2;

    int x = juce::roundToInt(6.0f * scale);

    // Title
    int titleW = juce::roundToInt(90.0f * scale);
    titleLabel.setFont(juce::Font(13.0f * scale, juce::Font::bold));
    titleLabel.setBounds(x, y, titleW, h);
    x += titleW + gap;

    // Difficulty: 4 segments, compact
    int skillW = juce::roundToInt(100.0f * scale);
    skillButtons.setBounds(x, y, skillW, h);
    x += skillW + gap;

    // Instrument: 2 segments
    int partW = juce::roundToInt(120.0f * scale);
    partButtons.setBounds(x, y, partW, h);
    x += partW + gap;

    // Note Speed stepper
    int speedW = juce::roundToInt(110.0f * scale);
    noteSpeedStepper.setBounds(x, y, speedW, h);

    // Right side: popup buttons
    int btnW = juce::roundToInt(56.0f * scale);
    int gearW = juce::roundToInt(36.0f * scale);
    int rx = getWidth() - juce::roundToInt(6.0f * scale);

    chartButton.setScale(scale);
    viewButton.setScale(scale);
    settingsButton.setScale(scale);

    rx -= gearW;
    settingsButton.setBounds(rx, y, gearW, h);
    rx -= (gap + btnW);
    viewButton.setBounds(rx, y, btnW, h);
    rx -= (gap + btnW);
    chartButton.setBounds(rx, y, btnW, h);

#ifdef DEBUG
    rx -= (gap + btnW);
    debugPanel.getButton().setBounds(rx, y, btnW, h);
    rx -= (gap + btnW);
    tuningPanel.getButton().setBounds(rx, y, btnW, h);
#endif
}

//==============================================================================
// State

void ToolbarComponent::loadState()
{
    // Difficulty (1-based → 0-based)
    int skill = (int)state["skillLevel"];
    if (skill >= 1 && skill <= 4)
        skillButtons.setSelectedIndex(skill - 1);

    // Part (1-based → 0-based)
    int part = (int)state["part"];
    if (part >= 1 && part <= 2)
        partButtons.setSelectedIndex(part - 1);

    // Note speed
    noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : 7;
    noteSpeedStepper.setDisplayValue(juce::String(noteSpeed));

    // Drum type → cymbals toggle (Pro = 2 = on)
    int drumType = (int)state["drumType"];
    cymbalsToggle.setToggleState(drumType == 2);

    // Auto HOPO (1-based → 0-based)
    int autoHopo = (int)state["autoHopo"];
    if (autoHopo >= 1 && autoHopo <= hopoModeLabels.size())
    {
        autoHopoIndex = autoHopo - 1;
        autoHopoStepper.setDisplayValue(hopoModeLabels[autoHopoIndex]);
    }

    // Toggles
    gemsToggle.setToggleState(!state.hasProperty("showGems") || (bool)state["showGems"]);
    barsToggle.setToggleState(!state.hasProperty("showBars") || (bool)state["showBars"]);
    sustainsToggle.setToggleState(!state.hasProperty("showSustains") || (bool)state["showSustains"]);
    lanesToggle.setToggleState(!state.hasProperty("showLanes") || (bool)state["showLanes"]);
    starPowerToggle.setToggleState(!state.hasProperty("starPower") || (bool)state["starPower"]);
    gridlinesToggle.setToggleState(!state.hasProperty("showGridlines") || (bool)state["showGridlines"]);
    hitIndicatorsToggle.setToggleState(!state.hasProperty("hitIndicators") || (bool)state["hitIndicators"]);
    kick2xToggle.setToggleState(!state.hasProperty("kick2x") || (bool)state["kick2x"]);
    dynamicsToggle.setToggleState(!state.hasProperty("dynamics") || (bool)state["dynamics"]);
    // Background
    juce::String savedBg = state.getProperty("background").toString();
    if (savedBg.isNotEmpty())
    {
        backgroundIndex = backgroundNames.indexOf(savedBg) + 1; // +1 because 0 = Default
        if (backgroundIndex < 0) backgroundIndex = 0;
        backgroundStepper.setDisplayValue(backgroundIndex == 0 ? "Default" : savedBg);
    }

    // Framerate (1-based → 0-based)
    int savedFramerate = (int)state["framerate"];
    if (savedFramerate < 1 || savedFramerate > 4) savedFramerate = 4;
    framerateIndex = savedFramerate - 1;
    framerateStepper.setDisplayValue(framerateLabels[framerateIndex]);

    // Latency (1-based → 0-based)
    int savedLatency = (int)state["latency"];
    if (savedLatency >= 1 && savedLatency <= latencyLabels.size())
    {
        latencyIndex = savedLatency - 1;
        latencyStepper.setDisplayValue(latencyLabels[latencyIndex]);
    }

    // Latency offset
    int latencyOffsetMs = (int)state["latencyOffsetMs"];
    latencyOffsetSlider.setValue(latencyOffsetMs, juce::dontSendNotification);

    // Gem scale
    float savedGemScale = state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f;
    gemScaleIndex = gemScaleDefault;
    for (int i = 0; i < (int)std::size(gemScaleValues); i++)
    {
        if (std::abs(gemScaleValues[i] - savedGemScale) < 0.01f)
        {
            gemScaleIndex = i;
            break;
        }
    }
    gemScaleStepper.setDisplayValue(juce::String((int)(gemScaleValues[gemScaleIndex] * 100)) + "%");

    // Highway length
    if (state.hasProperty("highwayLength"))
    {
        int savedPct = juce::roundToInt((float)state["highwayLength"] * 100.0f);
        highwayLengthPct = juce::jlimit(hwLenMinPct, hwLenMaxPct, savedPct);
        highwayLengthStepper.setDisplayValue(juce::String(highwayLengthPct) + "%");
    }

    // Texture scale
    if (state.hasProperty("textureScale"))
    {
        int savedPct = juce::roundToInt((float)state["textureScale"] * 100.0f);
        texScaleIndex = texScaleDefault;
        for (int i = 0; i < (int)std::size(texScaleValues); i++)
        {
            if (texScaleValues[i] == savedPct) { texScaleIndex = i; break; }
        }
        textureScaleStepper.setDisplayValue(juce::String(texScaleValues[texScaleIndex]) + "%");
    }

    // Texture opacity
    if (state.hasProperty("textureOpacity"))
    {
        textureOpacityPct = juce::jlimit(5, 100, juce::roundToInt((float)state["textureOpacity"] * 100.0f));
        textureOpacityStepper.setDisplayValue(juce::String(textureOpacityPct) + "%");
    }

    // Highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty())
    {
        highwayTextureIndex = highwayTextureNames.indexOf(savedTexture);
        highwayTextureStepper.setDisplayValue(highwayTextureIndex >= 0 ? savedTexture : "None");
    }

    updateVisibility();
}

void ToolbarComponent::updateVisibility()
{
    // No top-bar visibility changes needed anymore — drum type moved to chart panel
}

void ToolbarComponent::setReaperMode(bool isReaper)
{
    reaperMode = isReaper;
}

//==============================================================================
// Panel layouts

void ToolbarComponent::layoutChartPanel(juce::Component* panel)
{
    float s = chartButton.getScale();
    int margin = juce::roundToInt(10.0f * s);
    int pillH = juce::roundToInt(22.0f * s);
    int stepperH = juce::roundToInt(20.0f * s);
    int headerH = juce::roundToInt(16.0f * s);
    int gap = juce::roundToInt(4.0f * s);
    int sectionGap = juce::roundToInt(8.0f * s);
    int y = margin;
    int w = panel->getWidth() - margin * 2;
    int col2 = w / 2;
    int pillW = col2 - juce::roundToInt(2.0f * s);
    bool isDrums = isPart(state, Part::DRUMS);

    // --- Modifiers ---
    modifiersHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    starPowerToggle.setBounds(margin, y, pillW, pillH);

    if (isDrums)
    {
        cymbalsToggle.setBounds(margin + col2, y, pillW, pillH);
        cymbalsToggle.setVisible(true);
        y += pillH + gap;

        dynamicsToggle.setBounds(margin, y, pillW, pillH);
        dynamicsToggle.setVisible(true);
        kick2xToggle.setBounds(margin + col2, y, pillW, pillH);
        kick2xToggle.setVisible(true);
        y += pillH + gap;

        autoHopoStepper.setVisible(false);
    }
    else
    {
        cymbalsToggle.setVisible(false);
        dynamicsToggle.setVisible(false);
        kick2xToggle.setVisible(false);
        y += pillH + gap;

        autoHopoStepper.setBounds(margin, y, w, stepperH);
        autoHopoStepper.setVisible(true);
        y += stepperH + gap;
    }

    y += sectionGap - gap;

    // --- Elements ---
    elementsHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    gemsToggle.setBounds(margin, y, pillW, pillH);
    barsToggle.setBounds(margin + col2, y, pillW, pillH);
    y += pillH + gap;

    sustainsToggle.setBounds(margin, y, pillW, pillH);
    lanesToggle.setBounds(margin + col2, y, pillW, pillH);
    y += pillH + gap;

    gridlinesToggle.setBounds(margin, y, pillW, pillH);
    hitIndicatorsToggle.setBounds(margin + col2, y, pillW, pillH);
    y += pillH;

    panel->setSize(panel->getWidth(), y + margin);
}

void ToolbarComponent::layoutViewPanel(juce::Component* panel)
{
    float s = viewButton.getScale();
    int margin = juce::roundToInt(10.0f * s);
    int stepperH = juce::roundToInt(20.0f * s);
    int headerH = juce::roundToInt(16.0f * s);
    int gap = juce::roundToInt(4.0f * s);
    int sectionGap = juce::roundToInt(8.0f * s);
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    // --- Playback ---
    playbackHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    highwayLengthStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + sectionGap;

    // --- Style ---
    styleHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    backgroundStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    gemScaleStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    highwayTextureStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    // Scale + Opacity: labels above, value-only steppers below, two columns
    {
        int colW = (w - gap) / 2;
        int labelH = juce::roundToInt(14.0f * s);
        textureScaleLabel.setFont(juce::Font(Theme::fontSize * s));
        textureOpacityLabel.setFont(juce::Font(Theme::fontSize * s));
        textureScaleLabel.setBounds(margin, y, colW, labelH);
        textureOpacityLabel.setBounds(margin + colW + gap, y, colW, labelH);
        y += labelH + juce::roundToInt(1.0f * s);
        textureScaleStepper.setBounds(margin, y, colW, stepperH);
        textureOpacityStepper.setBounds(margin + colW + gap, y, colW, stepperH);
        y += stepperH + sectionGap;
    }

    // --- Performance ---
    performanceHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    framerateStepper.setBounds(margin, y, w, stepperH);
    y += stepperH;

    panel->setSize(panel->getWidth(), y + margin);
}

void ToolbarComponent::layoutSettingsPanel(juce::Component* panel)
{
    float s = settingsButton.getScale();
    int margin = juce::roundToInt(10.0f * s);
    int stepperH = juce::roundToInt(20.0f * s);
    int gap = juce::roundToInt(4.0f * s);
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    if (!reaperMode)
    {
        latencyStepper.setBounds(margin, y, w, stepperH);
        latencyStepper.setVisible(true);
        y += stepperH + gap;
    }
    else
    {
        latencyStepper.setVisible(false);
    }

    int labelH = juce::roundToInt(14.0f * s);
    int sliderH = juce::roundToInt(20.0f * s);
    latencyOffsetLabel.setFont(juce::Font(Theme::fontSize * s));
    latencyOffsetLabel.setBounds(margin, y, w, labelH);
    y += labelH + juce::roundToInt(2.0f * s);
    latencyOffsetSlider.setBounds(margin - 2, y, w + 4, sliderH);
    y += sliderH;

    panel->setSize(panel->getWidth(), y + margin);
}

void ToolbarComponent::setLatencyOffsetRange(int minMs, int maxMs)
{
    double current = latencyOffsetSlider.getValue();
    latencyOffsetSlider.setRange((double)minMs, (double)maxMs, 5.0);
    latencyOffsetSlider.setValue(juce::jlimit((double)minMs, (double)maxMs, current), juce::dontSendNotification);
}

#ifdef DEBUG
void ToolbarComponent::setDebugPlay(bool playing)
{
    debugPanel.setDebugPlay(playing);
}
#endif
