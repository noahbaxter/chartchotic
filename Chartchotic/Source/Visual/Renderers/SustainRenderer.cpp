/*
    ==============================================================================

        SustainRenderer.cpp
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "SustainRenderer.h"

using namespace PositionConstants;

SustainRenderer::SustainRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void SustainRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                               double windowStartTime, double windowEndTime,
                               uint width, uint height, bool showLanes, bool showSustains,
                               float posEnd,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               const NormalizedCoordinates* laneCoordsGuitar,
                               const NormalizedCoordinates* laneCoordsDrums)
{
    currentDrawCallMap = &drawCallMap;
    this->width = width;
    this->height = height;
    this->showLanes = showLanes;
    this->showSustains = showSustains;
    this->posEnd = posEnd;
    this->farFadeEnd = farFadeEnd;
    this->farFadeLen = farFadeLen;
    this->farFadeCurve = farFadeCurve;
    this->laneCoordsGuitar = laneCoordsGuitar;
    this->laneCoordsDrums = laneCoordsDrums;

    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, windowStartTime, windowEndTime);
    }
}

void SustainRenderer::drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    bool isLane = (sustain.sustainType == SustainType::LANE);

    // Gate by render toggle
    if (isLane && !showLanes) return;
    if (!isLane && !showSustains) return;

    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // Determine clip position
    float clipPos;
    if (isLane)
    {
        clipPos = HIGHWAY_POS_START;
    }
    else if (hitAnimationsOn)
    {
        clipPos = isBarNote(sustain.gemColumn, activePart == Part::GUITAR ? Part::GUITAR : Part::DRUMS)
            ? BAR_SUSTAIN_CLIP : SUSTAIN_CLIP;
    }
    else
    {
        clipPos = HIGHWAY_POS_START;
    }

    double clipTime = clipPos * windowTimeSpan;
    if (sustain.endTime < clipTime) return;

    double clippedStartTime = std::max(clipTime, sustain.startTime);

    // Position offsets differ for lanes vs sustains
    float startOffset, endOffset;
    if (isLane)
    {
        startOffset = laneShape.startOffset;
        endOffset = laneShape.endOffset;
    }
    else if (isBarNote(sustain.gemColumn, activePart == Part::GUITAR ? Part::GUITAR : Part::DRUMS))
    {
        startOffset = BAR_SUSTAIN_START_OFFSET;
        endOffset = BAR_SUSTAIN_END_OFFSET;
    }
    else
    {
        startOffset = SUSTAIN_START_OFFSET;
        endOffset = SUSTAIN_END_OFFSET;
    }

    // In Bemani mode, lanes use pixel-based padding (applied in drawSustainBody),
    // sustains still use position-space offsets
    if (PositionMath::bemaniMode)
    {
        if (sustain.sustainType == SustainType::LANE)
        {
            // Zero out position-space offsets — pixel padding applied after Y conversion
            startOffset = 0.0f;
            endOffset = 0.0f;
        }
        else
        {
            startOffset = bemaniConfig.sustStartOff;
            endOffset = bemaniConfig.sustEndOff;
        }
    }

    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan) + startOffset;
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan) + endOffset;

    if (endPosition < clipPos || startPosition > farFadeEnd) return;

    startPosition = std::max(clipPos, startPosition);
    endPosition = std::min(farFadeEnd, endPosition);

    bool starPowerActive = state.getProperty("starPower");
    bool shouldBeWhite = starPowerActive && sustain.gemType.starPower;
    auto colour = assetManager.getLaneColour(sustain.gemColumn, activePart == Part::GUITAR ? Part::GUITAR : Part::DRUMS, shouldBeWhite);

    float opacity, sustainWidth;
    DrawOrder sustainDrawOrder;
    bool isKickCol = isDrumKick(sustain.gemColumn);
    switch (sustain.sustainType) {
        case SustainType::LANE:
            opacity = LANE_OPACITY;
            sustainWidth = isKickCol ? LANE_OPEN_WIDTH : LANE_WIDTH;
            sustainDrawOrder = DrawOrder::LANE;
            break;
        case SustainType::SUSTAIN:
        default:
            opacity = SUSTAIN_OPACITY;
            sustainWidth = isKickCol ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            sustainDrawOrder = isKickCol ? DrawOrder::BAR : DrawOrder::SUSTAIN;
            break;
    }

    (*currentDrawCallMap)[static_cast<int>(sustainDrawOrder)][sustain.gemColumn].push_back([=](juce::Graphics& g) {
        this->drawSustainBody(g, sustain.gemColumn, startPosition, endPosition, opacity, sustainWidth, colour, isLane);
    });
}

void SustainRenderer::drawSustainBody(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour, bool isLane)
{
    bool isDrums = activePart == Part::DRUMS;
    bool isBar = isBarNote(gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);

    // Look up lane coords
    NormalizedCoordinates colCoords;
    float laneScale;
    int bemaniIdx = -1;
    if (isDrums) {
        bool isKick = isDrumKick(gemColumn);
        uint dIdx = drumColumnIndex(gemColumn);
        colCoords = laneCoordsDrums[dIdx];
        laneScale = isKick ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        bemaniIdx = (int)dIdx - 1;
    } else {
        colCoords = laneCoordsGuitar[gemColumn];
        laneScale = (gemColumn == 0) ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        bemaniIdx = (int)gemColumn - 1;
    }

    auto startLane = getColumnEdge(startPosition, colCoords, laneScale, PositionConstants::FRETBOARD_SCALE, bemaniIdx);
    auto endLane = getColumnEdge(endPosition, colCoords, laneScale, PositionConstants::FRETBOARD_SCALE, bemaniIdx);

    // Bemani mode: simple shapes, no perspective geometry
    if (PositionMath::bemaniMode)
    {
        float bSustW = bemaniConfig.sustainWidth;
        float bBarSustW = bemaniConfig.barSustainWidth;
        float capFrac = bemaniConfig.sustainCap;

        float laneWidth = startLane.rightX - startLane.leftX;
        float centerX = (startLane.leftX + startLane.rightX) * 0.5f;
        float topY = std::min(startLane.centerY, endLane.centerY);
        float botY = std::max(startLane.centerY, endLane.centerY);

        float zNudge = isLane ? bemaniConfig.laneZ : bemaniConfig.sustainZ;
        topY += zNudge;
        botY += zNudge;

        // Pixel-based lane padding — extends lane past note edges by fixed pixels
        if (isLane)
        {
            float laneEndPxVal = isBar ? bemaniConfig.barLaneEndPx(isDrums) : bemaniConfig.laneEndPx(isDrums);
            topY -= laneEndPxVal;
            botY += isBar ? bemaniConfig.barLaneStartPx : bemaniConfig.laneStartPx;
        }

        if (isBar)
        {
            auto fb = PositionMath::getFretboardEdge(isDrums, startPosition, width, height,
                PositionConstants::HIGHWAY_POS_START, posEnd);
            centerX = (fb.leftX + fb.rightX) * 0.5f;
            laneWidth = fb.rightX - fb.leftX;
        }

        if (isLane)
        {
            // Lanes: wider fill with caps on both ends
            float w = isBar ? laneWidth * bemaniConfig.barLaneFillW : laneWidth * bemaniConfig.laneFillW;
            float lx = centerX - w * 0.5f;
            float laneCapFrac = isBar ? bemaniConfig.barCap : bemaniConfig.laneCap;
            float capH = laneCapFrac * w;

            g.setColour(colour.withAlpha(opacity));
            juce::Path path;
            path.startNewSubPath(lx, topY);
            path.quadraticTo(centerX, topY - capH, lx + w, topY);
            path.lineTo(lx + w, botY);
            path.quadraticTo(centerX, botY + capH, lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        else
        {
            // Sustains: narrower, no front cap, small back cap (bar sustains get less cap)
            float w = isBar ? laneWidth * bBarSustW * 0.6f : laneWidth * bSustW;
            float lx = centerX - w * 0.5f;
            float sustCapFrac = isBar ? bemaniConfig.barCap : capFrac;
            float backCapH = sustCapFrac * w * 0.3f;

            g.setColour(colour.withAlpha(opacity));
            juce::Path path;
            // Top (far end) — small back cap
            path.startNewSubPath(lx, topY);
            if (backCapH > 0.5f)
                path.quadraticTo(centerX, topY - backCapH, lx + w, topY);
            else
                path.lineTo(lx + w, topY);
            // Right side down
            path.lineTo(lx + w, botY);
            // Bottom (strikeline end) — flat, no cap
            path.lineTo(lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        return;
    }

    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;

    float startCenterX = (startLane.leftX + startLane.rightX) * 0.5f;
    float endCenterX = (endLane.leftX + endLane.rightX) * 0.5f;

    float startLeftX  = startCenterX - startWidth * 0.5f;
    float startRightX = startCenterX + startWidth * 0.5f;
    float endLeftX    = endCenterX - endWidth * 0.5f;
    float endRightX   = endCenterX + endWidth * 0.5f;

    float startY = startLane.centerY;
    float endY   = endLane.centerY;

    float startCurve, endCurve;
    if (isLane)
    {
        startCurve = laneShape.innerStartArc;
        endCurve = laneShape.innerEndArc;
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

    const auto& fbCoords = isDrums
        ? PositionConstants::drumFretboardCoords
        : PositionConstants::guitarFretboardCoords;
    auto startFretboard = getColumnEdge(startPosition, fbCoords, 1.0f);
    auto endFretboard = getColumnEdge(endPosition, fbCoords, 1.0f);

    float startFretboardWidth = startFretboard.rightX - startFretboard.leftX;
    float endFretboardWidth = endFretboard.rightX - endFretboard.leftX;

    float startArcY = startCurve * startFretboardWidth;
    float endArcY = endCurve * endFretboardWidth;

    float laneStartParabolaY = 0.0f;
    float laneEndParabolaY = 0.0f;
    if (isLane)
    {
        float startT = (startFretboardWidth > 0.0f) ? (startCenterX - startFretboard.leftX) / startFretboardWidth : 0.5f;
        float endT = (endFretboardWidth > 0.0f) ? (endCenterX - endFretboard.leftX) / endFretboardWidth : 0.5f;

        laneStartParabolaY = laneShape.outerStartArc * startFretboardWidth * 4.0f * startT * (1.0f - startT);
        laneEndParabolaY = laneShape.outerEndArc * endFretboardWidth * 4.0f * endT * (1.0f - endT);
    }

    juce::Path path;

    float adjStartY = startY + laneStartParabolaY;
    float adjEndY = endY + laneEndParabolaY;

    path.startNewSubPath(startLeftX, adjStartY);

    float startMidX = (startLeftX + startRightX) * 0.5f;
    path.quadraticTo(startMidX, adjStartY + startArcY, startRightX, adjStartY);

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

    float endMidX = (endLeftX + endRightX) * 0.5f;
    path.quadraticTo(endMidX, adjEndY + endArcY, endLeftX, adjEndY);

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

    // Fill the path — use gradient if sustain extends into the fade zone
    float fadeStart = farFadeEnd - farFadeLen;
    if (endPosition > fadeStart)
    {
        float fadeStartClamped = std::max(fadeStart, startPosition);
        auto fadeStartEdge = PositionMath::getFretboardEdge(isDrums, fadeStartClamped, width, height,
            PositionConstants::HIGHWAY_POS_START, posEnd);
        auto fadeEndEdge = PositionMath::getFretboardEdge(isDrums, farFadeEnd, width, height,
            PositionConstants::HIGHWAY_POS_START, posEnd);

        float gradStartY = fadeStartEdge.centerY;
        float gradEndY   = fadeEndEdge.centerY;

        float startOpacity = calculateOpacity(fadeStartClamped) * opacity;

        juce::ColourGradient gradient(
            colour.withAlpha(startOpacity), 0.0f, gradStartY,
            colour.withAlpha(0.0f), 0.0f, gradEndY,
            false);

        if (startPosition < fadeStart)
        {
            float fullRange = adjStartY - gradEndY;
            float opaqueRange = adjStartY - gradStartY;
            float opaqueT = (fullRange > 0.0f) ? opaqueRange / fullRange : 0.0f;

            gradient = juce::ColourGradient(
                colour.withAlpha(opacity), 0.0f, adjStartY,
                colour.withAlpha(0.0f), 0.0f, gradEndY,
                false);
            gradient.addColour(opaqueT, colour.withAlpha(opacity));
        }

        g.setGradientFill(gradient);
    }
    else
    {
        g.setColour(colour.withAlpha(opacity));
    }
    g.fillPath(path);
}
