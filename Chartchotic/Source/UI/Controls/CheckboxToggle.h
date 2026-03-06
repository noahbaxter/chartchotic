#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Checkbox-style toggle: small square box with checkmark + label text.
// Coral fill when checked, outlined when unchecked. Matches theme.
class CheckboxToggle : public juce::Component
{
public:
    CheckboxToggle(const juce::String& text) : label(text) {}

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (on == shouldBeOn) return;
        on = shouldBeOn;
        repaint();
        if (notification != juce::dontSendNotification && onClick)
            onClick();
    }

    bool getToggleState() const { return on; }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        float h = (float)getHeight();
        float boxSize = h * 0.7f;
        float boxY = (h - boxSize) * 0.5f;
        float boxX = 2.0f;
        auto box = juce::Rectangle<float>(boxX, boxY, boxSize, boxSize);
        float corner = boxSize * 0.2f;
        bool hovering = isMouseOver();

        if (on)
        {
            g.setColour(hovering ? juce::Colour(Theme::coral).brighter(0.15f)
                                 : juce::Colour(Theme::coral));
            g.fillRoundedRectangle(box, corner);

            // Checkmark
            g.setColour(juce::Colour(Theme::textWhite));
            float cx = box.getCentreX();
            float cy = box.getCentreY();
            float s = boxSize * 0.25f;
            juce::Path check;
            check.startNewSubPath(cx - s, cy);
            check.lineTo(cx - s * 0.3f, cy + s * 0.7f);
            check.lineTo(cx + s, cy - s * 0.6f);
            g.strokePath(check, juce::PathStrokeType(boxSize * 0.12f,
                juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                                 : juce::Colour(Theme::textDim).withAlpha(0.5f));
            g.drawRoundedRectangle(box, corner, 1.0f);
        }

        // Label
        float labelX = boxX + boxSize + boxSize * 0.4f;
        g.setColour(hovering ? juce::Colour(Theme::textWhite)
                             : juce::Colour(Theme::textDim));
        g.setFont(Theme::controlFont);
        g.drawText(label, juce::Rectangle<float>(labelX, 0.0f, getWidth() - labelX, h),
                   juce::Justification::centredLeft);
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
    bool on = false;
};
