#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// One-of-many picker that shows only the current choice, for a row too tight for every option
// to sit side by side. Click opens a menu, the wheel steps through the items in place.
class DropdownButton : public juce::Component
{
public:
    void setItems(const juce::StringArray& labels) { items = labels; repaint(); }

    void setSelectedIndex(int index)
    {
        if (selectedIndex == index) return;
        selectedIndex = index;
        repaint();
    }

    int getSelectedIndex() const { return selectedIndex; }

    std::function<void(int index)> onSelectionChanged;

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void setCornerRadius(float r) { cornerRadius = r; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto cornerSize = (cornerRadius >= 0.0f) ? cornerRadius : Theme::pillCorner;
        bool hovering = isMouseOver();

        g.setColour(juce::Colour(hovering ? Theme::darkBgHover : Theme::darkBg));
        g.fillRoundedRectangle(bounds, cornerSize);
        g.setColour(accent.withAlpha(hovering ? 0.8f : 0.5f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        const float arrowW = bounds.getHeight() * ARROW_W_RATIO;
        auto textArea = bounds.withTrimmedRight(arrowW).toNearestInt();

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::controlFont);
        if (juce::isPositiveAndBelow(selectedIndex, items.size()))
            g.drawText(items[selectedIndex], textArea, juce::Justification::centred);

        const float cx = bounds.getRight() - arrowW * 0.5f;
        const float cy = bounds.getCentreY();
        const float s = arrowW * ARROW_SIZE_RATIO;
        juce::Path arrow;
        arrow.addTriangle(cx - s, cy - s * 0.5f, cx + s, cy - s * 0.5f, cx, cy + s * 0.5f);
        g.setColour(juce::Colour(Theme::textDim));
        g.fillPath(arrow);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition()) || items.isEmpty()) return;

        juce::PopupMenu menu;
        for (int i = 0; i < items.size(); ++i)
            menu.addItem(i + 1, items[i], true, i == selectedIndex);

        juce::Component::SafePointer<DropdownButton> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe](int result)
                           {
                               if (safe != nullptr && result > 0) safe->choose(result - 1);
                           });
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (items.isEmpty()) return;
        int delta = (wheel.deltaY > 0.0f) ? 1 : (wheel.deltaY < 0.0f) ? -1 : 0;
        if (delta != 0) choose(juce::jlimit(0, items.size() - 1, selectedIndex + delta));
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { repaint(); }

private:
    static constexpr float ARROW_W_RATIO = 0.9f;     // of the height
    static constexpr float ARROW_SIZE_RATIO = 0.25f; // of the arrow column

    void choose(int index)
    {
        if (index == selectedIndex) return;
        selectedIndex = index;
        repaint();
        if (onSelectionChanged) onSelectionChanged(selectedIndex);
    }

    juce::StringArray items;
    int selectedIndex = 0;
    juce::Colour accent { Theme::coral };
    float cornerRadius = -1.0f;
};
