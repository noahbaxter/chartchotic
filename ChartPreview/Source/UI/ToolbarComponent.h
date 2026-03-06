#pragma once

#include <JuceHeader.h>
#include "ChartchoticLogo.h"
#include "Controls/CircleIconSelector.h"
#include "Controls/PillToggle.h"
#include "Controls/ValueStepper.h"
#include "Controls/SegmentedButtons.h"
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
    static constexpr float toolbarRatio = 0.09f;   // fraction of editor height (single strip)
    static constexpr int referenceHeight = 36;     // design reference for the strip portion
    static constexpr float stripFraction = 1.0f;   // strip is the full toolbar height
    static constexpr float logoFontRatio = 0.90f;  // logo font size as fraction of strip height

    int getStripHeight() const { return getHeight(); }

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
    std::function<void(bool)> onTrackChanged;
    std::function<void(bool)> onStrikelineChanged;
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

    void setLatencyOffsetRange(int minMs, int maxMs);

    // Expose for scroll-wheel handling in editor
    ValueStepper& getNoteSpeedStepper() { return noteSpeedStepper; }

private:
    juce::ValueTree& state;
    bool reaperMode = false;

    //==============================================================================
    // Top bar — always visible

    ChartchoticLogo logo;
    CircleIconSelector instrumentSelector;
    CircleIconSelector difficultySelector;
    ValueStepper noteSpeedStepper{"Speed"};

    //==============================================================================
    // Chart panel — Modifiers (contextual per instrument)

    PanelSectionHeader modifiersHeader{"Modifiers"};
    PillToggle starPowerToggle{"Star Power"};

    // Guitar modifiers
    PillToggle autoHopoToggle{"Auto HOPO"};
    ValueStepper hopoThresholdStepper{"Threshold"};
    int hopoThresholdIndex = 2; // 0-based into hopoThresholdLabels (default: "170 Tick")

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
    PillToggle trackToggle{"Track"};
    PillToggle strikelineToggle{"Strikeline"};

    //==============================================================================
    // Settings panel (gear icon) — Style, Playback, Sync

    PanelSectionHeader visualHeader{"Visual"};
    PanelSectionHeader highwayHeader{"Highway"};
    ValueStepper highwayLengthStepper{"Length"};
    ValueStepper backgroundStepper{"Background"};
    ValueStepper highwayTextureStepper{"Texture"};
    juce::Label textureScaleLabel;
    juce::Label textureOpacityLabel;
    ValueStepper textureScaleStepper{"Scale"};
    ValueStepper textureOpacityStepper{"Opacity"};
    ValueStepper gemScaleStepper{"Gem Size"};

    PanelSectionHeader syncHeader{"Sync"};
    ValueStepper syncOffsetStepper{"Calibration"};
    int syncOffsetMs = 0;
    int syncOffsetMin = 0;
    int syncOffsetMax = 2000;
    ValueStepper latencyStepper{"Latency"};
    int latencyIndex = 0;
    SegmentedButtons framerateButtons;

    //==============================================================================
    // Value data — TODO_RELEASE_DEFAULT: audit all defaults before tagging release

    int noteSpeed = 7;                    // TODO_RELEASE_DEFAULT
    int backgroundIndex = 0;              // TODO_RELEASE_DEFAULT (0 = Default)
    int highwayTextureIndex = -1;         // TODO_RELEASE_DEFAULT (-1 = None)
    int textureOpacityPct = 50;           // TODO_RELEASE_DEFAULT
    int gemScaleIndex = gemScaleDefault;  // TODO_RELEASE_DEFAULT
    int highwayLengthPct = hwLenDefaultPct; // TODO_RELEASE_DEFAULT

    juce::StringArray backgroundNames;
    juce::StringArray highwayTextureNames;

    static constexpr int texScaleValues[] = { 25, 50, 75, 100, 125, 150, 200, 300, 400 };
    static constexpr int texScaleDefault = 3;
    int texScaleIndex = texScaleDefault;

    static constexpr float gemScaleValues[] = { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f };
    static constexpr int gemScaleDefault = 5;

    static constexpr int hwLenMinPct = (int)(FAR_FADE_MIN * 100);
    static constexpr int hwLenMaxPct = (int)(FAR_FADE_MAX * 100);
    static constexpr int hwLenStepPct = 10;
    static constexpr int hwLenDefaultPct = (int)(FAR_FADE_DEFAULT * 100);

    //==============================================================================
    // Popup buttons

    PopupMenuButton chartButton{"Chart"};
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
    void initSettingsPanel();
    void layoutChartPanel(juce::Component* panel);
    void layoutSettingsPanel(juce::Component* panel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
