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

    // Exponential interpolation (shared by width and position)
    float progress = (std::pow(10, perspParams.exponentialCurve * (1 - depth)) - 1) / (std::pow(10, perspParams.exponentialCurve) - 1);
    float interpolatedWidth = normWidth2 + (normWidth1 - normWidth2) * progress;
    float finalWidth = interpolatedWidth * width;

    // Height uses perspective scaling for 3D effect
    float currentHeight = targetHeight * perspectiveScale;
    float yPos = normY2 * height + (normY1 - normY2) * height * progress;
    float xPos = normX2 * width + (normX1 - normX2) * width * progress;

    // Apply X offset and center positioning
    float xOffset = targetWidth * perspParams.xOffsetMultiplier;
    float finalX = xPos + xOffset - targetWidth / 2.0f;
    float finalY = yPos - targetHeight / 2.0f;

    return juce::Rectangle<float>(finalX, finalY, finalWidth, currentHeight);
}

//==============================================================================
// Guitar Positioning Data (public access for GlyphRenderer)

NormalizedCoordinates PositionMath::getGuitarOpenNoteCoords()
{
    return PositionConstants::getGuitarOpenNoteCoords();
}

NormalizedCoordinates PositionMath::getGuitarNoteCoords(uint gemColumn)
{
    return PositionConstants::getGuitarNoteCoords(gemColumn);
}

NormalizedCoordinates PositionMath::getDrumKickCoords()
{
    return PositionConstants::getDrumKickCoords();
}

NormalizedCoordinates PositionMath::getDrumPadCoords(uint gemColumn)
{
    return PositionConstants::getDrumPadCoords(gemColumn);
}

//==============================================================================
// Fretboard Boundary

LaneCorners PositionMath::getFretboardEdge(bool isDrums, float position,
                                           uint width, uint height,
                                           float widthScaleNear, float widthScaleMid,
                                           float widthScaleFar,
                                           float posStart, float posEnd)
{
    const auto& baseCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    // Normalize t across the full fretboard range (posStart to posEnd)
    // so the bezier curve spans the entire visible fretboard
    float t = std::clamp((position - posStart) / (posEnd - posStart), 0.0f, 1.0f);

    // Quadratic bezier interpolation through near (t=0), mid (t=0.5), far (t=1)
    // Solve for bezier control point P1 such that B(0.5) = mid
    float P1 = 2.0f * widthScaleMid - 0.5f * widthScaleNear - 0.5f * widthScaleFar;
    float widthScale = (1 - t) * (1 - t) * widthScaleNear + 2 * (1 - t) * t * P1 + t * t * widthScaleFar;

    auto coords = applyWidthScaling(baseCoords, widthScale);
    auto rect = createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                           coords.normX1, coords.normX2,
                                           coords.normWidth1, coords.normWidth2,
                                           true, width, height);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

LaneCorners PositionMath::getColumnPosition(bool isDrums, float position,
                                             uint width, uint height,
                                             float wNear, float wMid, float wFar,
                                             float posStart, float posEnd,
                                             const NormalizedCoordinates& colCoords,
                                             float sizeScale,
                                             float fretboardScale)
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

juce::Path PositionMath::getFretboardPath(bool isDrums, float posStart, float posEnd,
                                          uint width, uint height,
                                          float widthScaleNear, float widthScaleMid,
                                          float widthScaleFar, int segments)
{
    juce::Path path;

    std::vector<LaneCorners> edges;
    edges.reserve(segments + 1);
    for (int i = 0; i <= segments; i++)
    {
        float pos = posStart + (posEnd - posStart) * (float)i / (float)segments;
        edges.push_back(getFretboardEdge(isDrums, pos, width, height, widthScaleNear, widthScaleMid, widthScaleFar, posStart, posEnd));
    }

    // Left edge: from posStart (bottom) up to posEnd (top)
    path.startNewSubPath(edges[0].leftX, edges[0].centerY);
    for (int i = 1; i <= segments; i++)
        path.lineTo(edges[i].leftX, edges[i].centerY);

    // Right edge: from posEnd (top) back down to posStart (bottom)
    for (int i = segments; i >= 0; i--)
        path.lineTo(edges[i].rightX, edges[i].centerY);

    path.closeSubPath();
    return path;
}
