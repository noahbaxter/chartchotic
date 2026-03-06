#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Compact pill-shaped on/off toggle.
// Filled coral when ON, outlined when OFF. Hover highlights.
class PillToggle : public juce::Component
{
public:
    PillToggle(const juce::String& text) : label(text) {}

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (on == shouldBeOn) return;
        on = shouldBeOn;
        repaint();
        if (notification != juce::dontSendNotification && onClick)
            onClick();
    }

    bool getToggleState() const { return on; }
    void setButtonText(const juce::String& text) { label = text; repaint(); }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto cornerSize = bounds.getHeight() * Theme::pillRadius;
        bool hovering = isMouseOver();

        if (on)
        {
            g.setColour(juce::Colour(Theme::coral));
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(juce::Colours::white);
        }
        else
        {
            g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                                 : juce::Colour(Theme::textDim).withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            g.setColour(hovering ? juce::Colour(Theme::textWhite)
                                 : juce::Colour(Theme::textDim));
        }

        g.setFont(juce::Font(Theme::fontSize));
        g.drawText(label, getLocalBounds(), juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains(e.getPosition()))
        {
            on = !on;
            repaint();
            if (onClick) onClick();
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::String label;
    bool on = true;
};
