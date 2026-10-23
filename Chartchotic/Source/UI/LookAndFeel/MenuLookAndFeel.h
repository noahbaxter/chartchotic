#pragma once

#include <JuceHeader.h>

#include "../Theme.h"

/**
    The look for the small menus that open off a value or a gear.

    JUCE's default popup is a system-grey list, which beside a dark coral
    window looks like something another application opened. These menus are
    part of the control that opened them, so they are drawn the way the rest
    of the window is: dark, quiet, coral where something is chosen.
*/
class MenuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MenuLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(Theme::darkBg));
        setColour(juce::PopupMenu::textColourId, juce::Colour(Theme::textWhite));
        setColour(juce::PopupMenu::headerTextColourId, juce::Colour(Theme::textDim));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  juce::Colour(Theme::coral).withAlpha(highlight));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(Theme::textWhite));
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);

        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);
    }

    juce::Font getPopupMenuFont() override { return Theme::getUIFont(fontHeight); }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int standardMenuItemHeight, int& idealWidth,
                                   int& idealHeight) override
    {
        if (isSeparator)
        {
            idealWidth = 50;
            idealHeight = separatorHeight;
            return;
        }

        const auto font = getPopupMenuFont();
        idealHeight = juce::jmax(standardMenuItemHeight, itemHeight);
        idealWidth = juce::roundToInt(font.getStringWidthFloat(text)) + itemHeight * 2;
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        juce::ignoreUnused(hasSubMenu, shortcutKeyText, icon, textColourToUse);

        if (isSeparator)
        {
            auto line = area.reduced(itemHeight / 2, 0).withHeight(1)
                            .withY(area.getCentreY());
            g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
            g.fillRect(line);
            return;
        }

        auto row = area.reduced(padding, 1);

        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(Theme::coral).withAlpha(highlight));
            g.fillRoundedRectangle(row.toFloat(), corner);
        }

        // The tick sits in its own column, so the labels line up whether or
        // not anything in the menu is chosen.
        auto tickColumn = row.removeFromLeft(itemHeight);

        if (isTicked)
        {
            auto body = tickColumn.reduced(tickColumn.getWidth() / 3).toFloat();

            juce::Path tick;
            tick.startNewSubPath(body.getX(), body.getCentreY());
            tick.lineTo(body.getX() + body.getWidth() * 0.36f, body.getBottom());
            tick.lineTo(body.getRight(), body.getY());

            g.setColour(juce::Colour(Theme::coral));
            g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colour(isActive ? Theme::textWhite : Theme::textDim)
                        .withMultipliedAlpha(isActive ? 1.0f : 0.4f));
        g.setFont(getPopupMenuFont());
        g.drawText(text, row, juce::Justification::centredLeft, true);
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    const juce::String& text) override
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(headerHeight));
        g.drawText(text.toUpperCase(), area.reduced(padding + itemHeight, 0),
                   juce::Justification::centredLeft, true);
    }

private:
    static constexpr float corner          = 4.0f;
    static constexpr float highlight       = 0.22f;
    static constexpr float fontHeight      = 13.0f;
    static constexpr float headerHeight    = 11.0f;
    static constexpr int   itemHeight      = 24;
    static constexpr int   separatorHeight = 9;
    static constexpr int   padding         = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuLookAndFeel)
};
