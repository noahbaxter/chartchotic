#pragma once

#include <JuceHeader.h>
#include "PopupMenuButton.h"
#include "../Utils/Utils.h"
#ifdef DEBUG
#include "../DebugTools/DebugToolbarPanel.h"
#endif

class ToolbarComponent : public juce::Component
{
public:
    static constexpr int toolbarHeight = 36;

    ToolbarComponent(juce::ValueTree& state);
    ~ToolbarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Load control values from state
    void loadState();

    // Update visibility of instrument-specific controls
    void updateVisibility();

    // Set whether REAPER mode is active (hides latency menu)
    void setReaperMode(bool isReaper);

    //==============================================================================
    // Callbacks — the editor wires these

    std::function<void(int skillId)> onSkillChanged;
    std::function<void(int partId)> onPartChanged;
    std::function<void(int drumTypeId)> onDrumTypeChanged;
    std::function<void(int autoHopoId)> onAutoHopoChanged;
    std::function<void(bool)> onHitIndicatorsChanged;
    std::function<void(bool)> onStarPowerChanged;
    std::function<void(bool)> onKick2xChanged;
    std::function<void(bool)> onDynamicsChanged;
    std::function<void(int noteSpeed)> onNoteSpeedChanged;
    std::function<void(int framerateId)> onFramerateChanged;
    std::function<void(int latencyId)> onLatencyChanged;
    std::function<void(int offsetMs)> onLatencyOffsetChanged;
    std::function<void(bool useRed)> onRedBackgroundChanged;
    std::function<void(const juce::String& textureName)> onHighwayTextureChanged;
    std::function<void(float scale)> onGemScaleChanged;

    // Set available highway texture names (called by editor after scanning directory)
    void setHighwayTextureList(const juce::StringArray& names) { highwayTextureNames = names; }

    // Expose latencyOffsetInput for arrow-key handling in editor
    juce::TextEditor& getLatencyOffsetInput() { return latencyOffsetInput; }
    juce::Slider& getChartSpeedSlider() { return chartSpeedSlider; }

private:
    juce::ValueTree& state;
    bool reaperMode = false;

    // Always-visible toolbar controls
    juce::ComboBox skillMenu, partMenu, drumTypeMenu, autoHopoMenu;

    // Display popup children
    juce::ToggleButton hitIndicatorsToggle, starPowerToggle, kick2xToggle, dynamicsToggle, redBackgroundToggle;

    // Highway texture selector (scrollable label in Display popup)
    class HighwayTextureLabel : public juce::Label
    {
    public:
        std::function<void(int delta)> onScroll;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            int delta = (wheel.deltaY > 0) ? -1 : 1;
            if (onScroll) onScroll(delta);
        }
    };
    HighwayTextureLabel highwayTextureLabel;
    juce::StringArray highwayTextureNames;
    int highwayTextureIndex = -1; // -1 = None

    // Gem scale selector (scrollable label in Display popup)
    HighwayTextureLabel gemScaleLabel; // reuse scrollable label class
    static constexpr float gemScaleValues[] = { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f };
    static constexpr int gemScaleDefault = 5; // index of 1.0
    int gemScaleIndex = gemScaleDefault;

    // Settings popup children
    juce::Slider chartSpeedSlider;
    juce::Label chartSpeedLabel;
    juce::ComboBox framerateMenu, latencyMenu;
    juce::Label latencyOffsetLabel;

    // Custom TextEditor that passes arrow keys to parent
    class LatencyOffsetEditor : public juce::TextEditor
    {
    public:
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
            {
                // Walk up to the editor (grandparent: toolbar -> editor)
                if (auto* toolbar = getParentComponent())
                    if (auto* editor = toolbar->getParentComponent())
                        return editor->keyPressed(key);
            }
            return juce::TextEditor::keyPressed(key);
        }
    };
    LatencyOffsetEditor latencyOffsetInput;

    // Popup buttons
    PopupMenuButton displayButton{"Display"};
    PopupMenuButton settingsButton{"Settings"};

#ifdef DEBUG
    DebugToolbarPanel debugPanel{state};
public:
    DebugToolbarPanel& getDebugPanel() { return debugPanel; }
    void setDebugPlay(bool playing);
private:
#endif

    void initControls();
    void layoutDisplayPanel(juce::Component* panel);
    void layoutSettingsPanel(juce::Component* panel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
