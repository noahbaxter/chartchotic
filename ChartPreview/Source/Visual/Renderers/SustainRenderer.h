/*
    ==============================================================================

        SustainRenderer.h
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

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

class SustainRenderer
{
public:
    SustainRenderer(juce::ValueTree& state, AssetManager& assetManager);

    PositionConstants::LaneShapeConfig laneShape;

    void populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height, bool showLanes, bool showSustains,
                  float wNear, float wMid, float wFar, float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve,
                  const PositionConstants::NormalizedCoordinates* laneCoordsGuitar,
                  const PositionConstants::NormalizedCoordinates* laneCoordsDrums);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    DrawCallMap* currentDrawCallMap = nullptr;
    uint width = 0, height = 0;
    float wNear = 0, wMid = 0, wFar = 0, posEnd = 0;
    float farFadeEnd = 0, farFadeLen = 0, farFadeCurve = 0;
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    bool showLanes = true, showSustains = true;

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

    float calculateOpacity(float position)
    {
        return calculateFarFade(position, farFadeEnd, farFadeLen, farFadeCurve);
    }

    void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
    void drawPerspectiveSustainFlat(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition,
                                     float opacity, float sustainWidth, juce::Colour colour, bool isLane);
};
