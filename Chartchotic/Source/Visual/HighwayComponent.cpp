/*
    ==============================================================================

        HighwayComponent.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "HighwayComponent.h"
#include "../UI/Theme.h"

HighwayComponent::HighwayComponent(juce::ValueTree& state, AssetManager& assetManager)
    : state(state),
      assetManager(assetManager),
      sceneRenderer(state, assetManager),
      trackRenderer(state)
{
    // Sync activePart from state
    setActivePart(getPartFromState(state));
#ifdef DEBUG
    debugColour = juce::Colour::fromHSV(
        juce::Random::getSystemRandom().nextFloat(), 0.4f, 0.4f, 1.0f);
#endif
}

void HighwayComponent::setActivePart(Part part)
{
    activePart = part;
    sceneRenderer.activePart = part;
    trackRenderer.activePart = part;
}

void HighwayComponent::paint(juce::Graphics& g)
{
#ifdef DEBUG
    if (showDebugColour && debugColour != juce::Colour()) g.fillAll(debugColour);
#endif

    // During resize debounce, render at baked dimensions and scale to fit.
    // During highway-length debounce (same dimensions), fill+fade is fresh
    // but layer overlays and texture use stale baked images.
    bool debouncing = isTimerRunning() && bakedRenderW > 0 && bakedRenderH > 0;

    int w        = debouncing ? bakedRenderW  : renderWidth;
    int h        = debouncing ? bakedRenderH  : renderHeight;
    int overflow = debouncing ? bakedOverflow  : topOverflow;
    int totalH   = h + overflow;

    if (stretchToFill && !PositionMath::bemaniMode)
    {
        // Non-uniform stretch to fill all available space (perspective only)
        float sx = (float)getWidth()  / (float)w;
        float sy = (float)getHeight() / (float)totalH;
        g.addTransform(juce::AffineTransform::scale(sx, sy));
    }
    else
    {
        // Uniform scale to maximize height, centered horizontally, bottom-anchored.
        // Bemani always uses uniform scale to avoid aspect ratio distortion.
        float scale = std::min((float)getWidth() / (float)w,
                               (float)getHeight() / (float)totalH);
        float scaledW = (float)w * scale;
        float scaledH = (float)totalH * scale;
        float offsetX = ((float)getWidth() - scaledW) / 2.0f;
        float offsetY = (float)getHeight() - scaledH;
        g.addTransform(juce::AffineTransform(scale, 0.0f, offsetX, 0.0f, scale, offsetY));
    }

    // Track fill, layers, and texture are baked at totalH (viewport + overflow).
    // Draw them in component coordinates (no translation needed).
    if (showHighway)
    {
        trackRenderer.paint(g, w, totalH);
        trackRenderer.paintTexture(g, frameData.scrollOffset, w, totalH);
    }
    // Bemani overlay (lane dividers, strikeline) always draws — independent of highway texture toggle
    if (showHighway || PositionMath::bemaniMode)
        trackRenderer.paintBemaniOverlay(g, w, totalH);

    // Translate so scene renderer (notes, gridlines, etc.) sees viewport coordinates.
    // Overlay images are drawn at (0, -overlayYOffset) to extend into the overflow area.
    if (overflow > 0)
        g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));

    sceneRenderer.paint(g, w, h,
                        frameData.trackWindow, frameData.sustainWindow, frameData.gridlines,
                        frameData.flipRegions, frameData.eventMarkers,
                        frameData.windowStartTime, frameData.windowEndTime, frameData.isPlaying);

    // Bemani sidebar masks — drawn after notes/sustains to clip overflow.
    // Must cover the full component height. In Bemani mode overflow=0 so h=totalH.
    if (PositionMath::bemaniMode)
    {
        // Undo the overflow translation to draw in component coordinates
        if (overflow > 0)
            g.addTransform(juce::AffineTransform::translation(0.0f, -(float)overflow));
        trackRenderer.paintBemaniSidebars(g, w, totalH);
        if (overflow > 0)
            g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));
    }

    // Disco ball indicator when disco flip is active at current playhead
    if (frameData.discoFlipActive)
    {
        auto* discoBall = assetManager.getDiscoBallImage();
        if (discoBall != nullptr && discoBall->isValid())
        {
            int size = juce::jmax(32, w / 8);
            int margin = size / 4;
            juce::Rectangle<float> dest((float)(w - size - margin), (float)margin,
                                         (float)size, (float)size);
            g.setOpacity(0.7f);
            g.drawImage(*discoBall, dest);
            g.setOpacity(1.0f);
        }
    }
}

// Part label overlay data — icon + display name per instrument
struct PartLabelInfo {
    const char* imgData;
    int imgSize;
    const char* label;
};

static PartLabelInfo getPartLabelInfo(Part part)
{
    switch (part)
    {
        case Part::DRUMS:       return { BinaryData::icon_drums_png,  BinaryData::icon_drums_pngSize,  "Drums" };
        case Part::BASS:        return { BinaryData::icon_bass_png,   BinaryData::icon_bass_pngSize,   "Bass" };
        case Part::GHL_BASS:    return { BinaryData::icon_bass_png,   BinaryData::icon_bass_pngSize,   "GHL Bass" };
        case Part::PRO_BASS:    return { BinaryData::icon_bass_png,   BinaryData::icon_bass_pngSize,   "Pro Bass" };
        case Part::KEYS:        return { BinaryData::icon_keys_png,   BinaryData::icon_keys_pngSize,   "Keys" };
        case Part::GHL_GUITAR:  return { BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize, "GHL Guitar" };
        case Part::PRO_GUITAR:  return { BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize, "Pro Guitar" };
        case Part::GUITAR:
        default:                return { BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize, "Guitar" };
    }
}

void HighwayComponent::paintOverChildren(juce::Graphics& g)
{
    if (!showPartLabel) return;

    auto info = getPartLabelInfo(activePart);
    juce::String label = info.label;

    juce::Image icon;
    if (info.imgData != nullptr)
        icon = juce::ImageCache::getFromMemory(info.imgData, info.imgSize);
    bool hasIcon = icon.isValid();

    float s = (float)getWidth() / 600.0f;
    int iconSize = hasIcon ? juce::roundToInt(40.0f * s) : 0;
    int pad = juce::roundToInt(10.0f * s);
    float fontSize = 18.0f * s;
    auto font = Theme::getUIFont(fontSize);
    int textW = (int)font.getStringWidthFloat(label);
    int totalW = (hasIcon ? iconSize + pad : 0) + textW + pad * 2;
    int totalH = juce::jmax(iconSize, juce::roundToInt(fontSize * 1.5f)) + pad * 2;

    int x = (getWidth() - totalW) / 2;
    int y = getHeight() - totalH - pad;

    auto pillBounds = juce::Rectangle<float>((float)x, (float)y, (float)totalW, (float)totalH);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(pillBounds, 6.0f * s);

    float textX = pillBounds.getX() + pad;

    if (hasIcon)
    {
        auto iconBounds = juce::Rectangle<float>(
            pillBounds.getX() + pad, pillBounds.getCentreY() - iconSize * 0.5f,
            (float)iconSize, (float)iconSize);
        g.setOpacity(0.9f);
        g.drawImage(icon, iconBounds);
        g.setOpacity(1.0f);
        textX += iconSize + pad * 0.5f;
    }

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(font);
    auto textBounds = juce::Rectangle<float>(
        textX, pillBounds.getY(),
        pillBounds.getRight() - textX - pad,
        (float)totalH);
    g.drawText(label, textBounds, hasIcon ? juce::Justification::centredLeft : juce::Justification::centred);
}

void HighwayComponent::resized()
{
    if (PositionMath::bemaniMode)
    {
        // No baked assets in Bemani mode — rebuild immediately, no debounce
        rebuildTrack();
    }
    else
    {
        startTimer(rebuildDebounceMs);
    }
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
    if (PositionMath::bemaniMode) { topOverflow = 0; return; }
    bool isDrums = isDrumLike(activePart);
    auto farEdge = PositionMath::getFretboardEdge(
        isDrums, sceneRenderer.farFadeEnd, renderWidth, renderHeight,
        PositionConstants::HIGHWAY_POS_START, sceneRenderer.highwayPosEnd);
    topOverflow = std::max(0, (int)std::ceil(-farEdge.centerY));
}

void HighwayComponent::rebuildTrack()
{
    int w = renderWidth;
    int h = renderHeight;
    if (w <= 0 || h <= 0) return;

    // Recompute overflow in case perspective params changed
    int prevOverflow = topOverflow;
    updateOverflow();

    bool isDrums = isDrumLike(activePart);

    sceneRenderer.rescaleAssets(w);
    sceneRenderer.overlayYOffset = topOverflow;

    // Pass lane coords to TrackRenderer for perspective-projected lane lines
    trackRenderer.setLaneCoords(
        isDrums ? sceneRenderer.drumLaneCoordsLocal : sceneRenderer.guitarLaneCoordsLocal,
        isDrums ? (int)PositionConstants::DRUM_LANE_COUNT : (int)PositionConstants::GUITAR_LANE_COUNT);

    trackRenderer.rebuild(w, h, topOverflow,
                          sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve,
                          sceneRenderer.highwayPosEnd);

    // Always reset overlays — Bemani mode uses programmatic drawing, perspective uses baked images
    sceneRenderer.clearOverlays();
    if (PositionMath::bemaniMode)
    {
        // Inject rail draw call into the scene's draw order so it layers correctly
        int rw = w, rh = h + topOverflow;
        sceneRenderer.setCustomDrawCall(DrawOrder::TRACK_SIDEBARS,
            [this, rw, rh](juce::Graphics& g) { trackRenderer.paintBemaniRails(g, rw, rh); });
    }
    else
    {
        sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &trackRenderer.getLayerImage(TrackRenderer::STRIKELINE));
        sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &trackRenderer.getLayerImage(TrackRenderer::LANE_LINES));
        sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &trackRenderer.getLayerImage(TrackRenderer::SIDEBARS));
        sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &trackRenderer.getLayerImage(TrackRenderer::CONNECTORS));
    }

    bakedRenderW = w;
    bakedRenderH = h;
    bakedOverflow = topOverflow;

    if (topOverflow != prevOverflow && onOverflowChanged) onOverflowChanged();
}
