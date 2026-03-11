#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/Controls/PopupMenuButton.h"
#include "../UI/SectionHeader.h"
#include "../Utils/Utils.h"
#include "../Visual/Utils/PositionConstants.h"
#include "../Visual/Utils/PositionMath.h"
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
    std::function<void(float)> onTextureScaleChanged;
    std::function<void(float)> onTextureOpacityChanged;
    std::function<void()> onPolyShadeChanged;
    std::function<void(bool)> onDebugColourChanged;
    std::function<void(bool)> onStretchChanged;

    // Current tuning values (read by applyTo)
    float guitarCurvature = PositionConstants::NOTE_CURVATURE;
    float drumCurvature = PositionConstants::NOTE_CURVATURE;
    float depthForeshorten = PositionConstants::NOTE_DEPTH_FORESHORTEN;

    // Base note/bar scale (ElementScale table: 2 rows × 2 cols)
    PositionConstants::ElementScale gemScale = PositionConstants::GEM_SCALE;
    PositionConstants::ElementScale barScale = PositionConstants::BAR_SCALE;

    // Hit animation scale (HitScale table: 2 rows × 3 cols)
    PositionConstants::HitScale hitGemScale = PositionConstants::HIT_GEM_SCALE;
    PositionConstants::HitScale hitBarScale = PositionConstants::HIT_BAR_SCALE;

    // Per-hit-type scales + flare config
    PositionConstants::HitTypeConfig hitTypeConfig;

    // Per-gem-type scales
    PositionConstants::GemTypeScales gemTypeScales;

    // Per-instrument offsets (Z + strike positions, table: 5+2 rows × 2 cols)
    PositionConstants::InstrumentOffsets guitarOffsets = PositionConstants::GUITAR_OFFSETS;
    PositionConstants::InstrumentOffsets drumOffsets = PositionConstants::DRUM_OFFSETS;

    // Per-column adjustments (Z offset, scale, W/H)
    PositionConstants::ColumnAdjust guitarColAdjust[6] = {
        PositionConstants::GUITAR_COL_ADJUST[0], PositionConstants::GUITAR_COL_ADJUST[1],
        PositionConstants::GUITAR_COL_ADJUST[2], PositionConstants::GUITAR_COL_ADJUST[3],
        PositionConstants::GUITAR_COL_ADJUST[4], PositionConstants::GUITAR_COL_ADJUST[5]};
    PositionConstants::ColumnAdjust drumColAdjust[5] = {
        PositionConstants::DRUM_COL_ADJUST[0], PositionConstants::DRUM_COL_ADJUST[1],
        PositionConstants::DRUM_COL_ADJUST[2], PositionConstants::DRUM_COL_ADJUST[3],
        PositionConstants::DRUM_COL_ADJUST[4]};

    // Overlay adjustments (mutable, for real-time tuning)
    PositionConstants::OverlayAdjust overlayAdjusts[PositionConstants::NUM_OVERLAY_TYPES];

    // Per-lane coordinates (mutable copies of BezierLaneCoords)
    static constexpr int GUITAR_LANES = 6;
    static constexpr int DRUM_LANES = 5;
    PositionConstants::NormalizedCoordinates guitarLaneCoords[GUITAR_LANES];
    PositionConstants::NormalizedCoordinates drumLaneCoords[DRUM_LANES];

    std::function<void()> onLaneCoordsChanged;

    // Lane shape config (tuneable)
    PositionConstants::LaneShapeConfig laneShape;

    // Perspective params (tuneable, synced to PositionMath::debugPerspParams)
    std::function<void()> onPerspectiveChanged;

