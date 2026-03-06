#pragma once

#include <JuceHeader.h>
#include "Controls/SegmentedButtons.h"
#include "Controls/PillToggle.h"
#include "Controls/ValueStepper.h"
#include "Controls/PanelSectionHeader.h"
#include "Controls/PopupMenuButton.h"
#include "../Utils/Utils.h"
#include "../Visual/Utils/DrawingConstants.h"
#ifdef DEBUG
#include "../DebugTools/DebugToolbarPanel.h"
#include "../DebugTools/DebugTuningPanel.h"
#endif

class ToolbarComponent : public juce::Component
{
public:
    static constexpr int toolbarHeight = 36;

    ToolbarComponent(juce::ValueTree& state);
    ~ToolbarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadState();
    void updateVisibility();
    void setReaperMode(bool isReaper);

    //==============================================================================
    // Callbacks — the editor wires these

    std::function<void(int skillId)> onSkillChanged;
    std::function<void(int partId)> onPartChanged;
    std::function<void(int drumTypeId)> onDrumTypeChanged;
    std::function<void(int autoHopoId)> onAutoHopoChanged;
    std::function<void(bool)> onGemsChanged;
    std::function<void(bool)> onBarsChanged;
    std::function<void(bool)> onSustainsChanged;
    std::function<void(bool)> onLanesChanged;
    std::function<void(bool)> onGridlinesChanged;
    std::function<void(bool)> onHitIndicatorsChanged;
    std::function<void(bool)> onStarPowerChanged;
    std::function<void(bool)> onKick2xChanged;
    std::function<void(bool)> onDynamicsChanged;
    std::function<void(int noteSpeed)> onNoteSpeedChanged;
    std::function<void(int framerateId)> onFramerateChanged;
    std::function<void(int latencyId)> onLatencyChanged;
    std::function<void(int offsetMs)> onLatencyOffsetChanged;
    std::function<void(const juce::String& bgName)> onBackgroundChanged;
    std::function<void(const juce::String& textureName)> onHighwayTextureChanged;
    std::function<void(float scale)> onTextureScaleChanged;
    std::function<void(float opacity)> onTextureOpacityChanged;
    std::function<void(float scale)> onGemScaleChanged;
    std::function<void(float length)> onHighwayLengthChanged;

    void setHighwayTextureList(const juce::StringArray& names) { highwayTextureNames = names; }
    void setBackgroundList(const juce::StringArray& names) { backgroundNames = names; }

    // Expose slider for mouse-over checks in editor
    juce::Slider& getLatencyOffsetSlider() { return latencyOffsetSlider; }

    void setLatencyOffsetRange(int minMs, int maxMs);

    // Expose for scroll-wheel handling in editor
    ValueStepper& getNoteSpeedStepper() { return noteSpeedStepper; }

private:
    juce::ValueTree& state;
    bool reaperMode = false;

    //==============================================================================
    // Top bar — always visible

    juce::Label titleLabel;
    SegmentedButtons skillButtons;
    SegmentedButtons partButtons;
    ValueStepper noteSpeedStepper{"Speed"};

    //==============================================================================
    // Chart panel — Modifiers (contextual per instrument)

    PanelSectionHeader modifiersHeader{"Modifiers"};
    PillToggle starPowerToggle{"Star Power"};

    // Guitar modifiers
    ValueStepper autoHopoStepper{"HOPO"};
    int autoHopoIndex = 0; // 0-based index into hopoModeLabels

    // Drum modifiers
    PillToggle dynamicsToggle{"Dynamics"};
    PillToggle kick2xToggle{"Kick 2x"};
    PillToggle cymbalsToggle{"Cymbals"};

    // Chart panel — Elements
    PanelSectionHeader elementsHeader{"Elements"};
    PillToggle gemsToggle{"Gems"};
    PillToggle barsToggle{"Bars"};
    PillToggle sustainsToggle{"Sustains"};
    PillToggle lanesToggle{"Lanes"};
    PillToggle gridlinesToggle{"Gridlines"};
    PillToggle hitIndicatorsToggle{"Hits"};

    //==============================================================================
    // View panel — Playback

    PanelSectionHeader playbackHeader{"Playback"};
    ValueStepper highwayLengthStepper{"Hwy Length"};

    // View panel — Style
    PanelSectionHeader styleHeader{"Style"};
    ValueStepper backgroundStepper{"Background"};
    ValueStepper gemScaleStepper{"Gem Size"};
    ValueStepper highwayTextureStepper{"Texture"};
    juce::Label textureScaleLabel;
    juce::Label textureOpacityLabel;
    ValueStepper textureScaleStepper{"Scale"};
    ValueStepper textureOpacityStepper{"Opacity"};

    // View panel — Performance
    PanelSectionHeader performanceHeader{"Performance"};
    ValueStepper framerateStepper{"Framerate"};
    int framerateIndex = 3; // 0-based: 15, 30, 60, Native

    //==============================================================================
    // Settings panel (gear icon) — Sync

    ValueStepper latencyStepper{"Latency"};
    int latencyIndex = 0;
    juce::Label latencyOffsetLabel;
    juce::Slider latencyOffsetSlider;

    //==============================================================================
    // Value data

    int noteSpeed = 7;

    juce::StringArray backgroundNames;
    int backgroundIndex = 0; // 0 = Default, then folder entries

    juce::StringArray highwayTextureNames;
    int highwayTextureIndex = -1;

    static constexpr int texScaleValues[] = { 25, 50, 75, 100, 125, 150, 200, 300, 400 };
    static constexpr int texScaleDefault = 3;
    int texScaleIndex = texScaleDefault;
    int textureOpacityPct = 45;

    static constexpr float gemScaleValues[] = { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f };
    static constexpr int gemScaleDefault = 5;
    int gemScaleIndex = gemScaleDefault;

    static constexpr int hwLenMinPct = (int)(FAR_FADE_MIN * 100);
    static constexpr int hwLenMaxPct = (int)(FAR_FADE_MAX * 100);
    static constexpr int hwLenStepPct = 10;
    static constexpr int hwLenDefaultPct = (int)(FAR_FADE_DEFAULT * 100);
    int highwayLengthPct = hwLenDefaultPct;

    //==============================================================================
    // Popup buttons

    PopupMenuButton chartButton{"Chart"};
    PopupMenuButton viewButton{"View"};
    PopupMenuButton settingsButton{juce::CharPointer_UTF8("\xe2\x9a\x99")}; // ⚙

    //==============================================================================

#ifdef DEBUG
    DebugToolbarPanel debugPanel{state};
    DebugTuningPanel tuningPanel{state};
public:
    DebugToolbarPanel& getDebugPanel() { return debugPanel; }
    DebugTuningPanel& getTuningPanel() { return tuningPanel; }
    void setDebugPlay(bool playing);
private:
#endif

    void initTopBar();
    void initChartPanel();
    void initViewPanel();
    void initSettingsPanel();
    void layoutChartPanel(juce::Component* panel);
    void layoutViewPanel(juce::Component* panel);
    void layoutSettingsPanel(juce::Component* panel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
