/*
    ==============================================================================

        GridlineRenderer.h
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"
#include "../../Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

class GridlineRenderer
{
public:
    GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager);

    void populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float wNear, float wMid, float wFar, float posEnd,
                  float gridlinePosOffset,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    uint width = 0, height = 0;
    float wNear = 0, wMid = 0, wFar = 0, posEnd = 0;

    using LaneCorners = PositionConstants::LaneCorners;
    using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

    LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                              float sizeScale, float fretboardScale = 1.0f)
    {
        bool isDrums = isPart(state, Part::DRUMS);
        return PositionMath::getColumnPosition(isDrums, position, width, height,
                                               wNear, wMid, wFar,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale);
    }

    void drawGridline(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType, float fadeOpacity);
};
