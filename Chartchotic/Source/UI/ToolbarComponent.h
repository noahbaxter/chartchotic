#pragma once

#include <JuceHeader.h>
#include "ChartchoticLogo.h"
#include "Controls/CircleIconSelector.h"
#include "Controls/PillToggle.h"
#include "Controls/CheckboxToggle.h"
#include "Controls/ValueStepper.h"
#include "Controls/SegmentedButtons.h"
#include "Controls/PanelSectionHeader.h"
#include "Controls/PopupMenuButton.h"
#include "../Utils/Utils.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "ControlConstants.h"
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
    std::function<void(bool enabled)> onAutoHopoChanged;
    std::function<void(int thresholdIndex)> onHopoThresholdChanged;
    std::function<void(bool)> onGemsChanged;
    std::function<void(bool)> onBarsChanged;
    std::function<void(bool)> onSustainsChanged;
    std::function<void(bool)> onLanesChanged;
    std::function<void(bool)> onGridlinesChanged;
    std::function<void(bool)> onHitIndicatorsChanged;
    std::function<void(bool)> onTrackChanged;
    std::function<void(bool)> onStrikelineChanged;
    std::function<void(bool)> onHighwayChanged;
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
    std::function<void(bool)> onShowFpsChanged;
    std::function<void()> onOpenBackgroundFolder;
    std::function<void()> onOpenTextureFolder;

    void setHighwayTextureList(const juce::StringArray& names)
    {
        highwayTextureNames = names;
        if (names.isEmpty())
        {
            highwayTextureIndex = 0;
            highwayTextureStepper.setDisplayValue("n/a");
            highwayTextureStepper.setValueClickable(true);
        }
    }

    void setBackgroundList(const juce::StringArray& names)
    {
        backgroundNames = names;
        if (names.isEmpty())
        {
            backgroundIndex = 0;
            backgroundStepper.setDisplayValue("n/a");
            backgroundStepper.setValueClickable(true);
        }
    }

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
    int hopoThresholdIndex = HOPO_THRESHOLD_DEFAULT;

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
    PillToggle strikelineToggle{"Strike"};
    PillToggle highwayToggle{"Hwy"};

    //==============================================================================
    // Settings panel (gear icon) — Style, Playback, Sync

    PanelSectionHeader visualHeader{"Visual"};
    PanelSectionHeader highwayHeader{"Highway"};
    ValueStepper highwayLengthStepper{"Length", "%"};
    ValueStepper backgroundStepper{"Background"};
    ValueStepper highwayTextureStepper{"Texture"};
    juce::Label textureScaleLabel;
    juce::Label textureOpacityLabel;
    ValueStepper textureScaleStepper{"Scale", "%"};
    ValueStepper textureOpacityStepper{"Opacity", "%"};
    ValueStepper gemScaleStepper{"Gem Size", "%"};

    PanelSectionHeader syncHeader{"Sync"};
    ValueStepper syncOffsetStepper{"Calibration", " ms"};
    int syncOffsetMs = CALIBRATION_DEFAULT;
    int syncOffsetMin = CALIBRATION_MIN_MS;
    int syncOffsetMax = CALIBRATION_MAX_MS;
    ValueStepper latencyStepper{"Latency"};
    int latencyIndex = LATENCY_DEFAULT - 1; // state is 1-based
    SegmentedButtons framerateButtons;
    CheckboxToggle showFpsToggle{"Show FPS"};

    //==============================================================================
    // Value data
    int noteSpeed = NOTE_SPEED_DEFAULT;
    int backgroundIndex = 0;
    int highwayTextureIndex = 0;
    int textureOpacityPct = TEX_OPACITY_DEFAULT;
    int gemScalePct = GEM_SCALE_DEFAULT_PCT;
    int highwayLengthPct = HWY_LENGTH_DEFAULT_PCT;

    juce::StringArray backgroundNames;
    juce::StringArray highwayTextureNames;

    int texScalePct = TEX_SCALE_DEFAULT_PCT;

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
