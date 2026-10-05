/*
    ==============================================================================

        SustainRenderer.h
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../../Midi/Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "../Geometry/PositionConstants.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "HighwayRenderer.h"

namespace PositionConstants { struct RenderTypeConfig; }

class SustainRenderer : public HighwayRenderer
{
public:
    SustainRenderer(juce::ValueTree& state, AssetManager& assetManager);

    PositionConstants::LaneShapeConfig laneShape;

    // Where gridlines sit, so phrase fills can meet them: the scene's gridline z offset (current
    // pixels at the strikeline) and position offset. Set before populate.
    float phraseGridZ = 0.0f;
    float phrasePosOffset = 0.0f;

    // In the phrase view a phrase is the subject rather than context, so it draws stronger.
    bool phraseViewActive = false;

    // The same curvature the gems are drawn with. A sustain starts at a gem, so it has to bow by
    // the same amount that gem does, which means tracking the tunable rather than a copy of the
    // default. SceneRenderer points both renderers at the same values.
    float noteCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
    float noteCurvatureDrums = PositionConstants::NOTE_CURVATURE;

    struct TintedSustain { int lane; double startTime; double endTime; juce::Colour colour; bool matchStart = false; };
    std::vector<TintedSustain> tintedSustains;

    void populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height, bool showLanes, bool showSustains,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve,
                  const PositionConstants::NormalizedCoordinates* laneCoords);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call (frame geometry state lives in HighwayRenderer)
    DrawCallMap* currentDrawCallMap = nullptr;
    bool showLanes = true, showSustains = true;

public:
    // The lane colour spread of a fill and of a BRE, each hideable on its own. The activation
    // band has its own toggle rather than riding on the fill's: it is the part a player reads,
    // and it stays useful with the colour spread turned off.
    bool showFills = DEFAULT_SHOW_FILLS, showBre = DEFAULT_SHOW_BRE,
         showActivationLane = DEFAULT_SHOW_ACTIVATION, showSolos = true;

private:

    void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
    void drawSustainBody(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition,
                         float opacity, float sustainWidth, juce::Colour colour, bool isLane);

    // Star power bands and fill lanes: flat fills whose ends trace the gridline centreline,
    // so a phrase that starts and stops on gridlines meets them with no seam.
    void drawPhrase(const TimeBasedSustainEvent& phrase, double windowStartTime, double windowEndTime);

    // Elite hi-hat ringing zone, solid to fadeStartTime then a fainter tail.
    void drawHiHatSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
};
