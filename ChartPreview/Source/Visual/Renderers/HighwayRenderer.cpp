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
    if (highwayTexture.isValid())
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
    }

    // Advance animation frames after rendering
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
        double gridlineTime = gridline.time - GRIDLINE_TIME_OFFSET;
        Gridline gridlineType = gridline.type;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan);

        if (normalizedPosition >= HIGHWAY_POS_START && normalizedPosition <= 1.0f)
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
    float arcHeight = fbHalfWidth * 2.0f * (barNote ? BAR_CURVATURE : NOTE_CURVATURE);

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

    // Don't render sustains that are fully past the strikeline (small grace period)
    if (sustain.endTime < -0.15) return;

    // Clip sustain start to slightly past the strikeline for smooth exit
    double startTime = sustain.startTime;
    if (sustain.sustainType == SustainType::LANE)
        startTime += LANE_START_OFFSET;
    double clippedStartTime = std::max(-0.15, startTime);

    // Calculate normalized positions for start and end of sustain
    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan);
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan);

    // Only draw sustains that are visible in our window
    if (endPosition < -0.05f || startPosition > 1.0f) return;

    // Clamp to visible area (allow slight past-strikeline for smooth exit)
    startPosition = std::max(-0.05f, startPosition);
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
        drawPerspectiveSustainFlat(g, sustain.gemColumn, startPosition, endPosition, opacity, sustainWidth, colour);
    });
}

void HighwayRenderer::drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour)
{
    // Look up lane coords (separate table from glyph — slightly different widths)
    bool isDrums = isPart(state, Part::DRUMS);
    NormalizedCoordinates colCoords;
    float laneScale;
    if (isDrums) {
        bool isKick = (gemColumn == 0 || gemColumn == 6);
        colCoords = isKick ? drumLaneCoords[0] : drumLaneCoords[gemColumn];
        laneScale = isKick ? BAR_SIZE : GEM_SIZE;
    } else {
        colCoords = guitarLaneCoords[gemColumn];
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
    float startArc = startFbHalfW * 2.0f * SUSTAIN_START_CURVE;
    float endArc = endFbHalfW * 2.0f * SUSTAIN_END_CURVE;

    // Build curved sustain path — edges follow the highway arc
    juce::Path path;

    // Bottom edge (start, near strikeline): left → right
    path.startNewSubPath(startCenterX - startWidth / 2.0f, startLane.centerY);
    path.quadraticTo(startCenterX, startLane.centerY + 2.0f * startArc,
                     startCenterX + startWidth / 2.0f, startLane.centerY);

    // Right edge: straight line to top-right
    path.lineTo(endCenterX + endWidth / 2.0f, endLane.centerY);

    // Top edge (end, far from strikeline): right → left
    path.quadraticTo(endCenterX, endLane.centerY + 2.0f * endArc,
                     endCenterX - endWidth / 2.0f, endLane.centerY);

    // Left edge: close back to start
    path.closeSubPath();

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

    float posRange = highwayPosEnd - HIGHWAY_POS_START;

    // Use enough strips for ~4px each to balance quality vs performance
    auto edgeNear = PositionMath::getFretboardEdge(isDrums, HIGHWAY_POS_START, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, highwayPosEnd);
    auto edgeFar = PositionMath::getFretboardEdge(isDrums, highwayPosEnd, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, highwayPosEnd);
    int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
    int stripCount = std::clamp(pixelHeight / 4, (int)HIGHWAY_MIN_STRIPS, 150);

    // Precompute edge positions for all strip boundaries (adjacent strips share edges)
    using LaneCorners = PositionConstants::LaneCorners;
    std::vector<std::pair<LaneCorners, float>> edges(stripCount + 1);
    for (int i = 0; i <= stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)stripCount;
        edges[i] = {PositionMath::getFretboardEdge(isDrums, pos, width, height, wNear, wMid, wFar, HIGHWAY_POS_START, highwayPosEnd), pos};
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

            // Fade out at far end, matching note opacity behavior
            float stripMidPos = (botPos + topPos) * 0.5f;
            float fadeAlpha = 1.0f;
            if (stripMidPos >= OPACITY_FADE_START)
                fadeAlpha = std::max(0.0f, 1.0f - (stripMidPos - OPACITY_FADE_START) / (1.0f - OPACITY_FADE_START));
            if (fadeAlpha <= 0.0f) continue;
            og.setOpacity(fadeAlpha);

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
    auto fretboardPath = PositionMath::getFretboardPath(isDrums, HIGHWAY_POS_START, highwayPosEnd,
                                                        width, height, wNear, wMid, wFar);
    g.saveState();
    g.reduceClipRegion(fretboardPath);
    g.setOpacity(HIGHWAY_OPACITY);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(highwayOffscreen, juce::Rectangle<float>(0, 0, (float)width, (float)height));
    g.restoreState();
}


