/*
    ==============================================================================

        NoteRenderer.h
        Author:  Noah Baxter

        Note/gem rendering: drawNotesFromMap, drawFrame, drawGem,
        plus overlay positioning (absorbed from GlyphRenderer).

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

class NoteRenderer
{
public:
    NoteRenderer(juce::ValueTree& state, AssetManager& assetManager);

    void populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float wNear, float wMid, float wFar, float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    DrawCallMap* currentDrawCallMap = nullptr;
    uint width = 0, height = 0;
    float wNear = 0, wMid = 0, wFar = 0, posEnd = 0;
    float farFadeEnd = 0, farFadeLen = 0, farFadeCurve = 0;

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

    void drawFrame(const TimeBasedTrackFrame& gems, float position, double frameTime);
    void drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime);

    // Overlay positioning (absorbed from GlyphRenderer)
    static juce::Rectangle<float> getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent);
};
