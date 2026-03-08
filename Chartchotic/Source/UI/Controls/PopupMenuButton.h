#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../MenuGroup.h"

// A panel that appears below a button and holds arbitrary child controls.
// Rendered as a child of the top-level component (not a desktop window).
class PopupPanel : public juce::Component
{
public:
    PopupPanel() { setAlwaysOnTop(true); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(Theme::panelBgAlpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::panelRadius);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Theme::panelRadius, 1.0f);
    }

    // Outside-click callback — set by PopupMenuButton to dismiss on click-outside
    std::function<void(const juce::MouseEvent&)> onOutsideMouseDown;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this || isParentOf(e.eventComponent))
            return;
        if (onOutsideMouseDown)
            onOutsideMouseDown(e);
    }
};

// A button that toggles a PopupPanel below itself.
// Click to open, click again or click outside to close.
// Assign to a MenuGroup for mutual exclusion with other menus.
class PopupMenuButton : public juce::TextButton,
                        private juce::ComponentListener
{
public:
    PopupMenuButton(const juce::String& buttonText) : juce::TextButton(buttonText) {}

    ~PopupMenuButton() override
    {
        dismissPanel();
    }

    void setMenuGroup(MenuGroup* group) { menuGroup = group; }

    void addPanelChild(juce::Component* child) { panelChildren.push_back(child); }

    void setPanelSize(int w, int h) { refPanelWidth = w; refPanelHeight = h; }
    void setPanelAnchorYOffset(int offset) { panelAnchorYOffset = offset; }

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
        openPanel();
    }

    void dismissPanel()
    {
        if (menuGroup != nullptr)
            menuGroup->deactivate(this);
        if (panel != nullptr)
        {
            if (auto* topLevel = getTopLevelComponent())
            {
                topLevel->removeComponentListener(this);
                topLevel->removeMouseListener(panel.get());
            }

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

    void clicked() override
    {
        if (panel != nullptr)
            dismissPanel();
        else
            openPanel();
    }

private:
    void openPanel()
    {
        if (panel != nullptr) return;

        if (menuGroup != nullptr)
            menuGroup->activate(this);

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
        topLevel->addComponentListener(this);

        // Click-outside detection
        panel->onOutsideMouseDown = [this](const juce::MouseEvent& e) {
            if (e.eventComponent == this) return;
            dismissPanel();
        };
        topLevel->addMouseListener(panel.get(), true);

        setToggleState(true, juce::dontSendNotification);
        repaint();
    }

    void repositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        auto btnBottom = topLevel->getLocalPoint(this, juce::Point<int>(0, getHeight() + panelAnchorYOffset));
        int gap = 6;
        int panelX = topLevel->getWidth() - panel->getWidth() - gap;
        int panelY = btnBottom.y + gap;

        panelY = juce::jmin(panelY, topLevel->getHeight() - panel->getHeight());

        panel->setTopLeftPosition(panelX, panelY);
    }

    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            repositionPanel();
    }

    MenuGroup* menuGroup = nullptr;
    std::unique_ptr<PopupPanel> panel;
    std::vector<juce::Component*> panelChildren;
    int refPanelWidth = 160;
    int refPanelHeight = 200;
    float panelScale = 1.0f;
    int panelAnchorYOffset = 0;
};
