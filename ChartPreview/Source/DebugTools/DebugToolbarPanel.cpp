#ifdef DEBUG

#include "DebugToolbarPanel.h"

DebugToolbarPanel::DebugToolbarPanel()
{
    debugPlayToggle.setButtonText("Play");
    debugPlayToggle.onClick = [this]() {
        if (onDebugPlayChanged) onDebugPlayChanged(debugPlayToggle.getToggleState());
    };

    debugNotesToggle.setButtonText("Notes");
    debugNotesToggle.onClick = [this]() {
        if (onDebugNotesChanged) onDebugNotesChanged(debugNotesToggle.getToggleState());
    };

    debugConsoleToggle.setButtonText("Console");
    debugConsoleToggle.onClick = [this]() {
        if (onDebugConsoleChanged) onDebugConsoleChanged(debugConsoleToggle.getToggleState());
    };

    // BPM control row
    bpmMinusButton.onClick = [this]() { adjustBpm(-5); };
    bpmPlusButton.onClick = [this]() { adjustBpm(5); };

    bpmValueLabel.setText("120", juce::dontSendNotification);
    bpmValueLabel.setJustificationType(juce::Justification::centred);
    bpmValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmValueLabel.setInterceptsMouseClicks(true, true);
    bpmValueLabel.onScroll = [this](int delta) { adjustBpm(delta); };

    debugButton.addPanelChild(&debugPlayToggle);
    debugButton.addPanelChild(&debugNotesToggle);
    debugButton.addPanelChild(&debugConsoleToggle);
    debugButton.addPanelChild(&bpmMinusButton);
    debugButton.addPanelChild(&bpmValueLabel);
    debugButton.addPanelChild(&bpmPlusButton);
    debugButton.setPanelSize(150, 118);
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

void DebugToolbarPanel::adjustBpm(int delta)
{
    bpm = juce::jlimit(20, 300, bpm + delta);
    updateBpmLabel();
    if (onDebugBpmChanged) onDebugBpmChanged(bpm);
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

    debugNotesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    debugConsoleToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    // BPM row: [ - ] [ 120 ] [ + ]
    const int btnW = 24;
    const int labelW = w - btnW * 2 - gap * 2;
    int x = margin;
    bpmMinusButton.setBounds(x, y, btnW, rowHeight);
    x += btnW + gap;
    bpmValueLabel.setBounds(x, y, labelW, rowHeight);
    x += labelW + gap;
    bpmPlusButton.setBounds(x, y, btnW, rowHeight);
    y += rowHeight;

    panel->setSize(panel->getWidth(), y + margin);
}

void DebugToolbarPanel::updateBpmLabel()
{
    bpmValueLabel.setText(juce::String(bpm), juce::dontSendNotification);
}

#endif
