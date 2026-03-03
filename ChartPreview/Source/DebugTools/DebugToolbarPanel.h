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
    int getBpm() const { return bpm; }

    void setDebugPlay(bool playing);
    void adjustBpm(int delta);

    // Callbacks — the editor wires these
    std::function<void(bool playing)> onDebugPlayChanged;
    std::function<void(int bpm)> onDebugBpmChanged;
    std::function<void(bool)> onDebugNotesChanged;
    std::function<void(bool)> onDebugConsoleChanged;
    std::function<void(float)> onSustainStartCurveChanged;
    std::function<void(float)> onSustainEndCurveChanged;
    std::function<void(float)> onBarSustainStartCurveChanged;
    std::function<void(float)> onBarSustainEndCurveChanged;
    std::function<void(float)> onNoteCurveChanged;
    std::function<void(float)> onBarCurveChanged;
    std::function<void(float)> onSustainStartOffsetChanged;
    std::function<void(float)> onSustainEndOffsetChanged;
    std::function<void(float)> onBarSustainStartOffsetChanged;
    std::function<void(float)> onBarSustainEndOffsetChanged;
    std::function<void(float)> onSustainClipChanged;
    std::function<void(float)> onBarSustainClipChanged;
    // Lane curve/offset/clip callbacks
    std::function<void(float)> onLaneStartCurveChanged;
    std::function<void(float)> onLaneEndCurveChanged;
    std::function<void(float)> onLaneInnerStartCurveChanged;
    std::function<void(float)> onLaneInnerEndCurveChanged;
    std::function<void(float)> onLaneSideCurveChanged;
    std::function<void(float)> onLaneStartOffsetChanged;
    std::function<void(float)> onLaneEndOffsetChanged;
    std::function<void(float)> onLaneClipChanged;
    // Lane coord callbacks: (col, pos, width)
    std::function<void(int, float, float)> onGuitarLaneCoordChanged;
    std::function<void(int, float, float)> onDrumLaneCoordChanged;
    // Highway scale/position callbacks
    std::function<void(float)> onScaleChanged;
    std::function<void(float)> onYPositionChanged;
    std::function<void(bool)> onSvgTracksChanged;
    std::function<void(float)> onSvgTrackScaleChanged;
    std::function<void(float)> onSvgTrackYOffsetChanged;
    std::function<void(float)> onSvgTrackOpacityChanged;
    std::function<void(float)> onSvgTrackFadeChanged;
    // Far fade callbacks
    std::function<void(float)> onFarFadeStartChanged;
    std::function<void(float)> onFarFadeEndChanged;
    std::function<void(float)> onFarFadeCurveChanged;
    // Gridline position offset callback
    std::function<void(float)> onGridlinePosOffsetChanged;

