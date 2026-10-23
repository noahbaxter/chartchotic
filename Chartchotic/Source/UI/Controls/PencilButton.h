#pragma once

#include <JuceHeader.h>

#include "../Theme.h"

/**
    A pencil that appears when you point at the thing it edits.

    For settings that are read far more often than they are changed. A button
    sitting there saying "Edit" spends a permanent piece of the window
    advertising something nobody does twice, and reads as a control rather
    than as the label it is attached to.
*/
class PencilButton : public juce::Button
{
public:
    PencilButton() : juce::Button("edit")
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    /**
        Draw a tick instead, and draw it whether or not you are pointing at it.

        The row it edits is open, so the way back out has to be visible
        without hunting for it, and what that click does now is accept rather
        than edit. A pencil that closes the thing a pencil opened reads as
        having done nothing.
    */
    void setDone(bool isDone)
    {
        if (done == isDone) return;
        done = isDone;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool hovered, bool held) override
    {
        if (!hovered && !held && !done) return;

        auto body = getLocalBounds().toFloat().reduced((float)getWidth() * inset);
        const float size = juce::jmin(body.getWidth(), body.getHeight());
        body = body.withSizeKeepingCentre(size, size);

        g.setColour(juce::Colour(hovered || held ? Theme::coral
                                : done          ? Theme::green
                                                : Theme::textDim));

        const float stroke = size * strokeShare;

        if (done)
        {
            juce::Path tick;
            tick.startNewSubPath(body.getX(), body.getCentreY());
            tick.lineTo(body.getX() + size * tickTurn, body.getBottom() - size * tickInset);
            tick.lineTo(body.getRight(), body.getY() + size * tickInset);
            g.strokePath(tick, juce::PathStrokeType(stroke, juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
            return;
        }

        // A nib on a shaft, corner to corner. Enough of a pencil at twenty
        // pixels, where anything more detailed is a smudge.
        juce::Path shaft;
        shaft.startNewSubPath(body.getX() + size * nibShare, body.getBottom() - size * nibShare);
        shaft.lineTo(body.getRight() - size * nibShare, body.getY() + size * nibShare);
        g.strokePath(shaft, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path nib;
        nib.startNewSubPath(body.getX(), body.getBottom());
        nib.lineTo(body.getX() + size * nibShare * 2.0f, body.getBottom() - size * nibShare);
        nib.lineTo(body.getX() + size * nibShare, body.getBottom() - size * nibShare * 2.0f);
        nib.closeSubPath();
        g.fillPath(nib);
    }

private:
    static constexpr float inset       = 0.14f;
    static constexpr float strokeShare = 0.14f;
    static constexpr float nibShare    = 0.18f;
    static constexpr float tickTurn    = 0.34f;
    static constexpr float tickInset   = 0.16f;

    bool done = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PencilButton)
};
