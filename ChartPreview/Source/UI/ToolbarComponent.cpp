#include "ToolbarComponent.h"

ToolbarComponent::ToolbarComponent(juce::ValueTree& state)
    : state(state)
{
    initControls();
}

ToolbarComponent::~ToolbarComponent()
{
    renderButton.dismissPanel();
    displayButton.dismissPanel();
    settingsButton.dismissPanel();
}

void ToolbarComponent::initControls()
{
    // Skill menu
    skillMenu.addItemList(skillLevelLabels, 1);
    skillMenu.onChange = [this]() {
        if (onSkillChanged) onSkillChanged(skillMenu.getSelectedId());
    };
    addAndMakeVisible(skillMenu);

    // Part menu
    partMenu.addItemList(partLabels, 1);
    partMenu.onChange = [this]() {
        if (onPartChanged) onPartChanged(partMenu.getSelectedId());
        updateVisibility();
        // Re-layout panels if open to show/hide drums-only toggles
        if (renderButton.isPanelVisible())
        {
            renderButton.dismissPanel();
            renderButton.showPanel();
        }
    };
    addAndMakeVisible(partMenu);

    // Drum type menu
    drumTypeMenu.addItemList(drumTypeLabels, 1);
    drumTypeMenu.onChange = [this]() {
        if (onDrumTypeChanged) onDrumTypeChanged(drumTypeMenu.getSelectedId());
    };
    addAndMakeVisible(drumTypeMenu);

    // Auto HOPO menu
    autoHopoMenu.addItemList(hopoModeLabels, 1);
    autoHopoMenu.onChange = [this]() {
        if (onAutoHopoChanged) onAutoHopoChanged(autoHopoMenu.getSelectedId());
    };
    addAndMakeVisible(autoHopoMenu);

    // Render popup children
    notesToggle.setButtonText("Notes");
    notesToggle.onClick = [this]() {
        if (onNotesChanged) onNotesChanged(notesToggle.getToggleState());
    };

    sustainsToggle.setButtonText("Sustains");
    sustainsToggle.onClick = [this]() {
        if (onSustainsChanged) onSustainsChanged(sustainsToggle.getToggleState());
    };

    lanesToggle.setButtonText("Lanes");
    lanesToggle.onClick = [this]() {
        if (onLanesChanged) onLanesChanged(lanesToggle.getToggleState());
    };

    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.onClick = [this]() {
        if (onStarPowerChanged) onStarPowerChanged(starPowerToggle.getToggleState());
    };

    gridlinesToggle.setButtonText("Gridlines");
    gridlinesToggle.onClick = [this]() {
        if (onGridlinesChanged) onGridlinesChanged(gridlinesToggle.getToggleState());
    };

    hitIndicatorsToggle.setButtonText("Hit Indicators");
    hitIndicatorsToggle.onClick = [this]() {
        if (onHitIndicatorsChanged) onHitIndicatorsChanged(hitIndicatorsToggle.getToggleState());
    };

    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.onClick = [this]() {
        if (onKick2xChanged) onKick2xChanged(kick2xToggle.getToggleState());
    };

    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.onClick = [this]() {
        if (onDynamicsChanged) onDynamicsChanged(dynamicsToggle.getToggleState());
    };

    // Render button
    renderButton.addPanelChild(&notesToggle);
    renderButton.addPanelChild(&sustainsToggle);
    renderButton.addPanelChild(&lanesToggle);
    renderButton.addPanelChild(&starPowerToggle);
    renderButton.addPanelChild(&gridlinesToggle);
    renderButton.addPanelChild(&hitIndicatorsToggle);
    renderButton.addPanelChild(&kick2xToggle);
    renderButton.addPanelChild(&dynamicsToggle);
    renderButton.setPanelSize(160, 120);
    renderButton.onLayoutPanel = [this](juce::Component* panel) { layoutRenderPanel(panel); };
    addAndMakeVisible(renderButton);

    // Display popup children
    redBackgroundToggle.setButtonText("Red Theme");
    redBackgroundToggle.onClick = [this]() {
        if (onRedBackgroundChanged) onRedBackgroundChanged(redBackgroundToggle.getToggleState());
    };

    // Highway texture label
    highwayTextureLabel.setText("Highway: None", juce::dontSendNotification);
    highwayTextureLabel.setJustificationType(juce::Justification::centredLeft);
    highwayTextureLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    highwayTextureLabel.setInterceptsMouseClicks(true, true);
    highwayTextureLabel.onScroll = [this](int delta) {
        int count = highwayTextureNames.size();
        if (count == 0) return;

        // Cycle: -1 (None) -> 0 -> 1 -> ... -> count-1 -> -1 (wraps)
        highwayTextureIndex += delta;
        if (highwayTextureIndex < -1) highwayTextureIndex = count - 1;
        if (highwayTextureIndex >= count) highwayTextureIndex = -1;

        juce::String name = (highwayTextureIndex < 0) ? "None" : highwayTextureNames[highwayTextureIndex];
        highwayTextureLabel.setText("Highway: " + name, juce::dontSendNotification);

        if (onHighwayTextureChanged)
            onHighwayTextureChanged(highwayTextureIndex < 0 ? "" : highwayTextureNames[highwayTextureIndex]);
    };

    // Texture scale label
    textureScaleLabel.setText("  Scale: 100%", juce::dontSendNotification);
    textureScaleLabel.setJustificationType(juce::Justification::centredLeft);
    textureScaleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    textureScaleLabel.setInterceptsMouseClicks(true, true);
    textureScaleLabel.onScroll = [this](int delta) {
        int count = (int)std::size(texScaleValues);
        texScaleIndex = juce::jlimit(0, count - 1, texScaleIndex - delta);
        int pct = texScaleValues[texScaleIndex];
        textureScaleLabel.setText("  Scale: " + juce::String(pct) + "%", juce::dontSendNotification);
        if (onTextureScaleChanged) onTextureScaleChanged(pct / 100.0f);
    };

    // Texture opacity label
    textureOpacityLabel.setText("  Opacity: 45%", juce::dontSendNotification);
    textureOpacityLabel.setJustificationType(juce::Justification::centredLeft);
    textureOpacityLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    textureOpacityLabel.setInterceptsMouseClicks(true, true);
    textureOpacityLabel.onScroll = [this](int delta) {
        textureOpacityPct = juce::jlimit(5, 100, textureOpacityPct - delta * 5);
        textureOpacityLabel.setText("  Opacity: " + juce::String(textureOpacityPct) + "%", juce::dontSendNotification);
        if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityPct / 100.0f);
    };

    // Gem scale label
    gemScaleLabel.setText("Gem Size: 100%", juce::dontSendNotification);
    gemScaleLabel.setJustificationType(juce::Justification::centredLeft);
    gemScaleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gemScaleLabel.setInterceptsMouseClicks(true, true);
    gemScaleLabel.onScroll = [this](int delta) {
        int count = (int)std::size(gemScaleValues);
        gemScaleIndex = juce::jlimit(0, count - 1, gemScaleIndex - delta);
        float scale = gemScaleValues[gemScaleIndex];
        gemScaleLabel.setText("Gem Size: " + juce::String((int)(scale * 100)) + "%", juce::dontSendNotification);
        if (onGemScaleChanged) onGemScaleChanged(scale);
    };

    // Highway length label
    highwayLengthLabel.setText("Hwy Length: " + juce::String(highwayLengthPct) + "%", juce::dontSendNotification);
    highwayLengthLabel.setJustificationType(juce::Justification::centredLeft);
    highwayLengthLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    highwayLengthLabel.setInterceptsMouseClicks(true, true);
    highwayLengthLabel.onScroll = [this](int delta) {
        highwayLengthPct = juce::jlimit(hwLenMinPct, hwLenMaxPct,
            highwayLengthPct + (-delta) * hwLenStepPct);
        highwayLengthLabel.setText("Hwy Length: " + juce::String(highwayLengthPct) + "%", juce::dontSendNotification);
        if (onHighwayLengthChanged) onHighwayLengthChanged(highwayLengthPct / 100.0f);
    };

    // Display button
    displayButton.addPanelChild(&redBackgroundToggle);
    displayButton.addPanelChild(&highwayTextureLabel);
    displayButton.addPanelChild(&textureScaleLabel);
    displayButton.addPanelChild(&textureOpacityLabel);
    displayButton.addPanelChild(&gemScaleLabel);
    displayButton.addPanelChild(&highwayLengthLabel);
    displayButton.setPanelSize(160, 172);
    displayButton.onLayoutPanel = [this](juce::Component* panel) { layoutDisplayPanel(panel); };
    addAndMakeVisible(displayButton);

    // Settings popup children
    chartSpeedSlider.setRange(2, 20, 1);
    chartSpeedSlider.setValue(7, juce::dontSendNotification);
    chartSpeedSlider.setSliderStyle(juce::Slider::LinearVertical);
    chartSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartSpeedSlider.onValueChange = [this]() {
        if (onNoteSpeedChanged) onNoteSpeedChanged((int)chartSpeedSlider.getValue());
    };

    chartSpeedLabel.setText("Note Speed", juce::dontSendNotification);
    chartSpeedLabel.setJustificationType(juce::Justification::centred);

    framerateMenu.addItemList({"15 FPS", "30 FPS", "60 FPS", "Native"}, 1);
    framerateMenu.onChange = [this]() {
        if (onFramerateChanged) onFramerateChanged(framerateMenu.getSelectedId());
    };

    latencyMenu.addItemList({"250ms", "500ms", "750ms", "1000ms", "1500ms"}, 1);
    latencyMenu.onChange = [this]() {
        if (onLatencyChanged) onLatencyChanged(latencyMenu.getSelectedId());
    };

    latencyOffsetLabel.setText("Offset (ms):", juce::dontSendNotification);
    latencyOffsetLabel.setJustificationType(juce::Justification::centred);

    latencyOffsetInput.setInputRestrictions(5, "-0123456789");
    latencyOffsetInput.setJustification(juce::Justification::centred);
    latencyOffsetInput.setText("0", false);
    latencyOffsetInput.setWantsKeyboardFocus(true);
    latencyOffsetInput.setSelectAllWhenFocused(true);
    latencyOffsetInput.onReturnKey = [this]() {
        if (onLatencyOffsetChanged)
            onLatencyOffsetChanged(latencyOffsetInput.getText().getIntValue());
        latencyOffsetInput.giveAwayKeyboardFocus();
    };
    latencyOffsetInput.onFocusLost = [this]() {
        if (onLatencyOffsetChanged)
            onLatencyOffsetChanged(latencyOffsetInput.getText().getIntValue());
    };
    latencyOffsetInput.onEscapeKey = [this]() {
        latencyOffsetInput.setText(juce::String((int)state["latencyOffsetMs"]), false);
        latencyOffsetInput.giveAwayKeyboardFocus();
    };

    // Settings button
    settingsButton.addPanelChild(&chartSpeedLabel);
    settingsButton.addPanelChild(&chartSpeedSlider);
    settingsButton.addPanelChild(&framerateMenu);
    settingsButton.addPanelChild(&latencyMenu);
    settingsButton.addPanelChild(&latencyOffsetLabel);
    settingsButton.addPanelChild(&latencyOffsetInput);
    settingsButton.setPanelSize(160, 310);
    settingsButton.onLayoutPanel = [this](juce::Component* panel) { layoutSettingsPanel(panel); };
    addAndMakeVisible(settingsButton);