private:
    juce::ValueTree& state;
    PopupMenuButton debugButton{"Debug"};
    juce::ToggleButton debugPlayToggle;
    juce::ToggleButton debugNotesToggle;
    juce::ToggleButton debugConsoleToggle;
    juce::ToggleButton svgTracksToggle;

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

    ScrollableLabel svgScaleLabel;
    ScrollableLabel svgYOffsetLabel;
    ScrollableLabel svgOpacityLabel;
    ScrollableLabel svgFadeLabel;
    float svgScaleVal = 1.0f;
    float svgYOffsetVal = 0.215f;
    float svgOpacityVal = 0.7f;
    float svgFadeVal = 0.3f;

    // Far fade sliders
    ScrollableLabel farFadeLenLabel, farFadeEndLabel, farFadeCurveLabel;
    float farFadeLenVal   = 0.35f;
    float farFadeEndVal   = 1.05f;
    float farFadeCurveVal = 1.0f;

    // Gridline position offset slider
    ScrollableLabel gridlinePosOffsetLabel;
    float gridlinePosOffsetVal = -0.020f;

    int bpm = 120;

    // Sustain curve sliders
    ScrollableLabel sustainStartLabel, sustainEndLabel;
    ScrollableLabel barSustainStartLabel, barSustainEndLabel;
    float sustainStartVal = 0.015f, sustainEndVal = -0.010f;
    float barSustainStartVal = -0.015f, barSustainEndVal = -0.015f;

    // Note/bar curvature sliders
    ScrollableLabel noteCurveLabel, barCurveLabel;
    float noteCurveVal = -0.02f, barCurveVal = 0.0f;

    // Sustain position offset sliders
    ScrollableLabel sustainStartOffsetLabel, sustainEndOffsetLabel;
    ScrollableLabel barSustainStartOffsetLabel, barSustainEndOffsetLabel;
    float sustainStartOffsetVal = 0.0f, sustainEndOffsetVal = -0.050f;
    float barSustainStartOffsetVal = 0.0f, barSustainEndOffsetVal = -0.050f;

    // Strikeline clip sliders
    ScrollableLabel sustainClipLabel, barSustainClipLabel;
    float sustainClipVal = -0.015f, barSustainClipVal = -0.015f;

    // Lane sliders
    ScrollableLabel laneStartCurveLabel, laneEndCurveLabel;
    ScrollableLabel laneInnerStartCurveLabel, laneInnerEndCurveLabel;
    ScrollableLabel laneSideCurveLabel;
    ScrollableLabel laneStartOffsetLabel, laneEndOffsetLabel;
    ScrollableLabel laneClipLabel;
    float laneStartCurveVal = -0.025f, laneEndCurveVal = -0.035f;
    float laneInnerStartCurveVal = 0.040f, laneInnerEndCurveVal = -0.040f;
    float laneSideCurveVal = 0.0f;
    float laneStartOffsetVal = -0.010f, laneEndOffsetVal = -0.010f;
    float laneClipVal = -0.3f;

    //==========================================================================
    // Lane Coord Rows
    // Each row: name label + 2 ScrollableLabels (position + width)

    struct LaneCoordRow
    {
        juce::Label nameLabel;
        ScrollableLabel posField;   // normX1 (column position)
        ScrollableLabel widthField; // normWidth1 (column width)
        float posVal, widthVal;

        void setup(const juce::String& name, float initPos, float initWidth,
                   std::function<void(float pos, float width)> onChange);
        void setBounds(int x, int y, int totalWidth, int height);
    };

    static constexpr int GUITAR_LANE_COUNT = 6;
    static constexpr int DRUM_LANE_COUNT = 5;
    LaneCoordRow guitarCoordRows[GUITAR_LANE_COUNT];
    LaneCoordRow drumCoordRows[DRUM_LANE_COUNT];

    //==========================================================================
    // Collapsible sections

    // Clickable section header
    class SectionHeader : public juce::Label
    {
    public:
        bool expanded = false;
        std::function<void()> onToggle;
        void mouseDown(const juce::MouseEvent&) override
        {
            expanded = !expanded;
            updateText();
            if (onToggle) onToggle();
        }
        void setTitle(const juce::String& title)
        {
            sectionTitle = title;
            updateText();
        }
    private:
        juce::String sectionTitle;
        void updateText()
        {
            setText((expanded ? "- " : "+ ") + sectionTitle + " -", juce::dontSendNotification);
        }
    };

    // Scale/position sliders
    ScrollableLabel scaleLabel;
    float scaleVal = 0.74f;
    ScrollableLabel yPosLabel;
    float yPosVal = 0.815f;

    SectionHeader farFadeHeader, curvatureHeader, positionHeader, clipHeader, laneHeader, laneCoordsHeader;
    void setupSectionHeader(SectionHeader& header, const juce::String& text);

    void setupCurveLabel(ScrollableLabel& label, const juce::String& prefix, float& value,
                         std::function<void(float)>& callback);

    void layoutPanel(juce::Component* panel);
    void updateBpmLabel();
    void refreshLaneCoordVisibility();
};

#endif
