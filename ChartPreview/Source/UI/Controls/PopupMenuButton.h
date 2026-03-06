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
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(Theme::panelBgAlpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }

    // Outside-click callback — set by PopupMenuButton when click-locked
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
// Participates in ToolbarPanelGroup: hover-transfers between toolbar items,
// click-lock persists across transfers, click-outside dismisses.
class PopupMenuButton : public juce::TextButton,
                        private juce::ComponentListener
{
public:
    PopupMenuButton(const juce::String& buttonText) : juce::TextButton(buttonText)
    {
        ToolbarPanelGroup::registerMember(this);
    }

    ~PopupMenuButton() override
    {
        ToolbarPanelGroup::unregisterMember(this);
        dismissPanel();
    }

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
        openPanel(true);
    }

    void dismissPanel()
    {
        hoverTimer.stopTimer();
        ToolbarPanelGroup::deactivate(this);
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
        {
            if (ToolbarPanelGroup::locked && ToolbarPanelGroup::isOwner(this))
                dismissPanel();
            else
                upgradeToClickLock();
        }
        else
        {
            openPanel(true);
        }
    }

    // Hover: transfer from another active panel
    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseEnter(e);
        hoverTimer.stopTimer();
        if (panel != nullptr) return;

        if (ToolbarPanelGroup::hasActive() && !ToolbarPanelGroup::isOwner(this))
        {
            bool wasLocked = ToolbarPanelGroup::locked;
            openPanel(wasLocked);
        }
    }

    // Hover: start dismiss timer when mouse leaves button (unless locked)
    void mouseExit(const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseExit(e);
        if (panel != nullptr && !ToolbarPanelGroup::locked)
            startHoverTimer();
    }

private:
    void upgradeToClickLock()
    {
        hoverTimer.stopTimer();
        ToolbarPanelGroup::locked = true;

        panel->onOutsideMouseDown = [this](const juce::MouseEvent& e) {
            if (e.eventComponent == this) return;
            dismissPanel();
        };
        if (auto* topLevel = getTopLevelComponent())
            topLevel->addMouseListener(panel.get(), true);
    }

    void openPanel(bool locked)
    {
        if (panel != nullptr) return;

        ToolbarPanelGroup::activate(this, [this]() { dismissPanel(); }, locked);

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

        // Click-outside detection (panel as mouse listener on topLevel)
        if (locked)
        {
            panel->onOutsideMouseDown = [this](const juce::MouseEvent& e) {
                if (e.eventComponent == this) return;
                dismissPanel();
            };
            topLevel->addMouseListener(panel.get(), true);
        }

        // Hover dismiss polling (when not locked)
        if (!locked)
            startHoverTimer();

        setToggleState(true, juce::dontSendNotification);
        repaint();
    }

    void repositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        auto btnBottom = topLevel->getLocalPoint(this, juce::Point<int>(0, getHeight() + panelAnchorYOffset));
        int panelX = topLevel->getWidth() - panel->getWidth();
        int panelY = btnBottom.y + 6;

        panelY = juce::jmin(panelY, topLevel->getHeight() - panel->getHeight());

        panel->setTopLeftPosition(panelX, panelY);
    }

    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            repositionPanel();
    }

    // Polling timer: dismiss when mouse leaves both button and panel
    struct HoverTimer : public juce::Timer
    {
        PopupMenuButton* owner = nullptr;
        void timerCallback() override
        {
            if (!owner || !owner->panel) { stopTimer(); return; }
            if (ToolbarPanelGroup::isOwner(owner) && ToolbarPanelGroup::locked)
            { stopTimer(); return; }
            if (owner->isMouseOver(false) || owner->panel->isMouseOver(true))
                return;
            stopTimer();
            owner->dismissPanel();
        }
    };

    HoverTimer hoverTimer;

    void startHoverTimer()
    {
        hoverTimer.owner = this;
        hoverTimer.startTimer(120);
    }

    std::unique_ptr<PopupPanel> panel;
    std::vector<juce::Component*> panelChildren;
    int refPanelWidth = 160;
    int refPanelHeight = 200;
    float panelScale = 1.0f;
    int panelAnchorYOffset = 0;
};
