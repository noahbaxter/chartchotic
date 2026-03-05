#ifdef DEBUG

#include "DebugToolbarPanel.h"

DebugToolbarPanel::DebugToolbarPanel(juce::ValueTree& state)
    : state(state)
{
    // --- General controls ---

    debugPlayToggle.setButtonText("Play");
    debugPlayToggle.onClick = [this]() {
        if (onDebugPlayChanged) onDebugPlayChanged(debugPlayToggle.getToggleState());
    };

    chartSelectLabel.setText("Chart: " + chartNames[chartIndex], juce::dontSendNotification);
    chartSelectLabel.setJustificationType(juce::Justification::centredLeft);
    chartSelectLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    chartSelectLabel.setInterceptsMouseClicks(true, true);
    chartSelectLabel.onScroll = [this](int delta) {
        chartIndex = (chartIndex + delta + CHART_COUNT) % CHART_COUNT;
        chartSelectLabel.setText("Chart: " + chartNames[chartIndex], juce::dontSendNotification);
        if (onDebugChartChanged) onDebugChartChanged(chartIndex);
    };

    debugConsoleToggle.setButtonText("Console");
    debugConsoleToggle.onClick = [this]() {
        if (onDebugConsoleChanged) onDebugConsoleChanged(debugConsoleToggle.getToggleState());
    };

    profilerToggle.setButtonText("Profiler");
    profilerToggle.onClick = [this]() {
        if (onProfilerChanged) onProfilerChanged(profilerToggle.getToggleState());
    };

    // --- Layers section ---

    setupSectionHeader(layersHeader, "Layers");

    for (int i = 0; i < NUM_LAYERS; i++)
    {
        juce::String name(layerNames[i]);

        setupScrollLabel(layerScaleLabels[i]);
        layerScaleLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].scale = juce::jlimit(0.10f, 3.00f, layerStates[i].scale + delta * 0.005f);
            layerScaleLabels[i].setText(name + " S: " + juce::String(layerStates[i].scale, 3), juce::dontSendNotification);
            fireLayer(i);
        };

        setupScrollLabel(layerXLabels[i]);
        layerXLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].xOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].xOffset + delta * 0.0005f);
            layerXLabels[i].setText(name + " X: " + juce::String(layerStates[i].xOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };

        setupScrollLabel(layerYLabels[i]);
        layerYLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].yOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].yOffset + delta * 0.0005f);
            layerYLabels[i].setText(name + " Y: " + juce::String(layerStates[i].yOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };
    }

    // --- Tiling section ---

    setupSectionHeader(tilingHeader, "Tiling");

    setupScrollLabel(tileStepLabel);
    tileStepLabel.onScroll = [this](int delta) {
        tileStepValue = juce::jlimit(0.30f, 1.00f, tileStepValue + delta * 0.005f);
        tileStepLabel.setText("Step: " + juce::String(tileStepValue, 3), juce::dontSendNotification);
        if (onTileStepChanged) onTileStepChanged(tileStepValue);
    };

    setupScrollLabel(tileScaleStepLabel);
    tileScaleStepLabel.onScroll = [this](int delta) {
        tileScaleStepValue = juce::jlimit(0.30f, 1.00f, tileScaleStepValue + delta * 0.005f);
        tileScaleStepLabel.setText("Scale: " + juce::String(tileScaleStepValue, 3), juce::dontSendNotification);
        if (onTileScaleStepChanged) onTileScaleStepChanged(tileScaleStepValue);
    };

    refreshLabels();

    // Register panel children
    debugButton.addPanelChild(&debugPlayToggle);
    debugButton.addPanelChild(&chartSelectLabel);
    debugButton.addPanelChild(&debugConsoleToggle);
    debugButton.addPanelChild(&profilerToggle);

    debugButton.addPanelChild(&layersHeader);
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        debugButton.addPanelChild(&layerScaleLabels[i]);
        debugButton.addPanelChild(&layerXLabels[i]);
        debugButton.addPanelChild(&layerYLabels[i]);
    }

    debugButton.addPanelChild(&tilingHeader);
    debugButton.addPanelChild(&tileStepLabel);
    debugButton.addPanelChild(&tileScaleStepLabel);

    debugButton.setPanelSize(170, 180);
    debugButton.onLayoutPanel = [this](juce::Component* panel) { layoutPanel(panel); };
}

DebugToolbarPanel::~DebugToolbarPanel()
{
    debugButton.dismissPanel();
}

void DebugToolbarPanel::setDebugPlay(bool playing)
{
    debugPlayToggle.setToggleState(playing, juce::dontSendNotification);
    if (onDebugPlayChanged) onDebugPlayChanged(playing);
}

void DebugToolbarPanel::initDefaults(const TrackRenderer& trackRenderer)
{
    std::copy_n(trackRenderer.layersGuitar, NUM_LAYERS, guitarStates);
    std::copy_n(trackRenderer.layersDrums, NUM_LAYERS, drumStates);
    tileStepValue = trackRenderer.tileStep;
    tileScaleStepValue = trackRenderer.tileScaleStep;
    refreshLabels();
}

void DebugToolbarPanel::setDrums(bool isDrums)
{
    layerStates = isDrums ? drumStates : guitarStates;
    refreshLabels();
}

void DebugToolbarPanel::refreshLabels()
{
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        juce::String name(layerNames[i]);
        layerScaleLabels[i].setText(name + " S: " + juce::String(layerStates[i].scale, 3), juce::dontSendNotification);
        layerXLabels[i].setText(name + " X: " + juce::String(layerStates[i].xOffset, 4), juce::dontSendNotification);
        layerYLabels[i].setText(name + " Y: " + juce::String(layerStates[i].yOffset, 4), juce::dontSendNotification);
    }
    tileStepLabel.setText("Step: " + juce::String(tileStepValue, 3), juce::dontSendNotification);
    tileScaleStepLabel.setText("Scale: " + juce::String(tileScaleStepValue, 3), juce::dontSendNotification);
}

void DebugToolbarPanel::fireLayer(int idx)
{
    if (onLayerChanged)
        onLayerChanged(idx, layerStates[idx].scale, layerStates[idx].xOffset, layerStates[idx].yOffset);
}

void DebugToolbarPanel::setupSectionHeader(SectionHeader& header, const juce::String& text)
{
    header.setTitle(text);
    header.setJustificationType(juce::Justification::centred);
    header.setColour(juce::Label::textColourId, juce::Colours::grey);
    header.setFont(juce::Font(11.0f));
    header.setInterceptsMouseClicks(true, true);
    header.onToggle = [this]() {
        if (debugButton.isPanelVisible())
        {
            debugButton.dismissPanel();
            debugButton.showPanel();
        }
    };
}

void DebugToolbarPanel::setupScrollLabel(ScrollableLabel& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setInterceptsMouseClicks(true, true);
}

void DebugToolbarPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 3;
    const int headerGap = 6;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    // General controls
    debugPlayToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    chartSelectLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    debugConsoleToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    profilerToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + headerGap;

    // --- Layers section ---
    layersHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        layerScaleLabels[i].setVisible(layersHeader.expanded);
        layerXLabels[i].setVisible(layersHeader.expanded);
        layerYLabels[i].setVisible(layersHeader.expanded);
        if (layersHeader.expanded)
        {
            layerScaleLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
            layerXLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
            layerYLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap + 2;
        }
    }

    // --- Tiling section ---
    y += headerGap;
    tilingHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    tileStepLabel.setVisible(tilingHeader.expanded);
    tileScaleStepLabel.setVisible(tilingHeader.expanded);
    if (tilingHeader.expanded)
    {
        tileStepLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        tileScaleStepLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
