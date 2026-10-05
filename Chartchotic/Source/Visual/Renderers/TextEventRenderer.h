/*
    ==============================================================================

        TextEventRenderer.h
        Author:  Noah Baxter

        Renders text event markers (e.g. disco flip start/end) as translucent
        "force field" bands on the highway.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Geometry/PositionConstants.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "HighwayRenderer.h"

// Which lane each gate kind draws in this frame, left to right. -1 is a kind that is hidden
// or has nothing to show, and takes no room.
struct GateLanes
{
    std::array<int, GATE_KIND_COUNT> index;
    int count = 0;

    GateLanes() { index.fill(-1); }
    int of(GateKind kind) const { return index[(size_t)kind]; }
};

class TextEventRenderer : public HighwayRenderer
{
public:
    void populate(DrawCallMap& drawCallMap,
                  const std::vector<TimeBasedFlipRegion>& flipRegions, const GateLanes& lanes,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

    void populateEventMarkers(DrawCallMap& drawCallMap,
                              const TimeBasedEventMarkers& markers, const GateLanes& lanes,
                              double windowStartTime, double windowEndTime,
                              uint width, uint height,
                              float posEnd,
                              float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    struct Gate { GateKind kind; int lane; int laneCount; };

    void drawMarker(juce::Graphics& g, float position, const juce::String& label,
                    Gate gate, float fadeOpacity);
};
