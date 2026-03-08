#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../MenuGroup.h"

struct CircleItem
{
    juce::String label;    // short text shown in circle ("E", "M", etc.)
    juce::Image image;     // optional PNG (instruments)
    juce::Colour colour;   // optional per-item colour (default = unset/black)
    juce::String tooltip;  // longer name shown in expanded panel ("Easy", "Guitar", etc.)
};

// Circle icon selector — hover to open, move away to close.
// Click an item in the expanded panel to select it. Scroll to cycle.
// Assign to a MenuGroup for mutual exclusion with other circle selectors.
class CircleIconSelector : public juce::Component,
                           private juce::ComponentListener
{
public:
    CircleIconSelector() {}

    ~CircleIconSelector() override
    {
        dismissPanel();
    }

    void setMenuGroup(MenuGroup* group) { menuGroup = group; }

    void setItems(std::vector<CircleItem> newItems)
    {
        items = std::move(newItems);
        repaint();
    }

    void setSelectedIndex(int index, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (selectedIndex == index) return;
        selectedIndex = index;
        repaint();
        if (notification != juce::dontSendNotification && onSelectionChanged)
            onSelectionChanged(selectedIndex);
    }

    int getSelectedIndex() const { return selectedIndex; }

    std::function<void(int index)> onSelectionChanged;

    void dismissPanel()
    {
        hoverTimer.stopTimer();
        if (menuGroup != nullptr)
            menuGroup->deactivate(this);
        if (panel != nullptr)
        {
            if (auto* topLevel = getTopLevelComponent())
                topLevel->removeComponentListener(this);
            panel.reset();
        }
    }

    bool isPanelVisible() const { return panel != nullptr; }

    void paint(juce::Graphics& g) override
    {
        if (items.empty()) return;
        auto bounds = getLocalBounds().toFloat();
        float d = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto circle = juce::Rectangle<float>(
            bounds.getCentreX() - d / 2.0f,
            bounds.getCentreY() - d / 2.0f, d, d);
        paintCircle(g, circle, items[(size_t)selectedIndex], true, false);

        if (mouseHovered || panel != nullptr)
        {
            g.setColour(juce::Colour(Theme::coral));
            g.drawEllipse(circle.reduced(0.5f), 1.5f);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        mouseHovered = true;
        repaint();
        hoverTimer.stopTimer();
        if (panel == nullptr)
            showPanel();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        mouseHovered = false;
        repaint();
        startHoverTimer();
    }

    // Scroll: cycle through items without wrapping (only over the main circle icon)
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (items.empty() || !isMouseOver(false)) return;
        int dir = (wheel.deltaY > 0) ? -1 : 1;
        int newIndex = juce::jlimit(0, (int)items.size() - 1, selectedIndex + dir);
        setSelectedIndex(newIndex, juce::sendNotification);
    }

    void showPanel()
    {
        if (panel != nullptr) return;

        if (menuGroup != nullptr)
            menuGroup->activate(this);

        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        float d = (float)juce::jmin(getWidth(), getHeight());
        int padding = 4;
        int labelW = juce::roundToInt(d * 2.5f);
        int panelW = (int)d + padding * 2 + labelW;
        int panelH = (int)(d * (int)items.size()) + padding * ((int)items.size() + 1);

        panel = std::make_unique<ExpandedPanel>(*this, d, (float)padding, labelW);
        panel->setSize(panelW, panelH);

        topLevel->addAndMakeVisible(panel.get());
        repositionPanel();
        topLevel->addComponentListener(this);
    }

private:
    MenuGroup* menuGroup = nullptr;

    std::vector<CircleItem> items;
    int selectedIndex = 0;
    int panelHoverIndex = -1;
    bool mouseHovered = false;

    //==========================================================================
    class ExpandedPanel : public juce::Component
    {
    public:
        ExpandedPanel(CircleIconSelector& o, float circleD, float pad, int lw)
            : owner(o), d(circleD), padding(pad), labelW(lw)
        {
            setAlwaysOnTop(true);
        }

        void paint(juce::Graphics& g) override
        {
            for (int i = 0; i < (int)owner.items.size(); i++)
            {
                auto circle = getCircleBounds(i);
                bool selected = (i == owner.selectedIndex);
                bool hovering = (i == owner.panelHoverIndex);
                owner.paintCircle(g, circle, owner.items[(size_t)i], selected, hovering);

                auto& item = owner.items[(size_t)i];
                auto text = item.tooltip.isNotEmpty() ? item.tooltip : item.label;
                if (text.isNotEmpty())
                {
                    auto labelBounds = juce::Rectangle<float>(
                        circle.getRight() + padding, circle.getY(),
                        (float)labelW - padding, circle.getHeight());
                    g.setColour(hovering ? juce::Colour(Theme::textWhite)
                                         : juce::Colour(Theme::textDim));
                    g.setFont(Theme::controlFont);
                    g.drawText(text, labelBounds.toNearestInt(),
                               juce::Justification::centredLeft);
                }
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            int idx = hitTestItem(e.y);
            if (idx != owner.panelHoverIndex) { owner.panelHoverIndex = idx; repaint(); }
        }

        void mouseEnter(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            owner.hoverTimer.stopTimer();
        }

        void mouseExit(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            owner.panelHoverIndex = -1;
            repaint();
            owner.startHoverTimer();
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            int idx = hitTestItem(e.y);
            if (idx >= 0 && idx < (int)owner.items.size() && idx != owner.selectedIndex)
            {
                owner.selectedIndex = idx;
                owner.repaint();
                if (owner.onSelectionChanged)
                    owner.onSelectionChanged(owner.selectedIndex);
            }
            owner.dismissPanel();
        }

    private:
        CircleIconSelector& owner;
        float d, padding;
        int labelW;

        juce::Rectangle<float> getCircleBounds(int index) const
        {
            float cy = padding + index * (d + padding);
            return { padding, cy, d, d };
        }

        int hitTestItem(int mouseY) const
        {
            float stride = d + padding;
            int idx = (int)((mouseY - padding) / stride);
            if (idx < 0 || idx >= (int)owner.items.size()) return -1;
            float itemTop = padding + idx * stride;
            if (mouseY >= itemTop && mouseY <= itemTop + d) return idx;
            return -1;
        }
    };

    std::unique_ptr<ExpandedPanel> panel;

    //==========================================================================
    void paintCircle(juce::Graphics& g, juce::Rectangle<float> bounds,
                     const CircleItem& item, bool selected, bool hovering) const
    {
        bool hasColour = item.colour != juce::Colour();

        if (hasColour)
        {
            g.setColour(item.colour);
        }
        else
        {
            if (selected)
                g.setColour(juce::Colour(Theme::coral));
            else if (hovering)
                g.setColour(juce::Colour(Theme::darkBgHover));
            else
                g.setColour(juce::Colour(Theme::darkBg));
        }

        g.fillEllipse(bounds);

        if (item.image.isValid())
        {
            auto reduced = bounds.reduced(bounds.getWidth() * 0.05f);
            g.saveState();
            juce::Path clipPath;
            clipPath.addEllipse(bounds);
            g.reduceClipRegion(clipPath);
            g.drawImageWithin(item.image,
                (int)reduced.getX(), (int)reduced.getY(),
                (int)reduced.getWidth(), (int)reduced.getHeight(),
                juce::RectanglePlacement::centred);
            g.restoreState();
        }
        else if (item.label.isNotEmpty())
        {
            g.setColour(juce::Colour(Theme::textWhite));
            auto typeface = juce::Typeface::createSystemTypefaceFor(
                BinaryData::RussoOneRegular_ttf, BinaryData::RussoOneRegular_ttfSize);
            auto font = juce::Font(typeface).withHeight(bounds.getHeight() * 0.72f);
            g.setFont(font);
            g.drawText(item.label, bounds.translated(0.5f, 0.0f).toNearestInt(), juce::Justification::centred);
            g.drawText(item.label, bounds.toNearestInt(), juce::Justification::centred);
        }

        if (!hasColour)
        {
            g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
            g.drawEllipse(bounds.reduced(0.5f), 1.0f);
        }
    }

    void repositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr) return;

        float d = (float)juce::jmin(getWidth(), getHeight());
        int padding = 4;
        auto btnBottom = topLevel->getLocalPoint(this, juce::Point<int>(getWidth() / 2, getHeight()));
        int panelX = btnBottom.x - padding - (int)(d / 2.0f);
        int panelY = btnBottom.y + 4;

        panelX = juce::jmax(0, juce::jmin(panelX, topLevel->getWidth() - panel->getWidth()));
        panelY = juce::jmin(panelY, topLevel->getHeight() - panel->getHeight());

        panel->setTopLeftPosition(panelX, panelY);
    }

    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            repositionPanel();
    }

    struct HoverTimer : public juce::Timer
    {
        CircleIconSelector* owner = nullptr;
        void timerCallback() override
        {
            if (!owner || !owner->panel) { stopTimer(); return; }
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
        hoverTimer.startTimer(Theme::hoverDismissMs);
    }
};
