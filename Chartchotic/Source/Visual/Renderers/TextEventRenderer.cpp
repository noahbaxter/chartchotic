/*
    ==============================================================================

        TextEventRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TextEventRenderer.h"

using namespace PositionConstants;

// How much the top/bottom edges bow downward (fraction of band width)
static constexpr float EDGE_CURVATURE = -0.025f;

void TextEventRenderer::populate(DrawCallMap& drawCallMap,
                                 const std::vector<TimeBasedFlipRegion>& flipRegions,
                                 const GateLanes& lanes,
                                 double windowStartTime, double windowEndTime,
                                 uint width, uint height,
                                 float posEnd,
                                 float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    setFrame(activePart, width, height, posEnd);

    double windowTimeSpan = windowEndTime - windowStartTime;
    if (windowTimeSpan <= 0.0) return;

    const int lane = lanes.of(GateKind::DiscoFlip);
    if (lane < 0) return;
    const Gate gate { GateKind::DiscoFlip, lane, lanes.count };

    for (const auto& region : flipRegions)
    {
        // Start marker
        float startPos = (float)((region.startTime - windowStartTime) / windowTimeSpan);
        if (startPos >= HIGHWAY_POS_START && startPos <= farFadeEnd)
        {
            float fade = calculateFarFade(startPos, farFadeEnd, farFadeLen, farFadeCurve);
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, startPos, gate, fade](juce::Graphics& g) {
                    this->drawMarker(g, startPos, "DISCO START", gate, fade);
                });
        }

        // End marker — no lower bound so it scrolls past the strikeline
        float endPos = (float)((region.endTime - windowStartTime) / windowTimeSpan);
        if (endPos <= farFadeEnd)
        {
            float fade = calculateFarFade(endPos, farFadeEnd, farFadeLen, farFadeCurve);
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, endPos, gate, fade](juce::Graphics& g) {
                    this->drawMarker(g, endPos, "DISCO END", gate, fade);
                });
        }
    }
}

void TextEventRenderer::populateEventMarkers(DrawCallMap& drawCallMap,
                                              const TimeBasedEventMarkers& markers,
                                              const GateLanes& lanes,
                                              double windowStartTime, double windowEndTime,
                                              uint width, uint height,
                                              float posEnd,
                                              float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    setFrame(activePart, width, height, posEnd);

    double windowTimeSpan = windowEndTime - windowStartTime;
    if (windowTimeSpan <= 0.0) return;

    for (const auto& marker : markers)
    {
        const int lane = lanes.of(marker.kind);
        if (lane < 0) continue;

        float pos = (float)((marker.time - windowStartTime) / windowTimeSpan);
        if (pos >= HIGHWAY_POS_START && pos <= farFadeEnd)
        {
            float fade = calculateFarFade(pos, farFadeEnd, farFadeLen, farFadeCurve);
            juce::String label = marker.label;
            const Gate gate { marker.kind, lane, lanes.count };
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, pos, label, gate, fade](juce::Graphics& g) {
                    this->drawMarker(g, pos, label, gate, fade);
                });
        }
    }
}

void TextEventRenderer::drawMarker(juce::Graphics& g, float position, const juce::String& label,
                                   Gate gate, float fadeOpacity)
{
    const juce::Colour tint = gateColour(gate.kind);

    // The gate's share of the board: lane `gate.lane` of `gate.laneCount` equal slots.
    const float t0 = (float)gate.lane / (float)gate.laneCount;
    const float t1 = (float)(gate.lane + 1) / (float)gate.laneCount;

    if (PositionMath::bemaniMode)
    {
        // Flat horizontal marker matching Bemani gridline style
        auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), position, width, height,
                        PositionConstants::HIGHWAY_POS_START, posEnd);
        float boardWidth = edge.rightX - edge.leftX;
        float leftX = edge.leftX + boardWidth * t0;
        float bandWidth = boardWidth * (t1 - t0);
        float baseY = edge.centerY;

        // Band dimensions, from the whole board so every lane's gate is the same height
        float bandH = std::max(24.0f, boardWidth * 0.10f);
        float topY = baseY - bandH;
        float lineH = std::max(2.0f, bandH * 0.12f);

        // Translucent band fill
        g.setColour(tint.withAlpha(0.12f * fadeOpacity));
        g.fillRect(leftX, topY, bandWidth, bandH);

        // Bright base line (bottom edge of band)
        g.setColour(tint.withAlpha(0.8f * fadeOpacity));
        g.fillRect(leftX, baseY - lineH, bandWidth, lineH);

        // Dimmer top line
        g.setColour(tint.withAlpha(0.4f * fadeOpacity));
        g.fillRect(leftX, topY, bandWidth, std::max(1.0f, lineH * 0.5f));

        // Text label centered in band
        g.setColour(tint.withAlpha(0.95f * fadeOpacity));
        float fontSize = std::max(11.0f, bandH * 0.55f);
        g.setFont(juce::Font(fontSize, juce::Font::bold));
        g.drawFittedText(label, juce::Rectangle<float>(leftX, topY, bandWidth, bandH).toNearestInt(),
                         juce::Justification::centred, 2, GATE_LABEL_MIN_SCALE);
        return;
    }

    // --- Perspective mode: curved force field ---

    // Width from fretboard coords, same scale as gridlines
    const auto& fbCoords = *currentConfig->fretboardCoords;
    auto laneEdge = getColumnEdge(position, fbCoords, TEXT_EVENT_WIDTH_SCALE);
    float boardWidth = laneEdge.rightX - laneEdge.leftX;
    float leftX  = laneEdge.leftX + boardWidth * t0;
    float rightX = laneEdge.leftX + boardWidth * t1;
    float baseY  = laneEdge.centerY;
    float bandWidth = rightX - leftX;

    // Wall height proportional to the board's width (perspective-correct), not the lane's, so
    // sharing the board with other kinds narrows a gate without also flattening it.
    float wallHeight = boardWidth * 0.28f;
    float topY = baseY - wallHeight;

    // Curvature: subtle bow on top and bottom edges, as one arc across the whole board. A lane
    // draws its own stretch of that arc: where the arc is at each of its ends, and the control
    // point that carries the same curve between them.
    float curvePx = boardWidth * EDGE_CURVATURE;
    float bowLeft  = 2.0f * t0 * (1.0f - t0) * curvePx;
    float bowRight = 2.0f * t1 * (1.0f - t1) * curvePx;
    float bowMid   = (t0 + t1 - 2.0f * t0 * t1) * curvePx;
    float midX = (leftX + rightX) * 0.5f;

    // --- Build the force field shape ---
    // Left side (straight up), top edge (curved), right side (straight down), bottom edge (curved)
    juce::Path wall;
    wall.startNewSubPath(leftX, baseY + bowLeft);
    wall.lineTo(leftX, topY + bowLeft);
    wall.quadraticTo(midX, topY + bowMid, rightX, topY + bowRight);
    wall.lineTo(rightX, baseY + bowRight);
    wall.quadraticTo(midX, baseY + bowMid, leftX, baseY + bowLeft);
    wall.closeSubPath();

    // Gradient fill: brighter at base, fading toward top
    juce::ColourGradient gradient(
        tint.withAlpha(0.45f * fadeOpacity), 0.0f, baseY,
        tint.withAlpha(0.10f * fadeOpacity), 0.0f, topY,
        false);
    g.setGradientFill(gradient);
    g.fillPath(wall);

    // Base edge line (bright, sits on highway surface, curved)
    float lineThick = juce::jmax(2.0f, wallHeight * 0.07f);
    g.setColour(tint.withAlpha(0.9f * fadeOpacity));
    juce::Path baseLine;
    baseLine.startNewSubPath(leftX, baseY + bowLeft);
    baseLine.quadraticTo(midX, baseY + bowMid, rightX, baseY + bowRight);
    g.strokePath(baseLine, juce::PathStrokeType(lineThick));

    // Top edge line (thinner, curved)
    float topLineThick = juce::jmax(1.0f, lineThick * 0.5f);
    g.setColour(tint.withAlpha(0.5f * fadeOpacity));
    juce::Path topLine;
    topLine.startNewSubPath(leftX, topY + bowLeft);
    topLine.quadraticTo(midX, topY + bowMid, rightX, topY + bowRight);
    g.strokePath(topLine, juce::PathStrokeType(topLineThick));

    // Side edge lines (straight vertical)
    float sideThick = juce::jmax(1.0f, lineThick * 0.4f);
    g.setColour(tint.withAlpha(0.35f * fadeOpacity));
    g.drawLine(leftX, baseY + bowLeft, leftX, topY + bowLeft, sideThick);
    g.drawLine(rightX, baseY + bowRight, rightX, topY + bowRight, sideThick);

    // Text label centered in the wall, squeezed or wrapped to fit a narrow lane
    g.setColour(tint.withAlpha(0.95f * fadeOpacity));
    float fontSize = juce::jmax(12.0f, wallHeight * 0.32f);
    g.setFont(juce::Font(fontSize, juce::Font::bold));
    juce::Rectangle<float> textRect(leftX, topY, bandWidth, wallHeight);
    g.drawFittedText(label, textRect.toNearestInt(), juce::Justification::centred,
                     2, GATE_LABEL_MIN_SCALE);
}
