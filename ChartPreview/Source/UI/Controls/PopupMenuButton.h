#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// A panel that appears below a button and holds arbitrary child controls.
// Rendered as a child of the top-level component (not a desktop window).
class PopupPanel : public juce::Component
{
public:
    PopupPanel() { setAlwaysOnTop(true); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }
};

// A button that toggles a PopupPanel below itself.
// Panel is added as a child of the top-level component so it stays
// within the plugin window and repositions on resize.
// Only dismisses when the button is clicked again.
class PopupMenuButton : public juce::TextButton,
                        private juce::ComponentListener
{
public:
    PopupMenuButton(const juce::String& buttonText) : juce::TextButton(buttonText) {}

    ~PopupMenuButton() override
    {
        dismissPanel();
    }

    void addPanelChild(juce::Component* child) { panelChildren.push_back(child); }

    void setExclusiveGroup(std::vector<PopupMenuButton*> group)
    {
        exclusiveGroup = group;
    }

    void setPanelSize(int w, int h) { refPanelWidth = w; refPanelHeight = h; }

    void setScale(float s)
    {
        panelScale = s;
        if (panel != nullptr)
        {
            panel->setSize(juce::roundToInt(refPanelWidth * panelScale), panel->getHeight());
            if (onLayoutPanel)
                onLayoutPanel(panel.get());
            repositionPanel();
        }
    }

    float getScale() const { return panelScale; }

    bool isPanelVisible() const { return panel != nullptr && panel->isVisible(); }

    void showPanel()
    {
        if (panel != nullptr) { dismissPanel(); return; }

        // Dismiss any other open panel in the exclusive group
        for (auto* other : exclusiveGroup)
            if (other != this && other->isPanelVisible())
                other->dismissPanel();

        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        int scaledW = juce::roundToInt(refPanelWidth * panelScale);

        panel = std::make_unique<PopupPanel>();
        panel->setSize(scaledW, refPanelHeight);

        for (auto* child : panelChildren)
            panel->addAndMakeVisible(child);

        if (onLayoutPanel)
            onLayoutPanel(panel.get());

        topLevel->addAndMakeVisible(panel.get());
        repositionPanel();

        // Listen for top-level resize to reposition
        topLevel->addComponentListener(this);

        setToggleState(true, juce::dontSendNotification);
        repaint();
    }

    void dismissPanel()
    {
        if (panel != nullptr)
        {
            if (auto* topLevel = getTopLevelComponent())
                topLevel->removeComponentListener(this);

            for (auto* child : panelChildren)
                panel->removeChildComponent(child);
            panel.reset();
        }

        setToggleState(false, juce::dontSendNotification);
        repaint();
    }

    void relayoutPanel()
    {
        if (panel != nullptr && onLayoutPanel)
        {
            onLayoutPanel(panel.get());
            panel->repaint();
        }
    }

    std::function<void(juce::Component* panel)> onLayoutPanel;

    void clicked() override { showPanel(); }

private:
    void repositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        // Position panel below button, right-aligned to button's right edge
        auto btnBottomRight = topLevel->getLocalPoint(this, juce::Point<int>(getWidth(), getHeight()));
        int panelX = btnBottomRight.x - panel->getWidth();
        int panelY = btnBottomRight.y + 2;

        // Clamp to stay within the top-level bounds
        panelX = juce::jmax(0, juce::jmin(panelX, topLevel->getWidth() - panel->getWidth()));
        panelY = juce::jmin(panelY, topLevel->getHeight() - panel->getHeight());

        panel->setTopLeftPosition(panelX, panelY);
    }

    // ComponentListener -- reposition when top-level resizes
    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            repositionPanel();
    }

    std::vector<PopupMenuButton*> exclusiveGroup;
    std::unique_ptr<PopupPanel> panel;
    std::vector<juce::Component*> panelChildren;
    int refPanelWidth = 160;
    int refPanelHeight = 200;
    float panelScale = 1.0f;
};
