#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../MenuGroup.h"

// A panel that appears below a button and holds arbitrary child controls.
// Rendered as a child of the top-level component (not a desktop window).
//
// Children live inside an inner "content" component so tall panels can scroll
// without re-laying out. The outer PopupPanel exposes itself to onLayoutPanel
// callbacks (so `panel->setSize` and `panel->getWidth` keep working for
// shrink-fit panels), but children are attached to content for scrolling.
//
// Scroll mechanisms:
//   - Drag the scrollbar (right edge, only visible when content overflows).
//   - Wheel over empty panel space, section headers, or row-label text.
//   - ScrollableLabel consumes wheel over its own cell (value tweak), so
//     scrolling from a value cell requires the scrollbar or empty space.
// Either way, wheel never reaches the underlying highway.
class PopupPanel : public juce::Component,
                   private juce::ScrollBar::Listener
{
public:
    PopupPanel()
        : scrollbar(true)
    {
        setAlwaysOnTop(true);
        addAndMakeVisible(content);
        content.setInterceptsMouseClicks(false, true);

        scrollbar.setAutoHide(false);
        scrollbar.addListener(this);
        addChildComponent(scrollbar);
    }

    juce::Component& getContent() { return content; }

    // Called by PopupMenuButton after it lays out children. Measures the
    // natural content height (max child bottom), updates the scrollbar, and
    // re-applies scroll clamp.
    void onContentLaidOut()
    {
        contentHeight = 0;
        for (auto* child : content.getChildren())
            if (child->isVisible())
                contentHeight = std::max(contentHeight, child->getBottom());

        const bool overflows = contentHeight > getHeight();
        scrollbar.setVisible(overflows);
        if (overflows)
        {
            scrollbar.setRangeLimits(0.0, (double)contentHeight);
            scrollbar.setCurrentRange((double)scrollOffset, (double)getHeight());
        }
        clampScroll();
        updateContentBounds();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(Theme::panelBgAlpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::panelRadius);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Theme::panelRadius, 1.0f);
    }

    void resized() override
    {
        // Scrollbar sits on the right edge, overlaying the panel's margin area
        // (panels typically reserve ~12px margin). Content fills the full width
        // so existing layoutPanel callbacks don't need to know about it.
        scrollbar.setBounds(getWidth() - scrollbarWidth - 2, 2,
                            scrollbarWidth, getHeight() - 4);
        updateContentBounds();
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

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (contentHeight <= getHeight()) return;
        scrollOffset -= juce::roundToInt(wheel.deltaY * scrollPixelsPerWheelUnit);
        clampScroll();
        updateContentBounds();
        scrollbar.setCurrentRangeStart((double)scrollOffset);
    }

private:
    void scrollBarMoved(juce::ScrollBar* sb, double newRangeStart) override
    {
        if (sb != &scrollbar) return;
        scrollOffset = juce::roundToInt((float)newRangeStart);
        updateContentBounds();
    }

    void clampScroll()
    {
        const int maxScroll = std::max(0, contentHeight - getHeight());
        scrollOffset = juce::jlimit(0, maxScroll, scrollOffset);
    }

    void updateContentBounds()
    {
        const int h = std::max(contentHeight, getHeight());
        content.setBounds(0, -scrollOffset, getWidth(), h);
    }

    juce::Component content;
    juce::ScrollBar scrollbar;
    int contentHeight = 0;
    int scrollOffset = 0;
    static constexpr int scrollPixelsPerWheelUnit = 60;
    static constexpr int scrollbarWidth = 6;
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
                panel->getContent().removeChildComponent(child);
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
            panel->onContentLaidOut();
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
            panel->getContent().addAndMakeVisible(child);

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

    // Lay out at requestedScale, then clamp height to the window — overflow
    // scrolls inside the panel instead of shrinking the whole UI unreadably.
    void fitAndPositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        int gap = 6;
        int panelY = panelTopMargin + gap;
        int maxH = topLevel->getHeight() - panelY - gap - panelBottomMargin;

        panelScale = requestedScale;
        layoutAtCurrentScale();

        if (maxH > 0 && panel->getHeight() > maxH)
        {
            panel->setSize(panel->getWidth(), maxH);
            panel->onContentLaidOut();
        }

        int panelX = topLevel->getWidth() - panel->getWidth() - gap;
        panel->setTopLeftPosition(panelX, panelY);
    }

    void layoutAtCurrentScale()
    {
        panel->setSize(juce::roundToInt(refPanelWidth * panelScale), refPanelHeight);
        if (onLayoutPanel)
            onLayoutPanel(panel.get());
        panel->onContentLaidOut();
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
