#pragma once

#include <JuceHeader.h>

// Clickable section header that toggles between collapsed/expanded state.
// Displays "+ Title -" when collapsed, "- Title -" when expanded.
// Wire onToggle to trigger a relayout of the parent panel.
class SectionHeader : public juce::Label
{
public:
    bool expanded = false;
    std::function<void()> onToggle;

    void mouseDown(const juce::MouseEvent&) override
    {
        expanded = !expanded;
        updateText();
        if (onToggle) onToggle();
    }

    void setTitle(const juce::String& title)
    {
        sectionTitle = title;
        updateText();
    }

private:
    juce::String sectionTitle;
    void updateText()
    {
        setText((expanded ? "- " : "+ ") + sectionTitle + " -", juce::dontSendNotification);
    }
};
