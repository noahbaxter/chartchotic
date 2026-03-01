#ifdef DEBUG

#include "DebugToolbarPanel.h"

//==============================================================================
// LaneCoordRow

void DebugToolbarPanel::LaneCoordRow::setup(const juce::String& name, float initPos, float initWidth,
                                             std::function<void(float pos, float width)> onChange)
{
    posVal = initPos;
    widthVal = initWidth;

    nameLabel.setText(name, juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont(juce::Font(10.0f));

    auto setupField = [](ScrollableLabel& field, float val) {
        field.setText(juce::String(val, 3), juce::dontSendNotification);
        field.setJustificationType(juce::Justification::centred);
        field.setColour(juce::Label::textColourId, juce::Colours::white);
        field.setFont(juce::Font(10.0f));
        field.setInterceptsMouseClicks(true, true);
    };

    setupField(posField, posVal);
    setupField(widthField, widthVal);

    posField.onScroll = [this, onChange](int delta) {
        posVal += delta * 0.001f;
        posField.setText(juce::String(posVal, 3), juce::dontSendNotification);
        if (onChange) onChange(posVal, widthVal);
    };
    widthField.onScroll = [this, onChange](int delta) {
        widthVal += delta * 0.001f;
        widthField.setText(juce::String(widthVal, 3), juce::dontSendNotification);
        if (onChange) onChange(posVal, widthVal);
    };
}

void DebugToolbarPanel::LaneCoordRow::setBounds(int x, int y, int totalWidth, int height)
{
    const int nameWidth = 32;
    const int fieldGap = 4;
    int fieldWidth = (totalWidth - nameWidth - fieldGap) / 2;

    nameLabel.setBounds(x, y, nameWidth, height);
    int fx = x + nameWidth;
    posField.setBounds(fx, y, fieldWidth, height);
    fx += fieldWidth + fieldGap;
    widthField.setBounds(fx, y, fieldWidth, height);
}

//==============================================================================
// DebugToolbarPanel

DebugToolbarPanel::DebugToolbarPanel(juce::ValueTree& state)
    : state(state)
{
    debugPlayToggle.setButtonText("Play");
    debugPlayToggle.onClick = [this]() {
        if (onDebugPlayChanged) onDebugPlayChanged(debugPlayToggle.getToggleState());
    };

    debugNotesToggle.setButtonText("Notes");
    debugNotesToggle.setToggleState(true, juce::dontSendNotification);
    debugNotesToggle.onClick = [this]() {
        if (onDebugNotesChanged) onDebugNotesChanged(debugNotesToggle.getToggleState());
    };

    debugConsoleToggle.setButtonText("Console");
    debugConsoleToggle.onClick = [this]() {
        if (onDebugConsoleChanged) onDebugConsoleChanged(debugConsoleToggle.getToggleState());
    };

    // BPM control row
    bpmMinusButton.onClick = [this]() { adjustBpm(-5); };
    bpmPlusButton.onClick = [this]() { adjustBpm(5); };

    bpmValueLabel.setText("120", juce::dontSendNotification);
    bpmValueLabel.setJustificationType(juce::Justification::centred);
    bpmValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmValueLabel.setInterceptsMouseClicks(true, true);
    bpmValueLabel.onScroll = [this](int delta) { adjustBpm(delta); };

    // Scale/position sliders
    setupCurveLabel(scaleLabel, "Scale", scaleVal, onScaleChanged);
    setupCurveLabel(yPosLabel, "Y.Pos", yPosVal, onYPositionChanged);


    // Section headers (collapsible)
    setupSectionHeader(curvatureHeader, "Curvature");
    setupSectionHeader(positionHeader, "Position");
    setupSectionHeader(clipHeader, "Clip");
    setupSectionHeader(laneHeader, "Lanes");
    setupSectionHeader(laneCoordsHeader, "Lane Coords");

    // Curvature sliders
    setupCurveLabel(sustainStartLabel, "S.Start", sustainStartVal, onSustainStartCurveChanged);
    setupCurveLabel(sustainEndLabel, "S.End", sustainEndVal, onSustainEndCurveChanged);
    setupCurveLabel(barSustainStartLabel, "B.Start", barSustainStartVal, onBarSustainStartCurveChanged);
    setupCurveLabel(barSustainEndLabel, "B.End", barSustainEndVal, onBarSustainEndCurveChanged);
    setupCurveLabel(noteCurveLabel, "N.Curve", noteCurveVal, onNoteCurveChanged);
    setupCurveLabel(barCurveLabel, "B.Curve", barCurveVal, onBarCurveChanged);

    // Position offset sliders
    setupCurveLabel(sustainStartOffsetLabel, "S.SPos", sustainStartOffsetVal, onSustainStartOffsetChanged);
    setupCurveLabel(sustainEndOffsetLabel, "S.EPos", sustainEndOffsetVal, onSustainEndOffsetChanged);
    setupCurveLabel(barSustainStartOffsetLabel, "B.SPos", barSustainStartOffsetVal, onBarSustainStartOffsetChanged);
    setupCurveLabel(barSustainEndOffsetLabel, "B.EPos", barSustainEndOffsetVal, onBarSustainEndOffsetChanged);

    // Clip sliders
    setupCurveLabel(sustainClipLabel, "S.Clip", sustainClipVal, onSustainClipChanged);
    setupCurveLabel(barSustainClipLabel, "B.Clip", barSustainClipVal, onBarSustainClipChanged);

    // Lane sliders
    setupCurveLabel(laneStartCurveLabel, "L.Start", laneStartCurveVal, onLaneStartCurveChanged);
    setupCurveLabel(laneEndCurveLabel, "L.End", laneEndCurveVal, onLaneEndCurveChanged);
    setupCurveLabel(laneInnerStartCurveLabel, "L.IStart", laneInnerStartCurveVal, onLaneInnerStartCurveChanged);
    setupCurveLabel(laneInnerEndCurveLabel, "L.IEnd", laneInnerEndCurveVal, onLaneInnerEndCurveChanged);
    setupCurveLabel(laneSideCurveLabel, "L.Side", laneSideCurveVal, onLaneSideCurveChanged);
    setupCurveLabel(laneStartOffsetLabel, "L.SPos", laneStartOffsetVal, onLaneStartOffsetChanged);
    setupCurveLabel(laneEndOffsetLabel, "L.EPos", laneEndOffsetVal, onLaneEndOffsetChanged);
    setupCurveLabel(laneClipLabel, "L.Clip", laneClipVal, onLaneClipChanged);

    // Lane coord rows — Guitar (normX1 = position, normWidth1 = width)
    const juce::String guitarNames[] = {"Open", "Grn", "Red", "Yel", "Blu", "Org"};
    const float guitarPos[] =   {0.179f, 0.228f, 0.330f, 0.445f, 0.558f, 0.674f};
    const float guitarWidth[] = {0.639f, 0.099f, 0.112f, 0.111f, 0.112f, 0.100f};
    for (int i = 0; i < GUITAR_LANE_COUNT; i++)
    {
        guitarCoordRows[i].setup(guitarNames[i], guitarPos[i], guitarWidth[i],
            [this, i](float pos, float width) {
                if (onGuitarLaneCoordChanged) onGuitarLaneCoordChanged(i, pos, width);
            });
    }

    // Lane coord rows — Drums
    const juce::String drumNames[] = {"Kick", "Red", "Yel", "Blu", "Grn"};
    const float drumPos[] =   {0.182f, 0.228f, 0.365f, 0.501f, 0.636f};
    const float drumWidth[] = {0.636f, 0.136f, 0.134f, 0.134f, 0.137f};
    for (int i = 0; i < DRUM_LANE_COUNT; i++)
    {
        drumCoordRows[i].setup(drumNames[i], drumPos[i], drumWidth[i],
            [this, i](float pos, float width) {
                if (onDrumLaneCoordChanged) onDrumLaneCoordChanged(i, pos, width);
            });
    }

    // Add panel children — basic controls
    debugButton.addPanelChild(&debugPlayToggle);
    debugButton.addPanelChild(&debugNotesToggle);
    debugButton.addPanelChild(&debugConsoleToggle);
    debugButton.addPanelChild(&bpmMinusButton);
    debugButton.addPanelChild(&bpmValueLabel);
    debugButton.addPanelChild(&bpmPlusButton);

    debugButton.addPanelChild(&scaleLabel);
    debugButton.addPanelChild(&yPosLabel);
    // Curvature section
    debugButton.addPanelChild(&curvatureHeader);
    debugButton.addPanelChild(&sustainStartLabel);
    debugButton.addPanelChild(&sustainEndLabel);
    debugButton.addPanelChild(&barSustainStartLabel);
    debugButton.addPanelChild(&barSustainEndLabel);
    debugButton.addPanelChild(&noteCurveLabel);
    debugButton.addPanelChild(&barCurveLabel);

    // Position section
    debugButton.addPanelChild(&positionHeader);
    debugButton.addPanelChild(&sustainStartOffsetLabel);
    debugButton.addPanelChild(&sustainEndOffsetLabel);
    debugButton.addPanelChild(&barSustainStartOffsetLabel);
    debugButton.addPanelChild(&barSustainEndOffsetLabel);

    // Clip section
    debugButton.addPanelChild(&clipHeader);
    debugButton.addPanelChild(&sustainClipLabel);
    debugButton.addPanelChild(&barSustainClipLabel);

    // Lane section
    debugButton.addPanelChild(&laneHeader);
    debugButton.addPanelChild(&laneStartCurveLabel);
    debugButton.addPanelChild(&laneEndCurveLabel);
    debugButton.addPanelChild(&laneInnerStartCurveLabel);
    debugButton.addPanelChild(&laneInnerEndCurveLabel);
    debugButton.addPanelChild(&laneSideCurveLabel);
    debugButton.addPanelChild(&laneStartOffsetLabel);
    debugButton.addPanelChild(&laneEndOffsetLabel);
    debugButton.addPanelChild(&laneClipLabel);

    // Lane coords section
    debugButton.addPanelChild(&laneCoordsHeader);
    for (int i = 0; i < GUITAR_LANE_COUNT; i++)
    {
        debugButton.addPanelChild(&guitarCoordRows[i].nameLabel);
        debugButton.addPanelChild(&guitarCoordRows[i].posField);
        debugButton.addPanelChild(&guitarCoordRows[i].widthField);
    }
    for (int i = 0; i < DRUM_LANE_COUNT; i++)
    {
        debugButton.addPanelChild(&drumCoordRows[i].nameLabel);
        debugButton.addPanelChild(&drumCoordRows[i].posField);
        debugButton.addPanelChild(&drumCoordRows[i].widthField);
    }

    debugButton.setPanelSize(150, 220);
    debugButton.onLayoutPanel = [this](juce::Component* panel) { layoutPanel(panel); };
}

DebugToolbarPanel::~DebugToolbarPanel()
{
    debugButton.dismissPanel();
}

void DebugToolbarPanel::setDebugPlay(bool playing)
{
    debugPlayToggle.setToggleState(playing, juce::dontSendNotification);
    if (onDebugPlayChanged) onDebugPlayChanged(playing);
}

void DebugToolbarPanel::adjustBpm(int delta)
{
    bpm = juce::jlimit(20, 300, bpm + delta);
    updateBpmLabel();
    if (onDebugBpmChanged) onDebugBpmChanged(bpm);
}

void DebugToolbarPanel::refreshLaneCoordVisibility()
{
    bool isDrums = isPart(state, Part::DRUMS);
    bool expanded = laneCoordsHeader.expanded;
    for (int i = 0; i < GUITAR_LANE_COUNT; i++)
    {
        bool vis = !isDrums && expanded;
        guitarCoordRows[i].nameLabel.setVisible(vis);
        guitarCoordRows[i].posField.setVisible(vis);
        guitarCoordRows[i].widthField.setVisible(vis);
    }
    for (int i = 0; i < DRUM_LANE_COUNT; i++)
    {
        bool vis = isDrums && expanded;
        drumCoordRows[i].nameLabel.setVisible(vis);
        drumCoordRows[i].posField.setVisible(vis);
        drumCoordRows[i].widthField.setVisible(vis);
    }
}

void DebugToolbarPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 4;
    const int headerGap = 6;
    int y = margin;
    int panelW = 150;
    int w = panelW - margin * 2;

    debugPlayToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    debugNotesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    debugConsoleToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    // BPM row: [ - ] [ 120 ] [ + ]
    const int btnW = 24;
    const int labelW = w - btnW * 2 - gap * 2;
    int x = margin;
    bpmMinusButton.setBounds(x, y, btnW, rowHeight);
    x += btnW + gap;
    bpmValueLabel.setBounds(x, y, labelW, rowHeight);
    x += labelW + gap;
    bpmPlusButton.setBounds(x, y, btnW, rowHeight);
    y += rowHeight + gap;

    // Scale / Y-position
    scaleLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    yPosLabel.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    // --- Curvature section ---
    y += headerGap;
    curvatureHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (curvatureHeader.expanded)
    {
        sustainStartLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        sustainEndLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barSustainStartLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barSustainEndLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        noteCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }
    sustainStartLabel.setVisible(curvatureHeader.expanded);
    sustainEndLabel.setVisible(curvatureHeader.expanded);
    barSustainStartLabel.setVisible(curvatureHeader.expanded);
    barSustainEndLabel.setVisible(curvatureHeader.expanded);
    noteCurveLabel.setVisible(curvatureHeader.expanded);
    barCurveLabel.setVisible(curvatureHeader.expanded);

    // --- Position section ---
    y += headerGap;
    positionHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (positionHeader.expanded)
    {
        sustainStartOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        sustainEndOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barSustainStartOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barSustainEndOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }
    sustainStartOffsetLabel.setVisible(positionHeader.expanded);
    sustainEndOffsetLabel.setVisible(positionHeader.expanded);
    barSustainStartOffsetLabel.setVisible(positionHeader.expanded);
    barSustainEndOffsetLabel.setVisible(positionHeader.expanded);

    // --- Clip section ---
    y += headerGap;
    clipHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (clipHeader.expanded)
    {
        sustainClipLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        barSustainClipLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }
    sustainClipLabel.setVisible(clipHeader.expanded);
    barSustainClipLabel.setVisible(clipHeader.expanded);

    // --- Lane section ---
    y += headerGap;
    laneHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (laneHeader.expanded)
    {
        laneStartCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneEndCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneInnerStartCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneInnerEndCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneSideCurveLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneStartOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneEndOffsetLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        laneClipLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }
    laneStartCurveLabel.setVisible(laneHeader.expanded);
    laneEndCurveLabel.setVisible(laneHeader.expanded);
    laneInnerStartCurveLabel.setVisible(laneHeader.expanded);
    laneInnerEndCurveLabel.setVisible(laneHeader.expanded);
    laneSideCurveLabel.setVisible(laneHeader.expanded);
    laneStartOffsetLabel.setVisible(laneHeader.expanded);
    laneEndOffsetLabel.setVisible(laneHeader.expanded);
    laneClipLabel.setVisible(laneHeader.expanded);

    // --- Lane Coords section ---
    y += headerGap;
    laneCoordsHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    refreshLaneCoordVisibility();

    bool isDrums = isPart(state, Part::DRUMS);
    if (laneCoordsHeader.expanded)
    {
        if (!isDrums)
        {
            for (int i = 0; i < GUITAR_LANE_COUNT; i++)
            {
                guitarCoordRows[i].setBounds(margin, y, w, rowHeight);
                y += rowHeight + gap;
            }
        }
        else
        {
            for (int i = 0; i < DRUM_LANE_COUNT; i++)
            {
                drumCoordRows[i].setBounds(margin, y, w, rowHeight);
                y += rowHeight + gap;
            }
        }
    }

    panel->setSize(panelW, y + margin);
}

void DebugToolbarPanel::setupSectionHeader(SectionHeader& header, const juce::String& text)
{
    header.setTitle(text);
    header.setJustificationType(juce::Justification::centred);
    header.setColour(juce::Label::textColourId, juce::Colours::grey);
    header.setFont(juce::Font(11.0f));
    header.setInterceptsMouseClicks(true, true);
    header.onToggle = [this]() {
        // Re-layout when any section is toggled
        if (debugButton.isPanelVisible())
        {
            debugButton.dismissPanel();
            debugButton.showPanel();
        }
    };
}

void DebugToolbarPanel::setupCurveLabel(ScrollableLabel& label, const juce::String& prefix,
                                         float& value, std::function<void(float)>& callback)
{
    label.setText(prefix + ": " + juce::String(value, 3), juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setInterceptsMouseClicks(true, true);
    label.onScroll = [&label, &value, &callback, prefix](int delta) {
        value += delta * 0.005f;
        label.setText(prefix + ": " + juce::String(value, 3), juce::dontSendNotification);
        if (callback) callback(value);
    };
}

void DebugToolbarPanel::updateBpmLabel()
{
    bpmValueLabel.setText(juce::String(bpm), juce::dontSendNotification);
}

#endif
