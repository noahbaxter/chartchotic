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

    // Draw highway texture overlay (between track image and gridlines)
    // When skipHighwayTexture is set, the editor draws the texture before the SVG track
    if (highwayTexture.isValid() && !skipHighwayTexture)
    {
        drawHighwayTexture(g);
    }

    // Repopulate drawCallMap
    drawCallMap.clear();
    drawNotesFromMap(g, trackWindow, windowStartTime, windowEndTime);
    drawSustainFromWindow(g, sustainWindow, windowStartTime, windowEndTime);
    drawGridlinesFromMap(g, gridlines, windowStartTime, windowEndTime);

    // Detect and add animations to drawCallMap (if enabled)
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    if (hitIndicatorsEnabled)
    {
        if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow); }
        animationRenderer.renderToDrawCallMap(drawCallMap, width, height);
    }

    // Draw layer by layer, then column by column within each layer
    bool calledAfterLanes = false;
    for (const auto& drawOrder : drawCallMap)
    {
        for (const auto& column : drawOrder.second)
        {
            // Draw each layer from back to front
            for (auto it = column.second.rbegin(); it != column.second.rend(); ++it)
            {
                (*it)(g);
            }
        }

        // Insert SVG track overlay after lanes but before notes/sustains/gridlines
        if (!calledAfterLanes && drawOrder.first >= DrawOrder::LANE && onAfterLanes)
        {
            onAfterLanes(g);
            calledAfterLanes = true;
        }
    }
    // If no LANE layer existed, still call it
    if (!calledAfterLanes && onAfterLanes)
        onAfterLanes(g);

    // Advance animation frames after rendering
    if (hitIndicatorsEnabled)
    {
        animationRenderer.advanceFrames();
    }
}

void HighwayRenderer::drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    // When hit indicators are off, let notes scroll past the strikeline to match
    // the lane/gridline range (HIGHWAY_POS_START = -0.3 in normalized position space)
    double minFrameTime = hitIndicatorsEnabled ? 0.0 : (HIGHWAY_POS_START * windowTimeSpan);

    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor

        // Don't render notes too far past the strikeline
        // When hit indicators are off, allow notes to scroll slightly past for visual feedback
        if (frameTime < minFrameTime) continue;

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
        double gridlineTime = gridline.time - GRIDLINE_TIME_OFFSET;
        Gridline gridlineType = gridline.type;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan);

        if (normalizedPosition >= HIGHWAY_POS_START && normalizedPosition <= farFadeEnd)
        {
            juce::Image *markerImage = assetManager.getGridlineImage(gridlineType);

            if (markerImage != nullptr)
            {
                float fadeOpacity = calculateOpacity(normalizedPosition);
                drawCallMap[DrawOrder::GRID][0].push_back([=](juce::Graphics &g) { drawGridline(g, normalizedPosition, markerImage, gridlineType, fadeOpacity); });
            }
        }
    }
}

