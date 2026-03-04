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

    debugButton.addPanelChild(&debugPlayToggle);
    debugButton.addPanelChild(&chartSelectLabel);
    debugButton.addPanelChild(&debugConsoleToggle);
    debugButton.addPanelChild(&profilerToggle);
    debugButton.setPanelSize(150, 112);
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

void DebugToolbarPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    debugPlayToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    chartSelectLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    debugConsoleToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    profilerToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight;

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