#ifdef DEBUG
    addAndMakeVisible(debugPanel.getButton());
#endif
}

void ToolbarComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xCC111111));
    g.fillRect(getLocalBounds());
}

void ToolbarComponent::resized()
{
    const int controlHeight = 28;
    const int comboWidth = 90;
    const int gap = 6;

    int x = 6;
    int y = (getHeight() - controlHeight) / 2;

    skillMenu.setBounds(x, y, comboWidth, controlHeight);
    x += comboWidth + gap;

    partMenu.setBounds(x, y, comboWidth, controlHeight);
    x += comboWidth + gap;

    // Context control (only one visible at a time)
    drumTypeMenu.setBounds(x, y, comboWidth, controlHeight);
    autoHopoMenu.setBounds(x, y, comboWidth, controlHeight);

    // Right side: popup buttons
    const int btnWidth = 72;
    int rx = getWidth() - 6;

    rx -= btnWidth;
    settingsButton.setBounds(rx, y, btnWidth, controlHeight);
    rx -= (gap + btnWidth);
    displayButton.setBounds(rx, y, btnWidth, controlHeight);
    rx -= (gap + btnWidth);
    renderButton.setBounds(rx, y, btnWidth, controlHeight);

#ifdef DEBUG
    rx -= (gap + btnWidth);
    debugPanel.getButton().setBounds(rx, y, btnWidth, controlHeight);
#endif
}

