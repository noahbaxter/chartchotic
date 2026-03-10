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
}

void HighwayComponent::paint(juce::Graphics& g)
{
    if (showHighway)
    {
        trackRenderer.paint(g, getWidth(), getHeight());
        trackRenderer.paintTexture(g, frameData.scrollOffset, getWidth(), getHeight());
    }

    sceneRenderer.paint(g, getWidth(), getHeight(),
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

void HighwayComponent::rebuildTrack()
{
    int w = getWidth();
    int h = getHeight();
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
