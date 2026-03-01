#include "ToolbarComponent.h"

ToolbarComponent::ToolbarComponent(juce::ValueTree& state)
    : state(state)
{
    initControls();
}

ToolbarComponent::~ToolbarComponent()
{
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
        // Re-layout display panel if open to show/hide drums-only toggles
        if (displayButton.isPanelVisible())
        {
            displayButton.dismissPanel();
            displayButton.showPanel();
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

    // Display popup children
    hitIndicatorsToggle.setButtonText("Hit Indicators");
    hitIndicatorsToggle.onClick = [this]() {
        if (onHitIndicatorsChanged) onHitIndicatorsChanged(hitIndicatorsToggle.getToggleState());
    };

    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.onClick = [this]() {
        if (onStarPowerChanged) onStarPowerChanged(starPowerToggle.getToggleState());
    };

    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.onClick = [this]() {
        if (onKick2xChanged) onKick2xChanged(kick2xToggle.getToggleState());
    };

    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.onClick = [this]() {
        if (onDynamicsChanged) onDynamicsChanged(dynamicsToggle.getToggleState());
    };

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

    // Gem scale label
    gemScaleLabel.setText("Gem Size: 100%", juce::dontSendNotification);
    gemScaleLabel.setJustificationType(juce::Justification::centredLeft);
    gemScaleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gemScaleLabel.setInterceptsMouseClicks(true, true);
    gemScaleLabel.onScroll = [this](int delta) {
        int count = (int)std::size(gemScaleValues);
        gemScaleIndex = juce::jlimit(0, count - 1, gemScaleIndex + delta);
        float scale = gemScaleValues[gemScaleIndex];
        gemScaleLabel.setText("Gem Size: " + juce::String((int)(scale * 100)) + "%", juce::dontSendNotification);
        if (onGemScaleChanged) onGemScaleChanged(scale);
    };

    // Display button
    displayButton.addPanelChild(&hitIndicatorsToggle);
    displayButton.addPanelChild(&starPowerToggle);
    displayButton.addPanelChild(&kick2xToggle);
    displayButton.addPanelChild(&dynamicsToggle);
    displayButton.addPanelChild(&redBackgroundToggle);
    displayButton.addPanelChild(&highwayTextureLabel);
    displayButton.addPanelChild(&gemScaleLabel);
    displayButton.setPanelSize(160, 120);
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
    debugPanel.onDebugPlayChanged = [this](bool playing) {
        if (onDebugPlayChanged) onDebugPlayChanged(playing);
    };
    debugPanel.onDebugNotesChanged = [this](bool notes) {
        if (onDebugNotesChanged) onDebugNotesChanged(notes);
    };
    debugPanel.onDebugConsoleChanged = [this](bool show) {
        if (onDebugConsoleChanged) onDebugConsoleChanged(show);
    };
    debugPanel.onDebugBpmChanged = [this](int bpm) {
        if (onDebugBpmChanged) onDebugBpmChanged(bpm);
    };
    debugPanel.onSustainStartCurveChanged = [this](float v) {
        if (onSustainStartCurveChanged) onSustainStartCurveChanged(v);
    };
    debugPanel.onSustainEndCurveChanged = [this](float v) {
        if (onSustainEndCurveChanged) onSustainEndCurveChanged(v);
    };
    debugPanel.onBarSustainStartCurveChanged = [this](float v) {
        if (onBarSustainStartCurveChanged) onBarSustainStartCurveChanged(v);
    };
    debugPanel.onBarSustainEndCurveChanged = [this](float v) {
        if (onBarSustainEndCurveChanged) onBarSustainEndCurveChanged(v);
    };
    debugPanel.onNoteCurveChanged = [this](float v) {
        if (onNoteCurveChanged) onNoteCurveChanged(v);
    };
    debugPanel.onBarCurveChanged = [this](float v) {
        if (onBarCurveChanged) onBarCurveChanged(v);
    };
    debugPanel.onSustainStartOffsetChanged = [this](float v) {
        if (onSustainStartOffsetChanged) onSustainStartOffsetChanged(v);
    };
    debugPanel.onSustainEndOffsetChanged = [this](float v) {
        if (onSustainEndOffsetChanged) onSustainEndOffsetChanged(v);
    };
    debugPanel.onBarSustainStartOffsetChanged = [this](float v) {
        if (onBarSustainStartOffsetChanged) onBarSustainStartOffsetChanged(v);
    };
    debugPanel.onBarSustainEndOffsetChanged = [this](float v) {
        if (onBarSustainEndOffsetChanged) onBarSustainEndOffsetChanged(v);
    };
    debugPanel.onSustainClipChanged = [this](float v) {
        if (onSustainClipChanged) onSustainClipChanged(v);
    };
    debugPanel.onBarSustainClipChanged = [this](float v) {
        if (onBarSustainClipChanged) onBarSustainClipChanged(v);
    };
    debugPanel.onLaneStartCurveChanged = [this](float v) {
        if (onLaneStartCurveChanged) onLaneStartCurveChanged(v);
    };
    debugPanel.onLaneEndCurveChanged = [this](float v) {
        if (onLaneEndCurveChanged) onLaneEndCurveChanged(v);
    };
    debugPanel.onLaneInnerStartCurveChanged = [this](float v) {
        if (onLaneInnerStartCurveChanged) onLaneInnerStartCurveChanged(v);
    };
    debugPanel.onLaneInnerEndCurveChanged = [this](float v) {
        if (onLaneInnerEndCurveChanged) onLaneInnerEndCurveChanged(v);
    };
    debugPanel.onLaneSideCurveChanged = [this](float v) {
        if (onLaneSideCurveChanged) onLaneSideCurveChanged(v);
    };
    debugPanel.onLaneStartOffsetChanged = [this](float v) {
        if (onLaneStartOffsetChanged) onLaneStartOffsetChanged(v);
    };
    debugPanel.onLaneEndOffsetChanged = [this](float v) {
        if (onLaneEndOffsetChanged) onLaneEndOffsetChanged(v);
    };
    debugPanel.onLaneClipChanged = [this](float v) {
        if (onLaneClipChanged) onLaneClipChanged(v);
    };
    debugPanel.onGuitarLaneCoordChanged = [this](int col, float pos, float width) {
        if (onGuitarLaneCoordChanged) onGuitarLaneCoordChanged(col, pos, width);
    };
    debugPanel.onDrumLaneCoordChanged = [this](int col, float pos, float width) {
        if (onDrumLaneCoordChanged) onDrumLaneCoordChanged(col, pos, width);
    };
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
    auto area = getLocalBounds().reduced(6, 4);
    const int controlHeight = 28;
    const int comboWidth = 90;
    const int gap = 6;

    // Left side: dropdowns
    skillMenu.setBounds(area.removeFromLeft(comboWidth).withHeight(controlHeight).withCentre({area.getX() - comboWidth / 2 - gap, area.getCentreY()}));

    // Recalculate — simpler approach
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

    hitIndicatorsToggle.setToggleState((bool)state["hitIndicators"], juce::dontSendNotification);
    starPowerToggle.setToggleState((bool)state["starPower"], juce::dontSendNotification);
    kick2xToggle.setToggleState((bool)state["kick2x"], juce::dontSendNotification);
    dynamicsToggle.setToggleState((bool)state["dynamics"], juce::dontSendNotification);
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

void ToolbarComponent::layoutDisplayPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    int y = margin;
    int w = panel->getWidth() - margin * 2;
    bool isDrums = isPart(state, Part::DRUMS);

    hitIndicatorsToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    starPowerToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    kick2xToggle.setBounds(margin, y, w, rowHeight);
    kick2xToggle.setVisible(isDrums);
    y += isDrums ? (rowHeight + gap) : 0;

    dynamicsToggle.setBounds(margin, y, w, rowHeight);
    dynamicsToggle.setVisible(isDrums);
    y += isDrums ? (rowHeight + gap) : 0;

    redBackgroundToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    highwayTextureLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    gemScaleLabel.setBounds(margin, y, w, rowHeight);
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
