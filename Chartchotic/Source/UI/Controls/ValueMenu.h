#pragma once

#include <JuceHeader.h>

#include "../Theme.h"

/**
    The current value, and a list of the others when you click it.

    For a choice that is made once and then read many times. Laid out as a row
    of buttons, every option takes permanent room and the row reads as a
    control worth attending to; as a word with a chevron it takes the space of
    its answer and says what the answer is.

    Deliberately the same shape as SegmentedButtons, so which one a choice
    wears is a layout decision rather than a rewrite.
*/
class ValueMenu : public juce::Component
{
public:
    ValueMenu() = default;

    void setItems(const juce::StringArray& newItems)
    {
        items = newItems;
        selected = juce::jlimit(0, juce::jmax(0, items.size() - 1), selected);
        repaint();
    }

    void setSelectedIndex(int index, juce::NotificationType notification = juce::dontSendNotification)
    {
        const int wanted = juce::jlimit(0, juce::jmax(0, items.size() - 1), index);
        if (wanted == selected) return;

        selected = wanted;
        repaint();

        if (notification != juce::dontSendNotification && onSelectionChanged)
            onSelectionChanged(selected);
    }

    int getSelectedIndex() const { return selected; }

    juce::String selectedText() const { return items[selected]; }

    std::function<void(int)> onSelectionChanged;

    /**
        The longest option plus its chevron, so no choice makes it jump width.

        Measured in the height it will be drawn at rather than in whatever
        font the theme happens to be holding, which is how "folder" came out
        as "fol...".
    */
    int preferredWidth(int height) const
    {
        const auto font = fontFor(height);

        float widest = 0.0f;
        for (const auto& item : items)
            widest = juce::jmax(widest, font.getStringWidthFloat(item));

        // With slack: measured to the exact pixel, rounding leaves the text
        // one short of fitting and it comes out as "fold...".
        return juce::roundToInt(widest) + chevronRoom + padding * 2 + slack;
    }

    void paint(juce::Graphics& g) override
    {
        const bool live = isEnabled();
        auto bounds = getLocalBounds().toFloat();

        if (isMouseOver() && live)
        {
            g.setColour(juce::Colour(Theme::darkBgHover));
            g.fillRoundedRectangle(bounds, corner);
        }

        auto text = getLocalBounds().reduced(padding, 0);
        auto chevron = text.removeFromRight(chevronRoom);

        g.setColour(juce::Colour(live ? Theme::textWhite : Theme::textDim)
                        .withMultipliedAlpha(live ? 1.0f : 0.4f));
        g.setFont(fontFor(getHeight()));
        g.drawText(items[selected], text, juce::Justification::centredLeft, true);

        // A chevron rather than a triangle: at this size a filled arrow is a
        // blob, and the line reads as pointing down.
        const auto centre = chevron.getCentre().toFloat();
        const float reach = (float)chevronRoom * chevronShare;

        juce::Path arrow;
        arrow.startNewSubPath(centre.x - reach, centre.y - reach * 0.5f);
        arrow.lineTo(centre.x, centre.y + reach * 0.5f);
        arrow.lineTo(centre.x + reach, centre.y - reach * 0.5f);

        g.setColour(juce::Colour(Theme::textDim).withMultipliedAlpha(live ? 1.0f : 0.4f));
        g.strokePath(arrow, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (!isEnabled() || items.isEmpty()) return;

        // The window's own look, not the system list. Taken from whatever was
        // set on this control, so one look serves every menu in a window.
        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());

        for (int i = 0; i < items.size(); ++i)
            menu.addItem(i + 1, items[i], true, i == selected);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [this](int choice)
                           {
                               if (choice > 0) setSelectedIndex(choice - 1, juce::sendNotification);
                           });
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void enablementChanged() override { repaint(); }

private:
    static juce::Font fontFor(int height) { return Theme::getUIFont((float)height * fontShare); }

    static constexpr float fontShare   = 0.42f;
    static constexpr int   chevronRoom  = 16;
    static constexpr int   padding      = 6;
    static constexpr int   slack        = 4;
    static constexpr float corner       = 3.0f;
    static constexpr float chevronShare = 0.28f;

    juce::StringArray items;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueMenu)
};