void HighwayRenderer::drawGridline(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType, float fadeOpacity)
{
    if (!markerImage) return;

    float opacity = 1.0f;
    switch (gridlineType) {
        case Gridline::MEASURE: opacity = MEASURE_OPACITY; break;
        case Gridline::BEAT: opacity = BEAT_OPACITY; break;
        case Gridline::HALF_BEAT: opacity = HALF_BEAT_OPACITY; break;
    }
    opacity *= fadeOpacity;

    bool isDrums = isPart(state, Part::DRUMS);
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    auto edge = getColumnEdge(position, fbCoords, 1.1f);

    float gridWidth = edge.rightX - edge.leftX;
    float gridHeight = gridWidth / getPerspectiveParams().barNoteHeightRatio;
    float centerX = (edge.leftX + edge.rightX) * 0.5f;

    juce::Rectangle<float> rect(centerX - gridWidth * 0.5f,
                                 edge.centerY - gridHeight * 0.5f,
                                 gridWidth, gridHeight);
    draw(g, markerImage, rect, opacity);
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
    bool isDrums = isPart(state, Part::DRUMS);
    bool barNote = isBarNote(gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);

    // Look up column coords from existing tables
    NormalizedCoordinates colCoords;
    if (isDrums)
        colCoords = barNote ? PositionMath::getDrumKickCoords()
                            : PositionMath::getDrumPadCoords(gemColumn);
    else
        colCoords = barNote ? PositionMath::getGuitarOpenNoteCoords()
                            : PositionMath::getGuitarNoteCoords(gemColumn);

    float gemScale = state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f;
    float sizeScale = (barNote ? BAR_SIZE : GEM_SIZE) * gemScale;
    float adjustedPosition = barNote ? position + BAR_NOTE_TIME_OFFSET : position;
    auto edge = getColumnEdge(adjustedPosition, colCoords, sizeScale, FRETBOARD_SCALE);

    // Compute glyph rect from edge
    float colWidth = edge.rightX - edge.leftX;
    auto perspParams = getPerspectiveParams();
    float colHeight = colWidth / (barNote ? perspParams.barNoteHeightRatio
                                          : perspParams.regularNoteHeightRatio);
    juce::Rectangle<float> glyphRect(edge.leftX, edge.centerY - colHeight * 0.5f,
                                      colWidth, colHeight);

    // Get glyph image
    bool starPowerActive = state.getProperty("starPower");
    juce::Image* glyphImage = isDrums
        ? assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive)
        : assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);

    if (glyphImage == nullptr) return;

    float opacity = calculateOpacity(position);

    // Fretboard arc: parabola centered on fretboard, peak toward strikeline
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    auto fbEdge = getColumnEdge(adjustedPosition, fbCoords, 1.0f);
    float fbCenterX = (fbEdge.leftX + fbEdge.rightX) * 0.5f;
    float fbHalfWidth = (fbEdge.rightX - fbEdge.leftX) * 0.5f;
    float arcHeight = fbHalfWidth * 2.0f * (barNote ? barCurvature : noteCurvature);

    if (barNote)
    {
        drawCallMap[DrawOrder::BAR][gemColumn].push_back([=](juce::Graphics &g) {
            drawCurved(g, glyphImage, glyphRect, opacity, fbCenterX, fbHalfWidth, arcHeight);
        });
    }
    else
    {
        drawCallMap[DrawOrder::NOTE][gemColumn].push_back([=](juce::Graphics &g) {
            drawCurved(g, glyphImage, glyphRect, opacity, fbCenterX, fbHalfWidth, arcHeight);
        });
    }

    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isDrums ? Part::DRUMS : Part::GUITAR);
    if (overlayImage != nullptr)
    {
        bool isDrumAccent = isDrums && gemWrapper.gem == Gem::TAP_ACCENT;
        juce::Rectangle<float> overlayRect = glyphRenderer.getOverlayGlyphRect(glyphRect, isDrumAccent);

        drawCallMap[DrawOrder::OVERLAY][gemColumn].push_back([=](juce::Graphics &g) {
            drawCurved(g, overlayImage, overlayRect, opacity, fbCenterX, fbHalfWidth, arcHeight);
        });
    }
}


