#ifdef DEBUG

#include "DebugToolbarPanel.h"

DebugToolbarPanel::DebugToolbarPanel(juce::ValueTree& state)
    : state(state)
{
    debugPlayToggle.setButtonText("Play");
    debugPlayToggle.onClick = [this]() {
        if (onDebugPlayChanged) onDebugPlayChanged(debugPlayToggle.getToggleState());
    };

    // Chart selector (scroll to cycle)
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

    // Per-layer tuning controls
    auto setup = [](ScrollableLabel& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setInterceptsMouseClicks(true, true);
    };

    for (int i = 0; i < NUM_LAYERS; i++)
    {
        juce::String name(layerNames[i]);

        setup(layerScaleLabels[i], "");
        layerScaleLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].scale = juce::jlimit(0.10f, 3.00f, layerStates[i].scale + delta * 0.005f);
            layerScaleLabels[i].setText(name + " S: " + juce::String(layerStates[i].scale, 3), juce::dontSendNotification);
            fireLayer(i);
        };

        setup(layerXLabels[i], "");
        layerXLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].xOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].xOffset + delta * 0.0005f);
            layerXLabels[i].setText(name + " X: " + juce::String(layerStates[i].xOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };

        setup(layerYLabels[i], "");
        layerYLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].yOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].yOffset + delta * 0.0005f);
            layerYLabels[i].setText(name + " Y: " + juce::String(layerStates[i].yOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };
    }
    refreshLabels();

    debugButton.addPanelChild(&debugPlayToggle);
    debugButton.addPanelChild(&chartSelectLabel);
    debugButton.addPanelChild(&debugConsoleToggle);
    debugButton.addPanelChild(&profilerToggle);
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        debugButton.addPanelChild(&layerScaleLabels[i]);
        debugButton.addPanelChild(&layerXLabels[i]);
        debugButton.addPanelChild(&layerYLabels[i]);
    }
    debugButton.setPanelSize(170, 460);
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
}

void DebugToolbarPanel::fireLayer(int idx)
{
    if (onLayerChanged)
        onLayerChanged(idx, layerStates[idx].scale, layerStates[idx].xOffset, layerStates[idx].yOffset);
}

void DebugToolbarPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 3;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    debugPlayToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    chartSelectLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    debugConsoleToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    profilerToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap * 2;

    // Per-layer tuning: 3 rows per layer, small gap between layers
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        layerScaleLabels[i].setBounds(margin, y, w, rowHeight);
        y += rowHeight + gap;
        layerXLabels[i].setBounds(margin, y, w, rowHeight);
        y += rowHeight + gap;
        layerYLabels[i].setBounds(margin, y, w, rowHeight);
        y += rowHeight + gap + 2;
    }

    panel->setSize(panel->getWidth(), y + margin - gap - 2);
}

#endif
