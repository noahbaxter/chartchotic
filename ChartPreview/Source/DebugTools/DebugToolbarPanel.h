#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/PopupMenuButton.h"
#include "../Utils/Utils.h"
#include "../Visual/Renderers/TrackRenderer.h"

class DebugToolbarPanel
{
public:
    DebugToolbarPanel(juce::ValueTree& state);
    ~DebugToolbarPanel();

    PopupMenuButton& getButton() { return debugButton; }
    void setDebugPlay(bool playing);
    void initDefaults(const TrackRenderer& trackRenderer);
    void setDrums(bool isDrums);

    // Callbacks -- the editor wires these
    std::function<void(bool playing)> onDebugPlayChanged;
    std::function<void(int)> onDebugChartChanged;
    std::function<void(bool)> onDebugConsoleChanged;
    std::function<void(bool)> onProfilerChanged;

    // Track layer tuning: onLayerChanged(layerIndex, scale, xOffset, yOffset)
    std::function<void(int, float, float, float)> onLayerChanged;

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

    // Per-layer tuning (Sidebars, Lane Lines, Strikeline, Connectors/Kick)
    static constexpr int NUM_LAYERS = 4;
    static constexpr const char* layerNames[NUM_LAYERS] = {"Side", "Lane", "Strike", "Conn"};

    using LayerTransform = TrackRenderer::LayerTransform;
    LayerTransform guitarStates[NUM_LAYERS];
    LayerTransform drumStates[NUM_LAYERS];
    LayerTransform* layerStates = guitarStates;

    ScrollableLabel layerScaleLabels[NUM_LAYERS];
    ScrollableLabel layerXLabels[NUM_LAYERS];
    ScrollableLabel layerYLabels[NUM_LAYERS];

    void fireLayer(int idx);
    void refreshLabels();
    void layoutPanel(juce::Component* panel);
};

#endif
