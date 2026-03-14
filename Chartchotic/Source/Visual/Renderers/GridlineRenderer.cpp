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
                float fadeOpacity = PositionMath::bemaniMode ? 1.0f : calculateFarFade(normalizedPosition, farFadeEnd, farFadeLen, farFadeCurve);
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

    if (PositionMath::bemaniMode)
    {
#ifdef DEBUG
        opacity = std::min(1.0f, opacity * debugBemaniGridlineBoost);
#else
        opacity = std::min(1.0f, opacity * BEMANI_GRIDLINE_BOOST);
#endif
        // Flat horizontal line instead of marker image
        bool isDrums = activePart == Part::DRUMS;
        auto edge = PositionMath::getFretboardEdge(isDrums, position, width, height,
                        PositionConstants::HIGHWAY_POS_START, posEnd);
        float lineH = std::max(1.0f, (float)width * 0.003f);
        float lineY = edge.centerY - lineH * 0.5f;

        // Gradient: brighter at center, fades at edges
        juce::ColourGradient grad(
            juce::Colours::white.withAlpha(opacity), (edge.leftX + edge.rightX) * 0.5f, lineY,
            juce::Colours::white.withAlpha(opacity * 0.3f), edge.leftX, lineY, false);
        grad.addColour(0.0, juce::Colours::white.withAlpha(opacity * 0.3f));
        grad.addColour(0.5, juce::Colours::white.withAlpha(opacity));
        grad.addColour(1.0, juce::Colours::white.withAlpha(opacity * 0.3f));
        g.setGradientFill(grad);
        g.fillRect(edge.leftX, lineY, edge.rightX - edge.leftX, lineH);
        return;
    }

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
