#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/PopupMenuButton.h"
#include "../Utils/Utils.h"

class DebugToolbarPanel
{
public:
    DebugToolbarPanel(juce::ValueTree& state);
    ~DebugToolbarPanel();

    PopupMenuButton& getButton() { return debugButton; }
    void setDebugPlay(bool playing);

    // Callbacks -- the editor wires these
    std::function<void(bool playing)> onDebugPlayChanged;
    std::function<void(int)> onDebugChartChanged;
    std::function<void(bool)> onDebugConsoleChanged;
    std::function<void(bool)> onProfilerChanged;

private:
    juce::ValueTree& state;
    PopupMenuButton debugButton{"Debug"};
    juce::ToggleButton debugPlayToggle;
    juce::ToggleButton debugConsoleToggle;
    juce::ToggleButton profilerToggle;

    class ScrollableLabel : public juce::Label
    {
    public:
        std::function<void(int delta)> onScroll;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            int delta = (wheel.deltaY > 0) ? 1 : -1;
            if (onScroll) onScroll(delta);
        }
    };

    // Chart selector
    static constexpr int CHART_COUNT = 8;
    const juce::String chartNames[CHART_COUNT] = {
        "None", "Test", "Stress", "Sleepy Tea",
        "Further Side", "Scarlet", "Everything", "Akroasis"
    };
    int chartIndex = 1;
    ScrollableLabel chartSelectLabel;

    void layoutPanel(juce::Component* panel);
};

#endif
