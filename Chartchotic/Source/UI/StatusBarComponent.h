#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class StatusBarComponent : public juce::Component
{
public:
    StatusBarComponent()
    {
        trackLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
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

private:
    juce::Label trackLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarComponent)
};
