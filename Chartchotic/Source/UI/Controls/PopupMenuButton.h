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
    void setPanelTopMargin(int offset) { panelTopMargin = offset; }
    void setPanelBottomMargin(int m) { panelBottomMargin = m; }

    void setScale(float s)
    {
        requestedScale = s;
        if (panel != nullptr)
            fitAndPositionPanel();
        else
            panelScale = s;
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
            if (auto* topLevel = Theme::getOverlayParent(this))
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

        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        panel = std::make_unique<PopupPanel>();
        panel->setSize(juce::roundToInt(refPanelWidth * requestedScale), refPanelHeight);

        for (auto* child : panelChildren)
            panel->addAndMakeVisible(child);

        topLevel->addAndMakeVisible(panel.get());
        fitAndPositionPanel();
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

    // Lay out at requestedScale, then shrink if the panel overflows vertically.
    void fitAndPositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        int gap = 6;
        int panelY = panelTopMargin + gap;
        int maxH = topLevel->getHeight() - panelY - gap - panelBottomMargin;

        // First pass: lay out at the full requested scale
        panelScale = requestedScale;
        layoutAtCurrentScale();

        // If it overflows, compute the exact scale that fits
        if (panel->getHeight() > maxH && maxH > 0)
        {
            float ratio = (float)maxH / (float)panel->getHeight();
            panelScale = requestedScale * ratio;
            layoutAtCurrentScale();
        }

        int panelX = topLevel->getWidth() - panel->getWidth() - gap;
        panel->setTopLeftPosition(panelX, panelY);
    }

    void layoutAtCurrentScale()
    {
        panel->setSize(juce::roundToInt(refPanelWidth * panelScale), refPanelHeight);
        if (onLayoutPanel)
            onLayoutPanel(panel.get());
    }

    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            fitAndPositionPanel();
    }

    MenuGroup* menuGroup = nullptr;
    std::unique_ptr<PopupPanel> panel;
    std::vector<juce::Component*> panelChildren;
    int refPanelWidth = 160;
    int refPanelHeight = 200;
    float panelScale = 1.0f;
    float requestedScale = 1.0f;
    int panelTopMargin = 0;
    int panelBottomMargin = 0;
};
