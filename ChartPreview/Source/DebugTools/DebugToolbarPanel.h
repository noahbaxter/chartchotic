#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/PopupMenuButton.h"

class DebugToolbarPanel
{
public:
    DebugToolbarPanel();
    ~DebugToolbarPanel();

    PopupMenuButton& getButton() { return debugButton; }
    int getBpm() const { return bpm; }

    void setDebugPlay(bool playing);
    void adjustBpm(int delta);

    // Callbacks — the editor wires these
    std::function<void(bool playing)> onDebugPlayChanged;
    std::function<void(int bpm)> onDebugBpmChanged;
    std::function<void(bool)> onDebugNotesChanged;
    std::function<void(bool)> onDebugConsoleChanged;

private:
    PopupMenuButton debugButton{"Debug"};
    juce::ToggleButton debugPlayToggle;
    juce::ToggleButton debugNotesToggle;
    juce::ToggleButton debugConsoleToggle;

    // BPM control row: [- ] [120] [+ ]
    juce::TextButton bpmMinusButton{"-"};
    juce::TextButton bpmPlusButton{"+"};

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
    ScrollableLabel bpmValueLabel;

    int bpm = 120;

    void layoutPanel(juce::Component* panel);
    void updateBpmLabel();
};

#endif
