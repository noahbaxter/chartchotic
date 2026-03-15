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
#include "DrawingConstants.h"

using namespace PositionConstants;

bool PositionMath::bemaniMode = false;
float PositionMath::bemaniHwyScale = 1.0f;

#ifdef DEBUG
PerspectiveParams PositionMath::debugPerspParamsGuitar = getGuitarPerspectiveParams();
PerspectiveParams PositionMath::debugPerspParamsDrums = getDrumPerspectiveParams();
bool PositionMath::debugPolyShade = false;
#endif

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
    if (bemaniMode)
    {
        // Flat mode: full-height linear Y, constant width, no perspective
        float nw = perspParams.nearWidth;
        float adjW = normWidth1 * nw;
        // Center the scaled fretboard around the original fretboard center
        float fbCenter = normX1 + normWidth1 * 0.5f;
        float adjX = fbCenter - adjW * 0.5f;

        // Position 0 = strikeline, positive = toward top of viewport
        float strikeY = bemaniConfig.strikelinePos;
        float noteYOff = bemaniConfig.noteYOffset;
        float scaledPos = position / std::max(0.1f, bemaniHwyScale);
        float yPos = (float)height * (strikeY - scaledPos * strikeY + noteYOff);

        float finalWidth = adjW * width;
        float targetHeight = isBarNote
            ? finalWidth / perspParams.barNoteHeightRatio
            : finalWidth / perspParams.regularNoteHeightRatio;

        float finalX = adjX * width;
        float finalY = yPos - targetHeight / 2.0f;

        return juce::Rectangle<float>(finalX, finalY, finalWidth, targetHeight);
    }

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
    float posStart, float posEnd)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

#ifdef DEBUG
    const auto& pp = perspParams(isDrums);
#else
    auto pp = getPerspectiveParams(isDrums);
#endif

    auto rect = createPerspectiveGlyphRect(pp, position,
        fbCoords.normY1, fbCoords.normY2,
        fbCoords.normX1, fbCoords.normX2,
        fbCoords.normWidth1, fbCoords.normWidth2,
        true, width, height);

    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}


LaneCorners PositionMath::getColumnPosition(
    bool isDrums, float position, uint width, uint height,
    float posStart, float posEnd,
    const NormalizedCoordinates& colCoords,
    float sizeScale, float fretboardScale,
    int bemaniLaneIdx)
{
    auto edge = getFretboardEdge(isDrums, position, width, height,
                                 posStart, posEnd);

    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    float nearCenterNorm = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
    float centerFrac = (nearCenterNorm - fbCoords.normX1) / fbCoords.normWidth1;
    float widthFrac  = (colCoords.normWidth1 / fbCoords.normWidth1) * sizeScale;

    // In Bemani mode, use tunable gem positions by lane index
    if (bemaniMode)
    {
        int numLanes = isDrums ? 4 : 5;

        if (bemaniLaneIdx >= 0 && bemaniLaneIdx < numLanes)
        {
            // Evenly spaced lanes: center of lane i = (i + 0.5) / numLanes
            centerFrac = ((float)bemaniLaneIdx + 0.5f) / (float)numLanes;
        }
        // else: bar notes (bemaniLaneIdx = -1) — keep original centerFrac (centered)

        widthFrac = sizeScale / (float)numLanes;
    }

    // Expand fretboard reference from center (matches visual highway width)
    float edgeCenter = (edge.leftX + edge.rightX) * 0.5f;
    // In Bemani mode, fretboard is already correctly sized — don't apply perspective expansion
    float edgeWidth = (edge.rightX - edge.leftX) * (bemaniMode ? 1.0f : fretboardScale);
    float scaledLeftX = edgeCenter - edgeWidth * 0.5f;

    float actualCenter = scaledLeftX + centerFrac * edgeWidth;
    float actualWidth = widthFrac * edgeWidth;

    return {actualCenter - actualWidth * 0.5f, actualCenter + actualWidth * 0.5f, edge.centerY};
}

