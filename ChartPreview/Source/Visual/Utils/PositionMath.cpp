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
    float position,
    float normY1, float normY2,
    float normX1, float normX2,
    float normWidth1, float normWidth2,
    bool isBarNote,
    uint width, uint height)
{
    auto perspParams = getPerspectiveParams();

    float depth = position;

    // Calculate 3D perspective scale for height
    float perspectiveScale = (perspParams.playerDistance + perspParams.highwayDepth * (1.0f - depth)) / perspParams.playerDistance;
    perspectiveScale = 1.0f + (perspectiveScale - 1.0f) * perspParams.perspectiveStrength;

    // Calculate dimensions
    float targetWidth = normWidth2 * width;
    float targetHeight = isBarNote ? targetWidth / perspParams.barNoteHeightRatio : targetWidth / perspParams.regularNoteHeightRatio;

    // Width calculation: both note types use exponential interpolation
    float widthProgress = (std::pow(10, perspParams.exponentialCurve * (1 - depth)) - 1) / (std::pow(10, perspParams.exponentialCurve) - 1);
    float interpolatedWidth = normWidth2 + (normWidth1 - normWidth2) * widthProgress;
    float finalWidth = interpolatedWidth * width;

    // Height uses perspective scaling for 3D effect
    float currentHeight = targetHeight * perspectiveScale;

    // Position calculation using exponential curve
    float progress = (std::pow(10, perspParams.exponentialCurve * (1 - depth)) - 1) / (std::pow(10, perspParams.exponentialCurve) - 1);
    float yPos = normY2 * height + (normY1 - normY2) * height * progress;
    float xPos = normX2 * width + (normX1 - normX2) * width * progress;

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

    // Normalize t across [posStart, posEnd], clamp to [0, 1]
    float range = posEnd - posStart;
    float t = (range > 0.0f) ? (position - posStart) / range : 0.0f;
    t = juce::jlimit(0.0f, 1.0f, t);

    // Quadratic bezier for width scale
    float P1 = 2.0f * wMid - 0.5f * wNear - 0.5f * wFar;
    float oneMinusT = 1.0f - t;
    float widthScale = oneMinusT * oneMinusT * wNear + 2.0f * oneMinusT * t * P1 + t * t * wFar;

    auto scaled = applyWidthScaling(fbCoords, widthScale);

    auto rect = createPerspectiveGlyphRect(position,
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

    // Column center and width as fractions of fretboard (derived from strikeline coords)
    float colCenterNorm = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
    float centerFrac = (colCenterNorm - fbCoords.normX1) / fbCoords.normWidth1;
    float widthFrac = (colCoords.normWidth1 / fbCoords.normWidth1) * sizeScale;

    // Expand fretboard reference from center (matches visual highway width)
    float edgeCenter = (edge.leftX + edge.rightX) * 0.5f;
    float edgeWidth = (edge.rightX - edge.leftX) * fretboardScale;
    float scaledLeftX = edgeCenter - edgeWidth * 0.5f;

    float actualCenter = scaledLeftX + centerFrac * edgeWidth;
    float actualWidth = widthFrac * edgeWidth;

    return {actualCenter - actualWidth * 0.5f, actualCenter + actualWidth * 0.5f, edge.centerY};
}

