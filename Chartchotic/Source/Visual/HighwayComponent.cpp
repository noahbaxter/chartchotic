/*
    ==============================================================================

        HighwayComponent.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "HighwayComponent.h"

HighwayComponent::HighwayComponent(juce::ValueTree& state, AssetManager& assetManager)
    : state(state),
      assetManager(assetManager),
      sceneRenderer(state, assetManager),
      trackRenderer(state)
{
#ifdef DEBUG
    debugColour = juce::Colour::fromHSV(
        juce::Random::getSystemRandom().nextFloat(), 0.4f, 0.4f, 1.0f);
#endif
}

void HighwayComponent::paint(juce::Graphics& g)
{
#ifdef DEBUG
    if (debugColour != juce::Colour()) g.fillAll(debugColour);
#endif

    // Draw converging track polygon in the overflow region (component coordinates, pre-translation)
    if (topOverflow > 0 && showHighway)
    {
        bool isDrums = isPart(state, Part::DRUMS);
        const auto& fbw = isDrums ? sceneRenderer.fbWidthsDrums : sceneRenderer.fbWidthsGuitar;

        // Get the track edge at the top of the original viewport (position that maps to y≈0)
        // and at the far end (vanishing point). Sample a few positions for a smooth polygon.
        constexpr int NUM_POINTS = 8;
        float posEnd = sceneRenderer.highwayPosEnd;
        float farEnd = sceneRenderer.farFadeEnd;

        juce::Path overflowPoly;
        bool pathStarted = false;

        // Right edges going up (toward vanishing point)
        for (int i = 0; i <= NUM_POINTS; i++)
        {
            float pos = posEnd + (farEnd - posEnd) * (float)i / (float)NUM_POINTS;
            auto edge = PositionMath::getFretboardEdge(isDrums, pos, renderWidth, renderHeight,
                                                        fbw.near, fbw.mid, fbw.far,
                                                        PositionConstants::HIGHWAY_POS_START, posEnd);
            float cy = edge.centerY + (float)topOverflow; // convert to component coords
            if (!pathStarted) { overflowPoly.startNewSubPath(edge.rightX, cy); pathStarted = true; }
            else overflowPoly.lineTo(edge.rightX, cy);
        }
        // Left edges coming back down
        for (int i = NUM_POINTS; i >= 0; i--)
        {
            float pos = posEnd + (farEnd - posEnd) * (float)i / (float)NUM_POINTS;
            auto edge = PositionMath::getFretboardEdge(isDrums, pos, renderWidth, renderHeight,
                                                        fbw.near, fbw.mid, fbw.far,
                                                        PositionConstants::HIGHWAY_POS_START, posEnd);
            float cy = edge.centerY + (float)topOverflow;
            overflowPoly.lineTo(edge.leftX, cy);
        }
        overflowPoly.closeSubPath();

        g.setColour(juce::Colour(0xFF111111));
        g.fillPath(overflowPoly);
    }

    // Translate so renderers see their original coordinate space
    if (topOverflow > 0)
        g.addTransform(juce::AffineTransform::translation(0.0f, (float)topOverflow));

    if (showHighway)
    {
        trackRenderer.paint(g, renderWidth, renderHeight);
        trackRenderer.paintTexture(g, frameData.scrollOffset, renderWidth, renderHeight);
    }

    sceneRenderer.paint(g, renderWidth, renderHeight,
                        frameData.trackWindow, frameData.sustainWindow, frameData.gridlines,
                        frameData.windowStartTime, frameData.windowEndTime, frameData.isPlaying);
}

void HighwayComponent::resized()
{
    startTimer(resizeDebounceMs);
}

void HighwayComponent::timerCallback()
{
    stopTimer();
    rebuildTrack();
}

void HighwayComponent::setFrameData(const HighwayFrameData& data)
{
    frameData = data;
}

void HighwayComponent::updateOverflow()
{
    if (renderWidth <= 0 || renderHeight <= 0) return;
    bool isDrums = isPart(state, Part::DRUMS);
    const auto& fbw = isDrums ? sceneRenderer.fbWidthsDrums : sceneRenderer.fbWidthsGuitar;
    auto farEdge = PositionMath::getFretboardEdge(
        isDrums, sceneRenderer.farFadeEnd, renderWidth, renderHeight,
        fbw.near, fbw.mid, fbw.far,
        PositionConstants::HIGHWAY_POS_START, sceneRenderer.highwayPosEnd);
    topOverflow = std::max(0, (int)std::ceil(-farEdge.centerY));
}

void HighwayComponent::rebuildTrack()
{
    int w = renderWidth;
    int h = renderHeight;
    if (w <= 0 || h <= 0) return;

    bool isDrums = isPart(state, Part::DRUMS);
    const auto& fbw = isDrums ? sceneRenderer.fbWidthsDrums : sceneRenderer.fbWidthsGuitar;

    sceneRenderer.rescaleAssets(w);

    trackRenderer.rebuild(w, h,
                          sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve,
                          fbw.near, fbw.mid, fbw.far, sceneRenderer.highwayPosEnd);

    sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &trackRenderer.getLayerImage(TrackRenderer::STRIKELINE));
    sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &trackRenderer.getLayerImage(TrackRenderer::LANE_LINES));
    sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &trackRenderer.getLayerImage(TrackRenderer::SIDEBARS));
    sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &trackRenderer.getLayerImage(TrackRenderer::CONNECTORS));
}