private:
    juce::ValueTree& state;
    PopupMenuButton tuningButton{"T"};

    class ScrollableLabel : public juce::Label
    {
    public:
        std::function<void(int delta)> onScroll;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            int delta = (wheel.deltaY > 0) ? 1 : -1;
            if (onScroll) onScroll(delta);
        }
        void mouseDown(const juce::MouseEvent& e) override { lastDragY = e.y; }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            int diff = lastDragY - e.y;
            int steps = diff / dragPixelsPerStep;
            if (steps != 0 && onScroll)
            {
                for (int i = 0; i < std::abs(steps); i++)
                    onScroll(steps > 0 ? 1 : -1);
                lastDragY -= steps * dragPixelsPerStep;
            }
        }
    private:
        int lastDragY = 0;
        static constexpr int dragPixelsPerStep = 3;
    };

    // --- Perspective section ---
    SectionHeader perspectiveHeader;
    static constexpr int PERSP_COUNT = 7;
    ScrollableLabel perspLabels[PERSP_COUNT];

    // --- Track section (layers table + tiling) ---
    SectionHeader trackHeader;
    static constexpr int NUM_TRACK_LAYERS = 4;  // storage (matches TrackRenderer::NUM_LAYERS)
    static constexpr int NUM_DISPLAY_LAYERS = 3; // UI rows (excludes LANE_LINES)
    static constexpr int LAYER_COLS = 3;
    static constexpr int displayLayerMap[NUM_DISPLAY_LAYERS] = {0, 2, 3}; // SIDEBARS, STRIKELINE, CONNECTORS
    static constexpr const char* layerNames[NUM_DISPLAY_LAYERS] = {"Side", "Strike", "Conn"};
    static constexpr const char* layerColNames[LAYER_COLS] = {"S", "X", "Y"};

    using LayerTransform = TrackRenderer::LayerTransform;
    LayerTransform guitarStates[NUM_TRACK_LAYERS];
    LayerTransform drumStates[NUM_TRACK_LAYERS];
    LayerTransform* layerStates = guitarStates;

    juce::Label layerColHdrLabels[LAYER_COLS];
    juce::Label layerRowLabels[NUM_DISPLAY_LAYERS];
    ScrollableLabel layerParams[NUM_DISPLAY_LAYERS][LAYER_COLS];

    float tileStepValue = 0.80f;
    float tileScaleStepValue = 0.50f;
    ScrollableLabel tileStepLabel;
    ScrollableLabel tileScaleStepLabel;

    // Texture controls (read by DebugEditorController)
    float textureScaleValue = 1.0f;
    float textureOpacityValue = TEXTURE_OPACITY_DEFAULT;
    ScrollableLabel textureScaleLabel;
    ScrollableLabel textureOpacityLabel;
    // Debug visualization toggles
    juce::ToggleButton polyShadeToggle;
    juce::ToggleButton debugColourToggle;
    juce::ToggleButton stretchToggle;

    // Gridline position offset
    float gridlinePosOffset = PositionConstants::GRIDLINE_POS_OFFSET;
    ScrollableLabel gridPosLabel;

    void fireLayer(int idx);
    void refreshTrackLabels();

    // --- Curvature section ---
    SectionHeader curvatureHeader;
    ScrollableLabel guitarCurvLabel, drumCurvLabel, depthForeshortenLabel;

    // --- Base Scale table (2 rows × 2 cols: W, H) ---
    SectionHeader baseScaleHeader;
    static constexpr int BASE_SCALE_ROWS = 2;
    static constexpr int BASE_SCALE_COLS = 2;
    static constexpr const char* baseScaleRowNames[BASE_SCALE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* baseScaleColNames[BASE_SCALE_COLS] = {"W", "H"};
    juce::Label baseScaleColHdrLabels[BASE_SCALE_COLS];
    juce::Label baseScaleRowLabels[BASE_SCALE_ROWS];
    ScrollableLabel baseScaleParams[BASE_SCALE_ROWS][BASE_SCALE_COLS];

    // --- Gem type scale labels (backed by gemTypeScales struct) ---
    static constexpr int GEM_TYPE_COUNT = 10;
    ScrollableLabel gemTypeScaleLabels[GEM_TYPE_COUNT];

    // --- Hit Scale table (2 rows × 3 cols: S, W, H) ---
    SectionHeader hitScaleHeader;
    static constexpr int HIT_SCALE_ROWS = 2;
    static constexpr int HIT_SCALE_COLS = 3;
    static constexpr const char* hitScaleRowNames[HIT_SCALE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* hitScaleColNames[HIT_SCALE_COLS] = {"S", "W", "H"};
    juce::Label hitScaleColHdrLabels[HIT_SCALE_COLS];
    juce::Label hitScaleRowLabels[HIT_SCALE_ROWS];
    ScrollableLabel hitScaleParams[HIT_SCALE_ROWS][HIT_SCALE_COLS];

    // --- Hit type scale labels (backed by hitTypeConfig struct) ---
    static constexpr int HIT_TYPE_FLOAT_COUNT = 5;
    static constexpr int HIT_TYPE_BOOL_COUNT = 2;
    ScrollableLabel hitTypeScaleLabels[HIT_TYPE_FLOAT_COUNT];
    ScrollableLabel hitTypeBoolLabels[HIT_TYPE_BOOL_COUNT];

    // --- Z Offsets table (5 rows × 2 cols: Guitar, Drums) ---
    SectionHeader zOffsetsHeader;
    static constexpr int Z_ROWS = 5;
    static constexpr int Z_COLS = 2;
    static constexpr const char* zRowNames[Z_ROWS] = {"Grid", "Gem", "Bar", "HitN", "HitB"};
    static constexpr const char* zColNames[Z_COLS] = {"Gtr", "Drm"};
    juce::Label zColHdrLabels[Z_COLS];
    juce::Label zRowLabels[Z_ROWS];
    ScrollableLabel zParams[Z_ROWS][Z_COLS];

    // --- Strike Position table (2 rows × 2 cols: Guitar, Drums) ---
    SectionHeader strikeHeader;
    static constexpr int STRIKE_ROWS = 2;
    static constexpr int STRIKE_COLS = 2;
    static constexpr const char* strikeRowNames[STRIKE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* strikeColNames[STRIKE_COLS] = {"Gtr", "Drm"};
    juce::Label strikeColHdrLabels[STRIKE_COLS];
    juce::Label strikeRowLabels[STRIKE_ROWS];
    ScrollableLabel strikeParams[STRIKE_ROWS][STRIKE_COLS];

    // --- Guitar Cols section (notes table + lanes table) ---
    SectionHeader guitarColsHeader;
    static constexpr const char* guitarColNames[GUITAR_LANES] = {"Open", "Grn", "Red", "Yel", "Blu", "Org"};
    static constexpr int COL_NOTE_COLS = 5;   // Z, S1, S2, W, H
    static constexpr int COL_LANE_COLS = 2;   // X, W
    static constexpr const char* colNoteColNames[COL_NOTE_COLS] = {"Z", "S1", "S2", "W", "H"};
    static constexpr const char* colLaneColNames[COL_LANE_COLS] = {"X", "W"};
    juce::Label gcolSubNoteLabel;   // "Notes" sub-header
    juce::Label gcolSubLaneLabel;   // "Lanes" sub-header
    juce::Label gcolNoteHdrLabels[COL_NOTE_COLS];
    juce::Label gcolNoteRowLabels[GUITAR_LANES];
    ScrollableLabel gcolNoteParams[GUITAR_LANES][COL_NOTE_COLS];
    juce::Label gcolLaneHdrLabels[COL_LANE_COLS];
    juce::Label gcolLaneRowLabels[GUITAR_LANES];
    ScrollableLabel gcolLaneParams[GUITAR_LANES][COL_LANE_COLS];

    // --- Drum Cols section (notes table + lanes table) ---
    SectionHeader drumColsHeader;
    static constexpr const char* drumColNames[DRUM_LANES] = {"Kick", "Red", "Yel", "Blu", "Grn"};
    juce::Label dcolSubNoteLabel;   // "Notes" sub-header
    juce::Label dcolSubLaneLabel;   // "Lanes" sub-header
    juce::Label dcolNoteHdrLabels[COL_NOTE_COLS];
    juce::Label dcolNoteRowLabels[DRUM_LANES];
    ScrollableLabel dcolNoteParams[DRUM_LANES][COL_NOTE_COLS];
    juce::Label dcolLaneHdrLabels[COL_LANE_COLS];
    juce::Label dcolLaneRowLabels[DRUM_LANES];
    ScrollableLabel dcolLaneParams[DRUM_LANES][COL_LANE_COLS];

    // --- Lane Shape section (2 rows × 3 cols: Offset, Inner, Outer) ---
    SectionHeader laneShapeHeader;
    static constexpr int LANE_SHAPE_ROWS = 2;
    static constexpr int LANE_SHAPE_COLS = 3;
    static constexpr const char* laneShapeRowNames[LANE_SHAPE_ROWS] = {"Start", "End"};
    static constexpr const char* laneShapeColNames[LANE_SHAPE_COLS] = {"Pos", "Arc", "Bow"};
    juce::Label laneShapeColHdrLabels[LANE_SHAPE_COLS];
    juce::Label laneShapeRowLabels[LANE_SHAPE_ROWS];
    ScrollableLabel laneShapeParams[LANE_SHAPE_ROWS][LANE_SHAPE_COLS];
    void refreshLaneShapeLabels();

    // --- Overlay Adjust section ---
    SectionHeader overlayAdjustHeader;
    static constexpr int NUM_OVERLAY_TYPES = PositionConstants::NUM_OVERLAY_TYPES;
    static constexpr int OVERLAY_PARAMS = 5;
    static constexpr const char* overlayRowNames[NUM_OVERLAY_TYPES] = {"GTap", "DGho", "DAcc", "CGho", "CAcc"};
    static constexpr const char* overlayColNames[OVERLAY_PARAMS] = {"X", "Y", "W", "H", "S"};
    juce::Label overlayColHeaderLabels[OVERLAY_PARAMS];
    juce::Label overlayRowNameLabels[NUM_OVERLAY_TYPES];
    ScrollableLabel overlayParamLabels[NUM_OVERLAY_TYPES][OVERLAY_PARAMS];
    void refreshOverlayLabels();

    void refreshColLabels();
    void fireLaneChanged();

    // Table layout helpers
    void setupTableHeader(juce::Label& label);
    void setupSectionHeader(SectionHeader& header, const juce::String& text);
    void setupScrollLabel(ScrollableLabel& label);
    void refreshLabels();
    void layoutPanel(juce::Component* panel);
    void fireChanged();

    // Generic table layout: positions column headers, row names, and cells
    // Returns Y after the table. nameW = width of row name column.
    int layoutTable(int y, int x, int w, int rowHeight, int gap,
                    juce::Label* colHdrs, int numCols,
                    juce::Label* rowNames, ScrollableLabel* params,
                    int numRows, int paramStride, int nameW);
};

#endif
