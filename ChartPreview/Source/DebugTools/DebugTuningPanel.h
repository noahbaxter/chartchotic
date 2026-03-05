#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/PopupMenuButton.h"
#include "../UI/SectionHeader.h"
#include "../Utils/Utils.h"
#include "../Visual/Utils/PositionConstants.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "../Visual/Renderers/TrackRenderer.h"

class SceneRenderer;

class DebugTuningPanel
{
public:
    DebugTuningPanel(juce::ValueTree& state);
    ~DebugTuningPanel();

    PopupMenuButton& getButton() { return tuningButton; }

    // Apply all tuning values to a SceneRenderer
    void applyTo(SceneRenderer& sr) const;

    // Initialize layer/tiling defaults from current TrackRenderer state
    void initDefaults(const TrackRenderer& trackRenderer);

    // Switch layer states between guitar/drums
    void setDrums(bool isDrums);

    // Callbacks
    std::function<void()> onTuningChanged;
    std::function<void(int, float, float, float)> onLayerChanged;
    std::function<void(float)> onTileStepChanged;
    std::function<void(float)> onTileScaleStepChanged;

    // Current tuning values (read by applyTo)
    float guitarCurvature = PositionConstants::NOTE_CURVATURE;
    float drumCurvature = PositionConstants::NOTE_CURVATURE;
    float gemW = PositionConstants::GEM_WIDTH_SCALE;
    float gemH = PositionConstants::GEM_HEIGHT_SCALE;
    float barW = PositionConstants::BAR_WIDTH_SCALE;
    float barH = PositionConstants::BAR_HEIGHT_SCALE;
    float hitGemScale = PositionConstants::HIT_GEM_SCALE;
    float hitBarScale = PositionConstants::HIT_BAR_SCALE;
    float hitGemW = PositionConstants::HIT_GEM_WIDTH_SCALE;
    float hitGemH = PositionConstants::HIT_GEM_HEIGHT_SCALE;
    float hitBarW = PositionConstants::HIT_BAR_WIDTH_SCALE;
    float hitBarH = PositionConstants::HIT_BAR_HEIGHT_SCALE;
    float hitGhostScale = PositionConstants::HIT_GHOST_SCALE;
    float hitAccentScale = PositionConstants::HIT_ACCENT_SCALE;
    float hitHopoScale = PositionConstants::HIT_HOPO_SCALE;
    float hitTapScale = PositionConstants::HIT_TAP_SCALE;
    float hitSpScale = PositionConstants::HIT_SP_SCALE;
    bool spWhiteFlare = SP_WHITE_FLARE_DEFAULT;
    bool tapPurpleFlare = TAP_PURPLE_FLARE_DEFAULT;

    float gemGhostScale = PositionConstants::GEM_GHOST_SCALE;
    float gemAccentScale = PositionConstants::GEM_ACCENT_SCALE;
    float gemHopoScale = PositionConstants::GEM_HOPO_SCALE;
    float gemTapScale = PositionConstants::GEM_TAP_SCALE;
    float gemSpScale = PositionConstants::GEM_SP_SCALE;

    // Per-instrument Z offsets (guitar)
    float gGridZ = 0.0f;
    float gGemZ = PositionConstants::GEM_Z_GUITAR;
    float gBarZ = PositionConstants::BAR_Z_GUITAR;
    float gHitGemZ = PositionConstants::HIT_GEM_Z_GUITAR;
    float gHitBarZ = PositionConstants::HIT_BAR_Z_GUITAR;
    float gStrikePosGem = PositionConstants::STRIKE_POS_GEM_GUITAR;
    float gStrikePosBar = PositionConstants::STRIKE_POS_BAR_GUITAR;

    // Per-instrument Z offsets (drums)
    float dGridZ = 0.0f;
    float dGemZ = PositionConstants::GEM_Z_DRUMS;
    float dBarZ = PositionConstants::BAR_Z_DRUMS;
    float dHitGemZ = PositionConstants::HIT_GEM_Z_DRUMS;
    float dHitBarZ = PositionConstants::HIT_BAR_Z_DRUMS;
    float dStrikePosGem = PositionConstants::STRIKE_POS_GEM_DRUMS;
    float dStrikePosBar = PositionConstants::STRIKE_POS_BAR_DRUMS;

    // Per-column Z offsets (drums only)
    float drumZ[5] = {};

private:
    juce::ValueTree& state;
    PopupMenuButton tuningButton{"Tuning"};

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

    // --- Layers section (moved from DebugToolbarPanel) ---
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

    // --- Tiling section (moved from DebugToolbarPanel) ---
    SectionHeader tilingHeader;
    float tileStepValue = 0.80f;
    float tileScaleStepValue = 0.50f;
    ScrollableLabel tileStepLabel;
    ScrollableLabel tileScaleStepLabel;

    void fireLayer(int idx);
    void refreshLayerLabels();

    // --- Curvature section ---
    SectionHeader curvatureHeader;
    ScrollableLabel guitarCurvLabel, drumCurvLabel;

    // --- Gem scale section ---
    SectionHeader gemScaleHeader;
    ScrollableLabel gemWLabel, gemHLabel, barWLabel, barHLabel;

    // --- Hit animation scale section ---
    SectionHeader hitScaleHeader;
    ScrollableLabel hitGemScaleLabel, hitBarScaleLabel;
    ScrollableLabel hitGemWLabel, hitGemHLabel, hitBarWLabel, hitBarHLabel;
    ScrollableLabel hitGhostScaleLabel, hitAccentScaleLabel, hitHopoScaleLabel, hitTapScaleLabel, hitSpScaleLabel;
    ScrollableLabel spWhiteFlareLabel, tapPurpleFlareLabel;

    // --- Gem dynamic scale labels ---
    ScrollableLabel gemGhostScaleLabel, gemAccentScaleLabel, gemHopoScaleLabel, gemTapScaleLabel, gemSpScaleLabel;

    // --- Guitar Z offsets ---
    SectionHeader guitarHeader;
    ScrollableLabel gGridZLabel, gGemZLabel, gBarZLabel, gHitGemZLabel, gHitBarZLabel;
    ScrollableLabel gStrikePosGemLabel, gStrikePosBarLabel;

    // --- Drum Z offsets ---
    SectionHeader drumHeader;
    ScrollableLabel dGridZLabel, dGemZLabel, dBarZLabel, dHitGemZLabel, dHitBarZLabel;
    ScrollableLabel dStrikePosGemLabel, dStrikePosBarLabel;
    ScrollableLabel drumColLabels[5];

    void setupSectionHeader(SectionHeader& header, const juce::String& text);
    void setupScrollLabel(ScrollableLabel& label);
    void refreshLabels();
    void layoutPanel(juce::Component* panel);
    void fireChanged();
};

#endif
