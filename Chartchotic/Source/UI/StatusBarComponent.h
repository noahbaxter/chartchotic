#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class StatusBarComponent : public juce::Component
{
public:
    // Clicking the track readout opens the parts menu. Set to make it clickable at all: with
    // no handler the label stays inert and does not light up, so it never offers a click that
    // goes nowhere.
    std::function<void()> onTrackLabelClicked;

    StatusBarComponent()
    {
        trackLabel.setColour(juce::Label::textColourId, restingColour());
        trackLabel.setJustificationType(juce::Justification::centredRight);
        trackLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(trackLabel);
    }

    void setTrackInfo(int trackNumber, const juce::String& trackName)
    {
        juce::String text = juce::String(trackNumber) + ": " + trackName;
        trackLabel.setText(text, juce::dontSendNotification);
        resized();
        repaint();
    }

    void clearTrackInfo()
    {
        trackLabel.setText("", juce::dontSendNotification);
        repaint();
    }

    void resized() override
    {
        trackLabel.setFont(Theme::getUIFont(Theme::fontSize));
        trackLabel.setBounds(getLocalBounds());
    }

    void mouseEnter(const juce::MouseEvent&) override { setHovered(true); }
    void mouseExit (const juce::MouseEvent&) override { setHovered(false); }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onTrackLabelClicked) onTrackLabelClicked();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return onTrackLabelClicked ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor;
    }

private:
    // Half-lit at rest so it reads as status, full white on hover so it reads as a control.
    static juce::Colour restingColour() { return juce::Colours::white.withAlpha(0.5f); }

    void setHovered(bool nowHovered)
    {
        if (!onTrackLabelClicked || hovered == nowHovered) return;
        hovered = nowHovered;
        trackLabel.setColour(juce::Label::textColourId,
                             hovered ? juce::Colours::white : restingColour());
        repaint();
    }

    juce::Label trackLabel;
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarComponent)
};
