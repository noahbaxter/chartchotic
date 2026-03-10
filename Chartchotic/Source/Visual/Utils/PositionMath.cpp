/*
    ==============================================================================

        PositionMath.cpp
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains mathematical functions for computing glyph positions
        and lane coordinates using 3D perspective calculations.

    ==============================================================================
*/

#include "PositionMath.h"

using namespace PositionConstants;

#ifdef DEBUG
PerspectiveParams PositionMath::debugPerspParamsGuitar = getGuitarPerspectiveParams();
PerspectiveParams PositionMath::debugPerspParamsDrums = getDrumPerspectiveParams();
bool PositionMath::debugPolyShade = false;
#endif

//==============================================================================
// Helper to apply scaling and centering to coordinates

NormalizedCoordinates PositionMath::applyWidthScaling(
    const NormalizedCoordinates& coords,
    float scaler)
{
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return {adjustedNormX1, adjustedNormX2, coords.normY1, coords.normY2, scaledNormWidth1, scaledNormWidth2};
}

//==============================================================================
// Core 3D Perspective Calculation (used by getFretboardEdge)

juce::Rectangle<float> PositionMath::createPerspectiveGlyphRect(
    const PerspectiveParams& perspParams,
    float position,
    float normY1, float normY2,
    float normX1, float normX2,
    float normWidth1, float normWidth2,
    bool isBarNote,
    uint width, uint height)
{
    float depth = position / perspParams.vanishingPointDepth;

    // Derive 1/z depth coefficient from exponentialCurve parameter
    float expDenom = std::pow(10.0f, perspParams.exponentialCurve) - 1.0f;
    float expMidProgress = (std::pow(10.0f, perspParams.exponentialCurve * 0.5f) - 1.0f) / expDenom;
    float k = 1.0f / expMidProgress - 2.0f;

    // Apply near-width scaling (controls bottom spread / edge angle from VP)
    float nw = perspParams.nearWidth;
    float adjNormWidth1 = normWidth1 * nw;
    float adjNormX1 = normX1 + (normWidth1 - adjNormWidth1) * 0.5f;

    // 1/z perspective scale for note height (always positive, matches endpoints exactly)
    float scaleNear = 1.0f + (perspParams.highwayDepth / perspParams.playerDistance)
                           * perspParams.perspectiveStrength;
    float perspectiveScale = scaleNear / (1.0f + depth * (scaleNear - 1.0f));

    // Calculate dimensions
    float targetWidth = normWidth2 * width;
    float targetHeight = isBarNote ? targetWidth / perspParams.barNoteHeightRatio : targetWidth / perspParams.regularNoteHeightRatio;

    // Width calculation: both note types use exponential interpolation
    float widthProgress = (1.0f - depth) / (1.0f + depth * k);
    float interpolatedWidth = normWidth2 + (adjNormWidth1 - normWidth2) * widthProgress;
    float finalWidth = interpolatedWidth * width;

    // Height uses perspective scaling for 3D effect
    float currentHeight = targetHeight * perspectiveScale;

    // Position calculation using exponential curve
    float progress = (1.0f - depth) / (1.0f + depth * k);
    float adjNormY2 = normY2 + perspParams.vanishingPointY;
    float yPos = adjNormY2 * height + (normY1 - adjNormY2) * height * progress;
    float xPos = normX2 * width + (adjNormX1 - normX2) * width * progress;

    // Apply X offset and center positioning
    float xOffset = targetWidth * perspParams.xOffsetMultiplier;
    float finalX = xPos + xOffset - targetWidth / 2.0f;
    float finalY = yPos - targetHeight / 2.0f;

    return juce::Rectangle<float>(finalX, finalY, finalWidth, currentHeight);
}

//==============================================================================
// Bezier Positioning System

LaneCorners PositionMath::getFretboardEdge(
    bool isDrums, float position, uint width, uint height,
    float wNear, float wMid, float wFar,
    float posStart, float posEnd)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    // Derive the same 1/z progress used by createPerspectiveGlyphRect, so the
    // bezier width scale follows the same curve as the perspective projection.
    // This prevents a derivative discontinuity at position = vpDepth.
#ifdef DEBUG
    const auto& pp = perspParams(isDrums);
#else
    auto pp = getPerspectiveParams(isDrums);
#endif
    float depth = position / pp.vanishingPointDepth;
    float expDenom = std::pow(10.0f, pp.exponentialCurve) - 1.0f;
    float expMidProgress = (std::pow(10.0f, pp.exponentialCurve * 0.5f) - 1.0f) / expDenom;
    float k = 1.0f / expMidProgress - 2.0f;
    float progress = (1.0f - depth) / (1.0f + depth * k);

    // Linear width scale: wNear at strikeline (progress=1), wFar at vanishing point (progress=0)
    float widthScale = wFar + (wNear - wFar) * progress;

    auto scaled = applyWidthScaling(fbCoords, widthScale);

    auto rect = createPerspectiveGlyphRect(pp, position,
        scaled.normY1, scaled.normY2,
        scaled.normX1, scaled.normX2,
        scaled.normWidth1, scaled.normWidth2,
        true, width, height);

    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}


LaneCorners PositionMath::getColumnPosition(
    bool isDrums, float position, uint width, uint height,
    float wNear, float wMid, float wFar,
    float posStart, float posEnd,
    const NormalizedCoordinates& colCoords,
    float sizeScale, float fretboardScale)
{
    auto edge = getFretboardEdge(isDrums, position, width, height,
                                 wNear, wMid, wFar, posStart, posEnd);

    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    float nearCenterNorm = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
    float centerFrac = (nearCenterNorm - fbCoords.normX1) / fbCoords.normWidth1;
    float widthFrac  = (colCoords.normWidth1 / fbCoords.normWidth1) * sizeScale;

    // Expand fretboard reference from center (matches visual highway width)
    float edgeCenter = (edge.leftX + edge.rightX) * 0.5f;
    float edgeWidth = (edge.rightX - edge.leftX) * fretboardScale;
    float scaledLeftX = edgeCenter - edgeWidth * 0.5f;

    float actualCenter = scaledLeftX + centerFrac * edgeWidth;
    float actualWidth = widthFrac * edgeWidth;

    return {actualCenter - actualWidth * 0.5f, actualCenter + actualWidth * 0.5f, edge.centerY};
}

