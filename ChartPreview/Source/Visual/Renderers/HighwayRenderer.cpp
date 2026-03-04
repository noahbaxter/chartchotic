/*
  ==============================================================================

    HighwayRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "HighwayRenderer.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/PositionConstants.h"

using namespace PositionConstants;

HighwayRenderer::HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter)
	: state(state),
	  midiInterpreter(midiInterpreter),
	  assetManager(),
	  animationRenderer(state, midiInterpreter)
{
}

HighwayRenderer::~HighwayRenderer()
{
}

void HighwayRenderer::paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying)
{
    ScopedPhaseMeasure totalMeasure(lastPhaseTiming.total_us, collectPhaseTiming);

    // Set the drawing area dimensions from the graphics context
    auto clipBounds = g.getClipBounds();
    width = clipBounds.getWidth();
    height = clipBounds.getHeight();

    // Calculate the total time window
    double windowTimeSpan = windowEndTime - windowStartTime;

    // Update sustain states for active animations (force-trigger if playing into active sustain)
    animationRenderer.updateSustainStates(sustainWindow, isPlaying);

    // Clear animations when paused to indicate gem is not being played
    if (!isPlaying)
    {
        animationRenderer.reset();
    }

    // Repopulate drawCallMap
    drawCallMap.clear();

    {
        ScopedPhaseMeasure m(lastPhaseTiming.notes_us, collectPhaseTiming);
        if (showNotes) drawNotesFromMap(g, trackWindow, windowStartTime, windowEndTime);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.sustains_us, collectPhaseTiming);
        drawSustainFromWindow(g, sustainWindow, windowStartTime, windowEndTime); // lane/sustain gating inside drawSustain()
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.gridlines_us, collectPhaseTiming);
        if (showGridlines) drawGridlinesFromMap(g, gridlines, windowStartTime, windowEndTime);
    }

    // Detect and add animations to drawCallMap (if enabled)
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_us, collectPhaseTiming);
        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow); }
            animationRenderer.renderToDrawCallMap(drawCallMap, width, height);
        }
    }

    // Draw layer by layer, then column by column within each layer
    {
        ScopedPhaseMeasure m(lastPhaseTiming.execute_us, collectPhaseTiming);
        if (collectPhaseTiming)
            lastPhaseTiming.layer_us.clear();

        for (const auto& drawOrder : drawCallMap)
        {
            ScopedPhaseMeasure lm(lastPhaseTiming.layer_us[drawOrder.first], collectPhaseTiming);
            for (const auto& column : drawOrder.second)
            {
                // Draw each layer from back to front
                for (auto it = column.second.rbegin(); it != column.second.rend(); ++it)
                {
                    (*it)(g);
                }
            }
        }
    }

    // Advance animation frames after rendering
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    if (hitIndicatorsEnabled)
    {
        animationRenderer.advanceFrames();
    }
}

void HighwayRenderer::drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor

        // Don't render notes in the past (below the strikeline at time 0)
        if (frameTime < 0.0) continue;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((frameTime - windowStartTime) / windowTimeSpan);

        drawFrame(frameItem.second, normalizedPosition, frameTime);
    }
}

void HighwayRenderer::drawGridlinesFromMap(juce::Graphics &g, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto &gridline : gridlines)
    {
        double gridlineTime = gridline.time;  // Time in seconds from cursor
        Gridline gridlineType = gridline.type;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan);

        if (normalizedPosition >= 0.0f && normalizedPosition <= 1.0f)
        {
            juce::Image *markerImage = assetManager.getGridlineImage(gridlineType);

            if (markerImage != nullptr)
            {
                drawCallMap[DrawOrder::GRID][0].push_back([=](juce::Graphics &g) { drawGridline(g, normalizedPosition, markerImage, gridlineType); });
            }
        }
    }
}

void HighwayRenderer::drawGridline(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType)
{
    if (!markerImage) return;
    
    float opacity = 1.0f;
    switch (gridlineType) {
        case Gridline::MEASURE: opacity = MEASURE_OPACITY; break;
        case Gridline::BEAT: opacity = BEAT_OPACITY; break;
        case Gridline::HALF_BEAT: opacity = HALF_BEAT_OPACITY; break;
    }

    if (isPart(state, Part::GUITAR))
    {
        juce::Rectangle<float> rect = glyphRenderer.getGuitarGridlineRect(position, width, height);
        draw(g, markerImage, rect, opacity);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        juce::Rectangle<float> rect = glyphRenderer.getDrumGridlineRect(position, width, height);
        draw(g, markerImage, rect, opacity);
    }
}


void HighwayRenderer::drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, frameTime);
        }
    }
}

void HighwayRenderer::drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime)
{
    juce::Rectangle<float> glyphRect;
    juce::Image* glyphImage;
    bool barNote;

    if (isPart(state, Part::GUITAR))
    {
        glyphRect = glyphRenderer.getGuitarGlyphRect(gemColumn, position, width, height);
        bool starPowerActive = state.getProperty("starPower");
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
        barNote = isBarNote(gemColumn, Part::GUITAR);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        glyphRect = glyphRenderer.getDrumGlyphRect(gemColumn, position, width, height);
        bool starPowerActive = state.getProperty("starPower");
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);
        barNote = isBarNote(gemColumn, Part::DRUMS);
    }

    // No glyph to draw
    if (glyphImage == nullptr)
    {
        return;
    }

    // Apply gem scale from state
    float gemScale = state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f;
    if (std::abs(gemScale - 1.0f) > 0.001f)
    {
        float cx = glyphRect.getCentreX();
        float cy = glyphRect.getCentreY();
        float newW = glyphRect.getWidth() * gemScale;
        float newH = glyphRect.getHeight() * gemScale;
        glyphRect = juce::Rectangle<float>(cx - newW / 2.0f, cy - newH / 2.0f, newW, newH);
    }

    float opacity = calculateOpacity(position);
    if (barNote)
    {
        drawCallMap[DrawOrder::BAR][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }
    else
    {
        drawCallMap[DrawOrder::NOTE][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }

    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        bool isDrumAccent = !isPart(state, Part::GUITAR) && gemWrapper.gem == Gem::TAP_ACCENT;
        juce::Rectangle<float> overlayRect = glyphRenderer.getOverlayGlyphRect(glyphRect, isDrumAccent);

        drawCallMap[DrawOrder::OVERLAY][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, overlayImage, overlayRect, opacity);
        });
    }
}


//==============================================================================
// Sustain Rendering

void HighwayRenderer::drawSustainFromWindow(juce::Graphics &g, const TimeBasedSustainWindow& sustainWindow, double windowStartTime, double windowEndTime)
{
    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, windowStartTime, windowEndTime);
    }
}

void HighwayRenderer::drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    bool isLane = (sustain.sustainType == SustainType::LANE);

    // Gate by render toggle
    if (isLane && !showLanes) return;
    if (!isLane && !showSustains) return;

    // Clip threshold: lanes extend further past strikeline than sustains
    float clipThreshold = isLane ? LANE_CLIP : (isBarNote(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS) ? BAR_SUSTAIN_CLIP : SUSTAIN_CLIP);

    // Don't render if fully past the clip threshold
    if (sustain.endTime < 0.0) return;

    // Clip sustain start to the clip threshold (in time space)
    double clipTime = clipThreshold * windowTimeSpan;
    double clippedStartTime = std::max(clipTime, sustain.startTime);

    // Position offsets differ for lanes vs sustains
    float startOffset, endOffset;
    if (isLane)
    {
        startOffset = LANE_START_OFFSET;
        endOffset = LANE_END_OFFSET;
    }
    else if (isBarNote(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS))
    {
        startOffset = BAR_SUSTAIN_START_OFFSET;
        endOffset = BAR_SUSTAIN_END_OFFSET;
    }
    else
    {
        startOffset = SUSTAIN_START_OFFSET;
        endOffset = SUSTAIN_END_OFFSET;
    }

    // Calculate normalized positions for start and end of sustain
    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan) + startOffset;
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan) + endOffset;

    // Only draw sustains that are visible in our window
    if (endPosition < 0.0f || startPosition > 1.0f) return;

    // Clamp to visible area (allow negative for lane extension past strikeline)
    float minPos = isLane ? clipThreshold : 0.0f;
    startPosition = std::max(minPos, startPosition);
    endPosition = std::min(1.0f, endPosition);

    // Get sustain color based on gem column and star power state
    bool starPowerActive = state.getProperty("starPower");
    bool shouldBeWhite = starPowerActive && sustain.gemType.starPower;
    auto colour = assetManager.getLaneColour(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS, shouldBeWhite);

    // Calculate opacity (average of start and end positions)
    float avgPosition = (startPosition + endPosition) / 2.0f;
    float baseOpacity = calculateOpacity(avgPosition);

    // Lanes and sustains render differently
    float opacity, sustainWidth;
    DrawOrder sustainDrawOrder;
    switch (sustain.sustainType) {
        case SustainType::LANE:
            opacity = LANE_OPACITY * baseOpacity;
            sustainWidth = (sustain.gemColumn == 0) ? LANE_OPEN_WIDTH : LANE_WIDTH;
            sustainDrawOrder = DrawOrder::LANE;
            break;
        case SustainType::SUSTAIN:
        default:
            opacity = SUSTAIN_OPACITY * baseOpacity;
            sustainWidth = (sustain.gemColumn == 0) ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            sustainDrawOrder = (sustain.gemColumn == 0) ? DrawOrder::BAR : DrawOrder::SUSTAIN;
            break;
    }

    drawCallMap[sustainDrawOrder][sustain.gemColumn].push_back([=](juce::Graphics &g) {
        drawPerspectiveSustainFlat(g, sustain.gemColumn, startPosition, endPosition, opacity, sustainWidth, colour, isLane);
    });
}

void HighwayRenderer::drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour, bool isLane)
{
    bool isDrums = isPart(state, Part::DRUMS);
    bool isBar = isBarNote(gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);

    // Get lane coordinates at start (near/strikeline) and end (far)
    auto startLane = isDrums
        ? PositionMath::getDrumLaneCoordinates(gemColumn, startPosition, width, height)
        : PositionMath::getGuitarLaneCoordinates(gemColumn, startPosition, width, height);
    auto endLane = isDrums
        ? PositionMath::getDrumLaneCoordinates(gemColumn, endPosition, width, height)
        : PositionMath::getGuitarLaneCoordinates(gemColumn, endPosition, width, height);

    // Calculate lane widths based on sustain width parameter
    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;

    // Corner positions (centered on lane)
    float startCenterX = (startLane.leftX + startLane.rightX) * 0.5f;
    float endCenterX = (endLane.leftX + endLane.rightX) * 0.5f;

    float startLeftX  = startCenterX - startWidth * 0.5f;
    float startRightX = startCenterX + startWidth * 0.5f;
    float endLeftX    = endCenterX - endWidth * 0.5f;
    float endRightX   = endCenterX + endWidth * 0.5f;

    float startY = startLane.centerY;
    float endY   = endLane.centerY;

    // Select curve amounts based on type
    float startCurve, endCurve;
    if (isLane)
    {
        startCurve = LANE_INNER_START_CURVE;
        endCurve = LANE_INNER_END_CURVE;
    }
    else if (isBar)
    {
        startCurve = BAR_SUSTAIN_START_CURVE;
        endCurve = BAR_SUSTAIN_END_CURVE;
    }
    else
    {
        startCurve = SUSTAIN_START_CURVE;
        endCurve = SUSTAIN_END_CURVE;
    }

    // Arc amount scales with fretboard width at each position
    // Get full fretboard width at start and end to scale curves proportionally
    auto startFretboard = isDrums
        ? PositionMath::getDrumLaneCoordinates(0, startPosition, width, height)
        : PositionMath::getGuitarLaneCoordinates(0, startPosition, width, height);
    auto endFretboard = isDrums
        ? PositionMath::getDrumLaneCoordinates(0, endPosition, width, height)
        : PositionMath::getGuitarLaneCoordinates(0, endPosition, width, height);

    float startFretboardWidth = startFretboard.rightX - startFretboard.leftX;
    float endFretboardWidth = endFretboard.rightX - endFretboard.leftX;

    // Lane-local arc: Y offset at the horizontal midpoint of the sustain edge
    // Positive curve = bow downward (toward strikeline), negative = bow upward
    float startArcY = startCurve * startFretboardWidth;
    float endArcY = endCurve * endFretboardWidth;

    // For lanes, also compute fretboard-wide parabolic Y offset
    // This makes adjacent lanes share the same overall curvature
    float laneStartParabolaY = 0.0f;
    float laneEndParabolaY = 0.0f;
    if (isLane)
    {
        // Where is this lane's center relative to the fretboard?
        // t = normalized position (0 = left edge, 1 = right edge)
        float startT = (startFretboardWidth > 0.0f) ? (startCenterX - startFretboard.leftX) / startFretboardWidth : 0.5f;
        float endT = (endFretboardWidth > 0.0f) ? (endCenterX - endFretboard.leftX) / endFretboardWidth : 0.5f;

        // Parabola: 4*t*(1-t) peaks at 1.0 when t=0.5, zero at edges
        laneStartParabolaY = LANE_START_CURVE * startFretboardWidth * 4.0f * startT * (1.0f - startT);
        laneEndParabolaY = LANE_END_CURVE * endFretboardWidth * 4.0f * endT * (1.0f - endT);
    }

    // Build the path with quadratic bezier curves for top and bottom edges
    juce::Path path;

    // Adjusted Y positions with parabolic offset
    float adjStartY = startY + laneStartParabolaY;
    float adjEndY = endY + laneEndParabolaY;

    // Start at bottom-left (near/strikeline, left edge)
    path.startNewSubPath(startLeftX, adjStartY);

    // Bottom edge (start/near): left → right with arc
    // Control point is at the horizontal midpoint, offset by startArcY
    float startMidX = (startLeftX + startRightX) * 0.5f;
    path.quadraticTo(startMidX, adjStartY + startArcY, startRightX, adjStartY);

    // Right edge: near → far (straight line, or curved if LANE_SIDE_CURVE != 0)
    if (isLane && std::abs(LANE_SIDE_CURVE) > 0.001f)
    {
        float sideMidY = (adjStartY + adjEndY) * 0.5f;
        float sideArc = LANE_SIDE_CURVE * (startFretboardWidth + endFretboardWidth) * 0.5f;
        path.quadraticTo(startRightX + sideArc, sideMidY, endRightX, adjEndY);
    }
    else
    {
        path.lineTo(endRightX, adjEndY);
    }

    // Top edge (end/far): right → left with arc
    float endMidX = (endLeftX + endRightX) * 0.5f;
    path.quadraticTo(endMidX, adjEndY + endArcY, endLeftX, adjEndY);

    // Left edge: far → near (straight line, or curved if LANE_SIDE_CURVE != 0)
    if (isLane && std::abs(LANE_SIDE_CURVE) > 0.001f)
    {
        float sideMidY = (adjStartY + adjEndY) * 0.5f;
        float sideArc = LANE_SIDE_CURVE * (startFretboardWidth + endFretboardWidth) * 0.5f;
        path.quadraticTo(endLeftX - sideArc, sideMidY, startLeftX, adjStartY);
    }
    else
    {
        path.closeSubPath();
    }

    // Fill the path directly — no offscreen buffer needed
    g.setColour(colour.withAlpha(opacity));
    g.fillPath(path);
}

