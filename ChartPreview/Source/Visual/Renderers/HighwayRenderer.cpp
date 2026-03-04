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
    using Clock = std::chrono::high_resolution_clock;
    auto tTotal = collectPhaseTiming ? Clock::now() : Clock::time_point{};

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
    {
        ScopedPhaseMeasure m(lastPhaseTiming.texture_us, collectPhaseTiming);
        if (highwayTextureRenderer.hasTexture() && !skipHighwayTexture)
            highwayTextureRenderer.drawTexture(g, buildTextureParams());
    }

    // Repopulate drawCallMap
    drawCallMap.clear();

    {
        ScopedPhaseMeasure m(lastPhaseTiming.build_notes_us, collectPhaseTiming);
        drawNotesFromMap(g, trackWindow, windowStartTime, windowEndTime);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.build_sustains_us, collectPhaseTiming);
        drawSustainFromWindow(g, sustainWindow, windowStartTime, windowEndTime);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.build_gridlines_us, collectPhaseTiming);
        drawGridlinesFromMap(g, gridlines, windowStartTime, windowEndTime);
    }

    // Detect and add animations to drawCallMap (if enabled)
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_detect_us, collectPhaseTiming);
        if (hitIndicatorsEnabled)
        {
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow); }
            animationRenderer.renderToDrawCallMap(drawCallMap, width, height);
        }
    }

    // Draw layer by layer, then column by column within each layer
    if (collectPhaseTiming)
        lastPhaseTiming.layer_us.clear();

    {
        ScopedPhaseMeasure m(lastPhaseTiming.execute_draws_us, collectPhaseTiming);
        bool calledAfterLanes = false;
        for (const auto& drawOrder : drawCallMap)
        {
            auto tLayer = collectPhaseTiming ? Clock::now() : Clock::time_point{};

            for (const auto& column : drawOrder.second)
            {
                // Draw each layer from back to front
                for (auto it = column.second.rbegin(); it != column.second.rend(); ++it)
                {
                    (*it)(g);
                }
            }

            if (collectPhaseTiming)
                lastPhaseTiming.layer_us[drawOrder.first] += std::chrono::duration<double, std::micro>(Clock::now() - tLayer).count();

            // Insert SVG track overlay after lanes but before notes/sustains/gridlines
            if (!calledAfterLanes && drawOrder.first >= DrawOrder::LANE && onAfterLanes)
            {
                ScopedPhaseMeasure m2(lastPhaseTiming.after_lanes_us, collectPhaseTiming);
                onAfterLanes(g);
                calledAfterLanes = true;
            }
        }
        // If no LANE layer existed, still call it
        if (!calledAfterLanes && onAfterLanes)
        {
            ScopedPhaseMeasure m2(lastPhaseTiming.after_lanes_us, collectPhaseTiming);
            onAfterLanes(g);
        }
    }

    // Advance animation frames after rendering
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_advance_us, collectPhaseTiming);
        if (hitIndicatorsEnabled)
            animationRenderer.advanceFrames();
    }

    if (collectPhaseTiming)
        lastPhaseTiming.total_us = std::chrono::duration<double, std::micro>(Clock::now() - tTotal).count();
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
        Gridline gridlineType = gridline.type;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((gridline.time - windowStartTime) / windowTimeSpan) + gridlinePosOffset;

        if (normalizedPosition >= HIGHWAY_POS_START && normalizedPosition <= farFadeEnd)
        {
            float fadeOpacity = calculateOpacity(normalizedPosition);

            if (useSvgAssets)
            {
                juce::Drawable* drawable = assetManager.getGridlineDrawable(gridlineType);
                if (drawable != nullptr)
                    drawCallMap[DrawOrder::GRID][0].push_back([=](juce::Graphics &g) { drawGridlineSVG(g, normalizedPosition, drawable, gridlineType, fadeOpacity); });
            }
            else
            {
                juce::Image *markerImage = assetManager.getGridlineImage(gridlineType);
                if (markerImage != nullptr)
                    drawCallMap[DrawOrder::GRID][0].push_back([=](juce::Graphics &g) { drawGridlinePNG(g, normalizedPosition, markerImage, gridlineType, fadeOpacity); });
            }
        }
    }
}

void HighwayRenderer::drawGridline(juce::Graphics& g, float position, Gridline gridlineType, float fadeOpacity)
{
    if (useSvgAssets)
    {
        juce::Drawable* drawable = assetManager.getGridlineDrawable(gridlineType);
        if (drawable)
            drawGridlineSVG(g, position, drawable, gridlineType, fadeOpacity);
    }
    else
    {
        juce::Image* markerImage = assetManager.getGridlineImage(gridlineType);
        if (markerImage)
            drawGridlinePNG(g, position, markerImage, gridlineType, fadeOpacity);
    }
}

void HighwayRenderer::drawGridlinePNG(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType, float fadeOpacity)
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

void HighwayRenderer::drawGridlineSVG(juce::Graphics& g, float position, juce::Drawable* drawable, Gridline gridlineType, float fadeOpacity)
{
    if (!drawable) return;

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

    g.saveState();
    g.setOpacity(opacity);
    drawable->drawWithin(g, rect, juce::RectanglePlacement::stretchToFit, 1.0f);
    g.restoreState();
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

    if (curvedOffscreen.getWidth() < offW || curvedOffscreen.getHeight() < offH)
        curvedOffscreen = juce::Image(juce::Image::ARGB, offW, offH, true);
    else
        curvedOffscreen.clear({0, 0, curvedOffscreen.getWidth(), curvedOffscreen.getHeight()});
    {
        juce::Graphics og(curvedOffscreen);
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
    g.drawImage(curvedOffscreen.getClippedImage({0, 0, offW, offH}), destRect);
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