// Draw image curved along the fretboard-wide arc. Renders strips into a
// supersampled offscreen image for clean compositing.
void HighwayRenderer::drawCurved(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> rect,
                                 float opacity, float fbCenterX, float fbHalfWidth, float arcHeight)
{
    if (arcHeight == 0.0f || fbHalfWidth == 0.0f) { draw(g, image, rect, opacity); return; }

    constexpr int S = NOTE_RENDER_SCALE;
    constexpr int STRIPS = 12;
    float absArc = std::abs(arcHeight);
    int offW = ((int)std::ceil(rect.getWidth()) + 2) * S;
    int offH = ((int)std::ceil(rect.getHeight() + absArc) + 2) * S;
    if (offW <= 0 || offH <= 0) return;

    juce::Image offscreen(juce::Image::ARGB, offW, offH, true);
    {
        juce::Graphics og(offscreen);
        og.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        float stripW = rect.getWidth() * S / STRIPS;
        int imgW = image->getWidth();
        float srcStripW = (float)imgW / STRIPS;
        float baseY = ((arcHeight > 0.0f) ? absArc : 0.0f) * S;

        for (int i = 0; i < STRIPS; i++)
        {
            float stripCenterX = rect.getX() + (rect.getWidth() / STRIPS) * (i + 0.5f);
            float dist = (stripCenterX - fbCenterX) / fbHalfWidth;
            float yOff = arcHeight * (1.0f - dist * dist) * S;

            int srcX = (int)(srcStripW * i);
            int srcEnd = std::min((int)(srcStripW * (i + 1) + 0.5f), imgW);

            og.drawImage(*image,
                         (int)(stripW * i), (int)(baseY + yOff),
                         (int)std::ceil(stripW), (int)std::ceil(rect.getHeight() * S),
                         srcX, 0, srcEnd - srcX, image->getHeight());
        }
    }

    float drawY = rect.getY() - ((arcHeight > 0.0f) ? absArc : 0.0f);
    g.setOpacity(opacity);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    juce::Rectangle<float> destRect(rect.getX() - 1, drawY,
                                     (float)offW / S, (float)offH / S);
    g.drawImage(offscreen, destRect);
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
    bool isDrums = isPart(state, Part::DRUMS);
    bool isBar = isBarNote(sustain.gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);
    bool isLane = sustain.sustainType == SustainType::LANE;

    // Don't render sustains that are fully past the strikeline (small grace period)
    // Lanes get a much wider window since they extend past strikeline
    if (!isLane && sustain.endTime < -0.15) return;

    // Clip sustain start to slightly past the strikeline for smooth exit
    // Lanes don't clip — they extend to laneClip in position space later
    double startTime = sustain.startTime;
    if (isLane)
        startTime += LANE_START_OFFSET;
    double clippedStartTime = isLane ? startTime : std::max(-0.15, startTime);

    // Calculate normalized positions for start and end of sustain
    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan);
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan);

    // Apply position offsets (nudge start/end in normalized position space)
    if (isLane) {
        startPosition += laneStartOffset;
        endPosition   += laneEndOffset;
    } else {
        startPosition += isBar ? barSustainStartOffset : sustainStartOffset;
        endPosition   += isBar ? barSustainEndOffset   : sustainEndOffset;
    }

    if (isLane) {
        // Lanes extend past strikeline (tunable, well beyond strikeline)
        if (endPosition < laneClip || startPosition > farFadeEnd) return;
        startPosition = std::max(laneClip, startPosition);
        endPosition = std::min(farFadeEnd, endPosition);
    } else {
        // Strikeline clip (separately tunable from position offset)
        float clip = isBar ? barSustainClip : sustainClip;
        if (endPosition < clip || startPosition > farFadeEnd) return;
        startPosition = std::max(clip, startPosition);
        endPosition = std::min(farFadeEnd, endPosition);
    }

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
    // Look up lane coords (separate table from glyph — slightly different widths)
    bool isDrums = isPart(state, Part::DRUMS);
    NormalizedCoordinates colCoords;
    float laneScale;
    if (isDrums) {
        bool isKick = (gemColumn == 0 || gemColumn == 6);
        colCoords = isKick ? drumLaneCoordsLocal[0] : drumLaneCoordsLocal[gemColumn];
        laneScale = isKick ? BAR_SIZE : GEM_SIZE;
    } else {
        colCoords = guitarLaneCoordsLocal[gemColumn];
        laneScale = (gemColumn == 0) ? BAR_SIZE : GEM_SIZE;
    }

    auto startLane = getColumnEdge(startPosition, colCoords, laneScale, FRETBOARD_SCALE);
    auto endLane = getColumnEdge(endPosition, colCoords, laneScale, FRETBOARD_SCALE);

    // Calculate lane widths based on sustain width parameter
    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;

    float startCenterX = (startLane.leftX + startLane.rightX) / 2.0f;
    float endCenterX = (endLane.leftX + endLane.rightX) / 2.0f;

    // Fretboard arc at start and end positions
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    auto startFb = getColumnEdge(startPosition, fbCoords, 1.0f);
    auto endFb = getColumnEdge(endPosition, fbCoords, 1.0f);
    float startFbHalfW = (startFb.rightX - startFb.leftX) * 0.5f;
    float endFbHalfW = (endFb.rightX - endFb.leftX) * 0.5f;
    bool isBar = isBarNote(gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);
    float startCurveVal, endCurveVal;
    if (isLane) {
        startCurveVal = laneStartCurve;
        endCurveVal   = laneEndCurve;
    } else {
        startCurveVal = isBar ? barSustainStartCurve : sustainStartCurve;
        endCurveVal   = isBar ? barSustainEndCurve   : sustainEndCurve;
    }
    float startArc = startFbHalfW * 2.0f * startCurveVal;
    float endArc   = endFbHalfW   * 2.0f * endCurveVal;

    // Fretboard center X at start and end positions
    float startFbCenterX = (startFb.leftX + startFb.rightX) * 0.5f;
    float endFbCenterX   = (endFb.leftX   + endFb.rightX)   * 0.5f;

    // For lanes: compute Y offsets at each corner from a fretboard-wide parabola
    // so adjacent lanes share the same curve. For sustains: use lane-local quadratic.
    auto parabolicOffset = [](float x, float centerX, float halfW, float arc) -> float {
        if (halfW == 0.0f) return 0.0f;
        float dist = (x - centerX) / halfW;
        return arc * (1.0f - dist * dist);
    };

    // Build curved path
    juce::Path path;

    if (isLane) {
        // Lane corners
        float startLeftX  = startCenterX - startWidth / 2.0f;
        float startRightX = startCenterX + startWidth / 2.0f;
        float endLeftX    = endCenterX   - endWidth   / 2.0f;
        float endRightX   = endCenterX   + endWidth   / 2.0f;

        // Fretboard-wide parabolic Y offsets (aligns lanes to gridline curve)
        float startLeftY  = startLane.centerY + parabolicOffset(startLeftX,  startFbCenterX, startFbHalfW, startArc);
        float startRightY = startLane.centerY + parabolicOffset(startRightX, startFbCenterX, startFbHalfW, startArc);
        float startMidY   = startLane.centerY + parabolicOffset(startCenterX, startFbCenterX, startFbHalfW, startArc);
        float endLeftY    = endLane.centerY   + parabolicOffset(endLeftX,    endFbCenterX,   endFbHalfW,   endArc);
        float endRightY   = endLane.centerY   + parabolicOffset(endRightX,   endFbCenterX,   endFbHalfW,   endArc);
        float endMidY     = endLane.centerY   + parabolicOffset(endCenterX,  endFbCenterX,   endFbHalfW,   endArc);

        // Individual lane-local arc (additional curve within the lane's own width)
        float startInnerArc = startFbHalfW * 2.0f * laneInnerStartCurve;
        float endInnerArc   = endFbHalfW   * 2.0f * laneInnerEndCurve;

        // Precompute side curve midpoint (used by both left and right edges)
        float sideMidCenterX = 0.0f, sideMidHalfW = 0.0f, sideMidY = 0.0f, sideOffset = 0.0f;
        if (laneSideCurve != 0.0f) {
            float midPos = (startPosition + endPosition) * 0.5f;
            auto midEdge = getColumnEdge(midPos, colCoords, laneScale, FRETBOARD_SCALE);
            sideMidHalfW = (midEdge.rightX - midEdge.leftX) * sustainWidth * 0.5f;
            sideMidCenterX = (midEdge.leftX + midEdge.rightX) * 0.5f;
            sideMidY = midEdge.centerY;
            sideOffset = (startFbHalfW + endFbHalfW) * laneSideCurve;
        }

        // Bottom edge (start, near strikeline): left → right
        path.startNewSubPath(startLeftX, startLeftY);
        path.quadraticTo(startCenterX, startMidY + startInnerArc, startRightX, startRightY);

        // Right edge
        if (laneSideCurve != 0.0f) {
            path.quadraticTo(sideMidCenterX + sideMidHalfW + sideOffset, sideMidY,
                             endRightX, endRightY);
        } else {
            path.lineTo(endRightX, endRightY);
        }

        // Top edge (end, far from strikeline): right → left
        path.quadraticTo(endCenterX, endMidY + endInnerArc, endLeftX, endLeftY);

        // Left edge
        if (laneSideCurve != 0.0f) {
            path.quadraticTo(sideMidCenterX - sideMidHalfW - sideOffset, sideMidY,
                             startLeftX, startLeftY);
        }
        path.closeSubPath();
    } else {
        // Sustains: lane-local quadratic (original behavior)
        path.startNewSubPath(startCenterX - startWidth / 2.0f, startLane.centerY);
        path.quadraticTo(startCenterX, startLane.centerY + 2.0f * startArc,
                         startCenterX + startWidth / 2.0f, startLane.centerY);

        path.lineTo(endCenterX + endWidth / 2.0f, endLane.centerY);

        path.quadraticTo(endCenterX, endLane.centerY + 2.0f * endArc,
                         endCenterX - endWidth / 2.0f, endLane.centerY);

        path.closeSubPath();
    }

    // Draw with opacity baked into the colour
    g.setColour(colour.withMultipliedAlpha(opacity));
    g.fillPath(path);
}

