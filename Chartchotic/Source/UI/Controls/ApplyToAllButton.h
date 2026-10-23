#pragma once

#include <JuceHeader.h>

#include "../Theme.h"

/**
    A tick offering to put one value on everything else selected.

    Appears only where the offer is real: some of the selected songs carry a
    value and the rest carry nothing, so there is exactly one answer and the
    only question is whether it applies to all of them. Where they disagree
    there is nothing to offer and this stays away, because picking one of two
    real answers is not something a button should do quietly.
*/
class ApplyToAllButton : public juce::Button
{
public:
    ApplyToAllButton() : juce::Button("apply to all")
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Give this to every selected song");
    }

    void paintButton(juce::Graphics& g, bool hovered, bool held) override
    {
        auto body = getLocalBounds().toFloat().reduced((float)getWidth() * inset);
        const float size = juce::jmin(body.getWidth(), body.getHeight());
        body = body.withSizeKeepingCentre(size, size);

        if (hovered || held)
        {
            g.setColour(juce::Colour(Theme::green).withAlpha(backing));
            g.fillEllipse(getLocalBounds().toFloat());
        }

        juce::Path tick;
        tick.startNewSubPath(body.getX(), body.getCentreY());
        tick.lineTo(body.getX() + size * turn, body.getBottom() - size * edge);
        tick.lineTo(body.getRight(), body.getY() + size * edge);

        g.setColour(juce::Colour(Theme::green).withMultipliedAlpha(hovered || held ? 1.0f : quiet));
        g.strokePath(tick, juce::PathStrokeType(size * stroke, juce::PathStrokeType::mitered,
                                                juce::PathStrokeType::rounded));
    }

private:
    static constexpr float inset   = 0.24f;
    static constexpr float stroke  = 0.16f;
    static constexpr float turn    = 0.34f;
    static constexpr float edge    = 0.16f;
    static constexpr float backing = 0.16f;
    static constexpr float quiet   = 0.75f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyToAllButton)
};