void ToolbarComponent::loadState()
{
    skillMenu.setSelectedId((int)state["skillLevel"], juce::dontSendNotification);
    partMenu.setSelectedId((int)state["part"], juce::dontSendNotification);
    drumTypeMenu.setSelectedId((int)state["drumType"], juce::dontSendNotification);
    autoHopoMenu.setSelectedId((int)state["autoHopo"], juce::dontSendNotification);

    notesToggle.setToggleState(!state.hasProperty("showNotes") || (bool)state["showNotes"], juce::dontSendNotification);
    sustainsToggle.setToggleState(!state.hasProperty("showSustains") || (bool)state["showSustains"], juce::dontSendNotification);
    lanesToggle.setToggleState(!state.hasProperty("showLanes") || (bool)state["showLanes"], juce::dontSendNotification);
    starPowerToggle.setToggleState(!state.hasProperty("starPower") || (bool)state["starPower"], juce::dontSendNotification);
    gridlinesToggle.setToggleState(!state.hasProperty("showGridlines") || (bool)state["showGridlines"], juce::dontSendNotification);
    hitIndicatorsToggle.setToggleState(!state.hasProperty("hitIndicators") || (bool)state["hitIndicators"], juce::dontSendNotification);
    kick2xToggle.setToggleState(!state.hasProperty("kick2x") || (bool)state["kick2x"], juce::dontSendNotification);
    dynamicsToggle.setToggleState(!state.hasProperty("dynamics") || (bool)state["dynamics"], juce::dontSendNotification);
    redBackgroundToggle.setToggleState((bool)state["redBackground"], juce::dontSendNotification);

    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : 7;
    chartSpeedSlider.setValue(noteSpeed, juce::dontSendNotification);

    int savedFramerate = (int)state["framerate"];
    // Migrate old 120/144 FPS options (ids 4,5) and unset (0) to Native (id 4)
    if (savedFramerate < 1 || savedFramerate > 4)
        savedFramerate = 4;
    framerateMenu.setSelectedId(savedFramerate, juce::dontSendNotification);
    latencyMenu.setSelectedId((int)state["latency"], juce::dontSendNotification);

    int latencyOffsetMs = (int)state["latencyOffsetMs"];
    latencyOffsetInput.setText(juce::String(latencyOffsetMs), false);

    // Restore gem scale selection
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
    gemScaleLabel.setText("Gem Size: " + juce::String((int)(gemScaleValues[gemScaleIndex] * 100)) + "%", juce::dontSendNotification);

    // Restore highway length
    if (state.hasProperty("highwayLength"))
    {
        int savedPct = juce::roundToInt((float)state["highwayLength"] * 100.0f);
        highwayLengthPct = juce::jlimit(hwLenMinPct, hwLenMaxPct, savedPct);
        highwayLengthLabel.setText("Hwy Length: " + juce::String(highwayLengthPct) + "%", juce::dontSendNotification);
    }

    // Restore texture scale
    if (state.hasProperty("textureScale"))
    {
        int savedPct = juce::roundToInt((float)state["textureScale"] * 100.0f);
        texScaleIndex = texScaleDefault;
        for (int i = 0; i < (int)std::size(texScaleValues); i++)
        {
            if (texScaleValues[i] == savedPct) { texScaleIndex = i; break; }
        }
        textureScaleLabel.setText("  Scale: " + juce::String(texScaleValues[texScaleIndex]) + "%", juce::dontSendNotification);
    }

    // Restore texture opacity
    if (state.hasProperty("textureOpacity"))
    {
        textureOpacityPct = juce::jlimit(5, 100, juce::roundToInt((float)state["textureOpacity"] * 100.0f));
        textureOpacityLabel.setText("  Opacity: " + juce::String(textureOpacityPct) + "%", juce::dontSendNotification);
    }

    // Restore highway texture selection
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty())
    {
        highwayTextureIndex = highwayTextureNames.indexOf(savedTexture);
        if (highwayTextureIndex >= 0)
            highwayTextureLabel.setText("Highway: " + savedTexture, juce::dontSendNotification);
        else
            highwayTextureLabel.setText("Highway: None", juce::dontSendNotification);
    }

    updateVisibility();
}