//==============================================================================
// Highway Texture Overlay

void HighwayRenderer::drawHighwayTexture(juce::Graphics &g)
{
    int texW = highwayTexture.getWidth();
    int texH = highwayTexture.getHeight();
    if (texW == 0 || texH == 0) return;

    bool isDrums = isPart(state, Part::DRUMS);
    float wNear = isDrums ? fretboardWidthScaleNearDrums : fretboardWidthScaleNearGuitar;
    float wMid  = isDrums ? fretboardWidthScaleMidDrums  : fretboardWidthScaleMidGuitar;
    float wFar  = isDrums ? fretboardWidthScaleFarDrums  : fretboardWidthScaleFarGuitar;

    // Extend texture to cover the full fade range
    float effectiveEnd = std::max(highwayPosEnd, farFadeEnd);
    float posRange = effectiveEnd - HIGHWAY_POS_START;

    // Use enough strips for ~4px each to balance quality vs performance
    auto edgeNear = PositionMath::getFretboardEdge(isDrums, HIGHWAY_POS_START, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, effectiveEnd);
    auto edgeFar = PositionMath::getFretboardEdge(isDrums, effectiveEnd, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, effectiveEnd);
    int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
    int stripCount = std::clamp(pixelHeight / 4, (int)HIGHWAY_MIN_STRIPS, 150);

    // Precompute edge positions for all strip boundaries (adjacent strips share edges)
    using LaneCorners = PositionConstants::LaneCorners;
    std::vector<std::pair<LaneCorners, float>> edges(stripCount + 1);
    for (int i = 0; i <= stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)stripCount;
        edges[i] = {PositionMath::getFretboardEdge(isDrums, pos, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, effectiveEnd), pos};
    }

    // Render strips into supersampled offscreen image for clean compositing
    constexpr int HS = HIGHWAY_RENDER_SCALE;
    int offW = (int)width * HS;
    int offH = (int)height * HS;
    if (highwayOffscreen.getWidth() != offW || highwayOffscreen.getHeight() != offH)
        highwayOffscreen = juce::Image(juce::Image::ARGB, offW, offH, true);
    else
        highwayOffscreen.clear({0, 0, offW, offH});

    {
        juce::Graphics og(highwayOffscreen);
        og.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        for (int i = 0; i < stripCount; i++)
        {
            auto& [bot, botPos] = edges[i];      // bottom (near strikeline)
            auto& [top, topPos] = edges[i + 1];  // top (far end)

            float destTop = top.centerY * HS;
            float destBottom = bot.centerY * HS;
            if (destBottom <= destTop) continue;

            // Average top/bottom edges for horizontal span — eliminates width jumps between strips
            float destLeft = (bot.leftX + top.leftX) * 0.5f * HS;
            float destRight = (bot.rightX + top.rightX) * 0.5f * HS;
            float destW = destRight - destLeft;
            float destH = destBottom - destTop;
            if (destW <= 0) continue;

            // Per-strip fade based on far-end position
            float stripMidPos = (botPos + topPos) * 0.5f;
            float stripAlpha = calculateOpacity(stripMidPos);
            if (stripAlpha <= 0.0f) continue;
            og.setOpacity(stripAlpha);

            // Texture V coordinates (vBot > vTop because lower position = larger V)
            float vTop = (1.0f - topPos) * HIGHWAY_TILES_PER_HIGHWAY + (float)scrollOffset;
            float vBot = (1.0f - botPos) * HIGHWAY_TILES_PER_HIGHWAY + (float)scrollOffset;

            float srcYf = fmod(vTop * texH, (float)texH);
            if (srcYf < 0) srcYf += texH;
            int srcY = (int)srcYf;
            int srcH = std::max(1, (int)((vBot - vTop) * texH));

            // Split draw at texture wrap boundary instead of clipping
            if (srcY + srcH > texH)
            {
                int h1 = texH - srcY;
                int h2 = srcH - h1;
                float splitFrac = (float)h1 / (float)srcH;
                float splitY = destTop + destH * splitFrac;

                if (h1 > 0)
                    og.drawImage(highwayTexture, (int)destLeft, (int)destTop, (int)std::ceil(destW), (int)std::ceil(splitY - destTop), 0, srcY, texW, h1);
                if (h2 > 0)
                    og.drawImage(highwayTexture, (int)destLeft, (int)splitY, (int)std::ceil(destW), (int)std::ceil(destBottom - splitY), 0, 0, texW, h2);
            }
            else
            {
                og.drawImage(highwayTexture, (int)destLeft, (int)destTop, (int)std::ceil(destW), (int)std::ceil(destH), 0, srcY, texW, srcH);
            }
        }
    }

    // Composite supersampled offscreen with fretboard clip path — downsample to canvas
    auto fretboardPath = PositionMath::getFretboardPath(isDrums, HIGHWAY_POS_START, effectiveEnd,
                                                        width, height, wNear, wMid, wFar);
    g.saveState();
    g.reduceClipRegion(fretboardPath);
    g.setOpacity(highwayTextureOpacity);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(highwayOffscreen, juce::Rectangle<float>(0, 0, (float)width, (float)height));
    g.restoreState();
}

