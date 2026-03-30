#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// iOS/Material-style slide toggle: rounded-rect track with sliding circle knob.
// Coral when on, dim outline when off. Supports label text + hover label swap.
class SlideToggle : public juce::Component
{
public:
    SlideToggle(const juce::String& text) : label(text) {}
    SlideToggle(const juce::String& text, const juce::String& hover) : label(text), hoverLabel(hover) {}

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (on == shouldBeOn) return;
        on = shouldBeOn;
        knobT = on ? 1.0f : 0.0f;  // snap when set programmatically
        repaint();
        if (notification != juce::dontSendNotification && onClick)
            onClick();
    }

    bool getToggleState() const { return on; }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        float h = (float)getHeight();
        bool hovering = isMouseOver();

        // Track dimensions
        float trackH = h * 0.7f;
        float trackW = trackH * 1.8f;
        float trackY = (h - trackH) * 0.5f;
        float trackX = 2.0f;
        float trackR = trackH * 0.5f;
        auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);

        // Knob — slightly larger than track height for that iOS "peeking" look
        float knobD = trackH * 1.05f;
        float knobY = trackY + (trackH - knobD) * 0.5f;
        float knobPad = (trackH - knobD) * 0.5f;
        float knobOffX = trackX + knobPad;
        float knobOnX = trackX + trackW - knobD - knobPad;

        // Animate knob position
        float targetKnobT = on ? 1.0f : 0.0f;
        knobT += (targetKnobT - knobT) * 0.3f;
        if (std::abs(knobT - targetKnobT) > 0.005f)
        {
            auto& self = const_cast<SlideToggle&>(*this);
            juce::MessageManager::callAsync([&self]() { self.repaint(); });
        }
        else
        {
            knobT = targetKnobT;
        }
        float knobX = knobOffX + knobT * (knobOnX - knobOffX);

        if (on)
        {
            auto col = juce::Colour(Theme::coral);
            g.setColour(hovering ? col.brighter(0.15f) : col);
            g.fillRoundedRectangle(trackRect, trackR);

            g.setColour(juce::Colour(Theme::textWhite));
            g.fillEllipse(knobX, knobY, knobD, knobD);
        }
        else
        {
            g.setColour(hovering ? juce::Colour(Theme::textDim).withAlpha(0.6f)
                                 : juce::Colour(Theme::textDim).withAlpha(0.35f));
            g.fillRoundedRectangle(trackRect, trackR);

            g.setColour(juce::Colour(Theme::textDim).withAlpha(hovering ? 0.9f : 0.7f));
            g.fillEllipse(knobX, knobY, knobD, knobD);
        }

        // Label
        float labelX = trackX + trackW + trackH * 0.35f;
        g.setColour(hovering ? juce::Colour(Theme::textWhite)
                             : juce::Colour(Theme::textDim));
        g.setFont(Theme::controlFont);
        auto& displayLabel = (hovering && hoverLabel.isNotEmpty()) ? hoverLabel : label;
        g.drawText(displayLabel, juce::Rectangle<float>(labelX, 0.0f, getWidth() - labelX, h),
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
    juce::String hoverLabel;
    bool on = false;
    mutable float knobT = 0.0f;  // 0 = off position, 1 = on position
};
