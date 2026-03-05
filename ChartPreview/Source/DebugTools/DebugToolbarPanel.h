#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/PopupMenuButton.h"
#include "../UI/SectionHeader.h"
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
    std::function<void(int, float, float, float)> onLayerChanged;
    std::function<void(float)> onTileStepChanged;
    std::function<void(float)> onTileScaleStepChanged;

private:
    juce::ValueTree& state;
    PopupMenuButton debugButton{"Debug"};

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

    // --- General controls ---
    juce::ToggleButton debugPlayToggle;
    juce::ToggleButton debugConsoleToggle;
    juce::ToggleButton profilerToggle;

    static constexpr int CHART_COUNT = 8;
    const juce::String chartNames[CHART_COUNT] = {
        "None", "Test", "Stress", "Sleepy Tea",
        "Further Side", "Scarlet", "Everything", "Akroasis"
    };
    int chartIndex = 1;
    ScrollableLabel chartSelectLabel;

    // --- Layers section ---
    SectionHeader layersHeader;
    static constexpr int NUM_LAYERS = 4;
    static constexpr const char* layerNames[NUM_LAYERS] = {"Side", "Lane", "Strike", "Conn"};

    using LayerTransform = TrackRenderer::LayerTransform;
    LayerTransform guitarStates[NUM_LAYERS];
    LayerTransform drumStates[NUM_LAYERS];
    LayerTransform* layerStates = guitarStates;

    ScrollableLabel layerScaleLabels[NUM_LAYERS];
    ScrollableLabel layerXLabels[NUM_LAYERS];
    ScrollableLabel layerYLabels[NUM_LAYERS];

    // --- Tiling section ---
    SectionHeader tilingHeader;
    float tileStepValue = 0.80f;
    float tileScaleStepValue = 0.50f;
    ScrollableLabel tileStepLabel;
    ScrollableLabel tileScaleStepLabel;

    void setupSectionHeader(SectionHeader& header, const juce::String& text);
    void setupScrollLabel(ScrollableLabel& label);
    void fireLayer(int idx);
    void refreshLabels();
    void layoutPanel(juce::Component* panel);
};

#endif
