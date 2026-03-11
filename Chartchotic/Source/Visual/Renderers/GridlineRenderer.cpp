/*
    ==============================================================================

        GridlineRenderer.cpp
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "GridlineRenderer.h"

using namespace PositionConstants;

GridlineRenderer::GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void GridlineRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                                double windowStartTime, double windowEndTime,
                                uint width, uint height,
                                float posEnd,
                                float gridlinePosOffset, float gridZOffset,
                                float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    this->width = width;
    this->height = height;
    this->posEnd = posEnd;

    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto& gridline : gridlines)
    {
        double gridlineTime = gridline.time;
        Gridline gridlineType = gridline.type;

        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan) + gridlinePosOffset;

        if (normalizedPosition >= HIGHWAY_POS_START && normalizedPosition <= farFadeEnd)
        {
            juce::Image* markerImage = assetManager.getGridlineImage(gridlineType);

            if (markerImage != nullptr)
            {
                float fadeOpacity = calculateFarFade(normalizedPosition, farFadeEnd, farFadeLen, farFadeCurve);
                drawCallMap[static_cast<int>(DrawOrder::GRID)][0].push_back([=](juce::Graphics& g) {
                    this->drawGridline(g, normalizedPosition, markerImage, gridlineType, fadeOpacity, gridZOffset);
                });
            }
        }
    }
}

void GridlineRenderer::drawGridline(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType, float fadeOpacity, float gridZOffset)
{
    if (!markerImage) return;

    float opacity = 1.0f;
    switch (gridlineType) {
        case Gridline::MEASURE: opacity = MEASURE_OPACITY; break;
        case Gridline::BEAT: opacity = BEAT_OPACITY; break;
        case Gridline::HALF_BEAT: opacity = HALF_BEAT_OPACITY; break;
    }
    opacity *= fadeOpacity;

    const auto& fbCoords = activePart == Part::DRUMS
        ? PositionConstants::drumFretboardCoords
        : PositionConstants::guitarFretboardCoords;
    auto edge = getColumnEdge(position, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
    float gridWidth = edge.rightX - edge.leftX;
    auto perspParams = PositionConstants::getPerspectiveParams(activePart == Part::DRUMS);
    float gridHeight = gridWidth / perspParams.barNoteHeightRatio;

    // Scale Z offset by perspective (ratio of current width to strikeline width)
    float scaledZOffset = gridZOffset;
    if (std::abs(gridZOffset) > 0.001f)
    {
        auto strikeEdge = getColumnEdge(0.0f, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
        float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
        if (strikeWidth > 0.0f)
            scaledZOffset *= (gridWidth / strikeWidth);
    }

    juce::Rectangle<float> rect(edge.leftX, edge.centerY - gridHeight * 0.5f + scaledZOffset, gridWidth, gridHeight);
    g.setOpacity(opacity);
    g.drawImage(*markerImage, rect);
}