//==============================================================================
// Draw image with per-strip highway fade

void HighwayRenderer::drawImageWithFade(juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> destRect)
{
    if (!img.isValid()) return;

    bool isDrums = isPart(state, Part::DRUMS);
    float wNear = isDrums ? fretboardWidthScaleNearDrums : fretboardWidthScaleNearGuitar;
    float wMid  = isDrums ? fretboardWidthScaleMidDrums  : fretboardWidthScaleMidGuitar;
    float wFar  = isDrums ? fretboardWidthScaleFarDrums  : fretboardWidthScaleFarGuitar;

    float effectiveEnd = std::max(highwayPosEnd, farFadeEnd);
    float posRange = effectiveEnd - HIGHWAY_POS_START;

    // Build position→Y edge table (same approach as highway texture strips)
    constexpr int stripCount = 40;
    std::pair<float, float> strips[stripCount + 1]; // {Y, position}
    for (int i = 0; i <= stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)stripCount;
        auto edge = PositionMath::getFretboardEdge(isDrums, pos, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, effectiveEnd);
        strips[i] = {edge.centerY, pos};
    }

    int imgW = img.getWidth(), imgH = img.getHeight();
    float dstTop = destRect.getY();
    float dstBot = destRect.getBottom();
    float dstH = destRect.getHeight();

    for (int i = 0; i < stripCount; i++)
    {
        float yBot = strips[i].first;    // bottom (near strikeline, large Y)
        float yTop = strips[i + 1].first; // top (far end, small Y)
        float posBot = strips[i].second;
        float posTop = strips[i + 1].second;

        // Skip strips entirely outside the destination rect
        if (yBot <= dstTop || yTop >= dstBot) continue;

        float midPos = (posBot + posTop) * 0.5f;
        float opacity = calculateOpacity(midPos);
        if (opacity <= 0.0f) continue;

        // Clamp strip bounds to destination rect
        float clampedTop = std::max(yTop, dstTop);
        float clampedBot = std::min(yBot, dstBot);

        // Map screen Y to source image coordinates
        float srcFracTop = (clampedTop - dstTop) / dstH;
        float srcFracBot = (clampedBot - dstTop) / dstH;
        int srcY = (int)(srcFracTop * imgH);
        int srcH = std::max(1, (int)(srcFracBot * imgH) - srcY);

        g.setOpacity(opacity);
        g.drawImage(img,
                    (int)destRect.getX(), (int)clampedTop,
                    (int)std::ceil(destRect.getWidth()), (int)std::ceil(clampedBot - clampedTop),
                    0, srcY, imgW, srcH);
    }
    g.setOpacity(1.0f);
}