void ToolbarComponent::updateVisibility()
{
    bool isDrums = isPart(state, Part::DRUMS);
    drumTypeMenu.setVisible(isDrums);
    autoHopoMenu.setVisible(!isDrums);
}

void ToolbarComponent::setReaperMode(bool isReaper)
{
    reaperMode = isReaper;
}

void ToolbarComponent::layoutRenderPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    int y = margin;
    int w = panel->getWidth() - margin * 2;
    bool isDrums = isPart(state, Part::DRUMS);

    notesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    sustainsToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    lanesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    starPowerToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    gridlinesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    hitIndicatorsToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    kick2xToggle.setBounds(margin, y, w, rowHeight);
    kick2xToggle.setVisible(isDrums);
    y += isDrums ? (rowHeight + gap) : 0;

    dynamicsToggle.setBounds(margin, y, w, rowHeight);
    dynamicsToggle.setVisible(isDrums);
    y += isDrums ? rowHeight : 0;

    int totalHeight = y + margin;
    panel->setSize(panel->getWidth(), totalHeight);
}

void ToolbarComponent::layoutDisplayPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    int y = margin;
    int w = panel->getWidth() - margin * 2;
    redBackgroundToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    highwayTextureLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    textureScaleLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    textureOpacityLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    gemScaleLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    highwayLengthLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight;

    // Resize panel to fit content
    int totalHeight = y + margin;
    panel->setSize(panel->getWidth(), totalHeight);
}

#ifdef DEBUG
void ToolbarComponent::setDebugPlay(bool playing)
{
    debugPanel.setDebugPlay(playing);
}
#endif

void ToolbarComponent::layoutSettingsPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    chartSpeedLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + 2;

    chartSpeedSlider.setBounds(margin, y, w, 130);
    y += 130 + gap;

    framerateMenu.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    if (!reaperMode)
    {
        latencyMenu.setBounds(margin, y, w, rowHeight);
        latencyMenu.setVisible(true);
        y += rowHeight + gap;
    }
    else
    {
        latencyMenu.setVisible(false);
    }

    latencyOffsetLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + 2;

    latencyOffsetInput.setBounds(margin, y, w, rowHeight);
    y += rowHeight + margin;

    panel->setSize(panel->getWidth(), y);
}
