#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Uppercase section label with trailing divider line.
// Used to visually separate groups within popup panels.
class PanelSectionHeader : public juce::Component
{
public:
    PanelSectionHeader(const juce::String& text) : title(text) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto font = juce::Font(bounds.getHeight() * 0.625f);

        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.5f));
        g.setFont(font);
        g.drawText(title.toUpperCase(), bounds.withLeft(2.0f), juce::Justification::centredLeft);

        float textW = font.getStringWidthFloat(title.toUpperCase()) + 6.0f;
        g.drawLine(textW, bounds.getCentreY(), bounds.getRight(), bounds.getCentreY(), 0.5f);
    }

private:
    juce::String title;
};
