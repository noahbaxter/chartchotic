/*
    ==============================================================================

        TrackRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TrackRenderer.h"

using namespace PositionConstants;

TrackRenderer::TrackRenderer(juce::ValueTree& state)
    : state(state)
{
    // Load layer images from BinaryData
    sidebarsImage = juce::ImageCache::getFromMemory(BinaryData::sidebars_png, BinaryData::sidebars_pngSize);
    laneLinesGuitarImage = juce::ImageCache::getFromMemory(BinaryData::lane_lines_guitar_png, BinaryData::lane_lines_guitar_pngSize);
    laneLinesDrumsImage = juce::ImageCache::getFromMemory(BinaryData::lane_lines_drums_png, BinaryData::lane_lines_drums_pngSize);
    strikelineGuitarImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_guitar_png, BinaryData::strikeline_guitar_pngSize);
    strikelineDrumsImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_drums_png, BinaryData::strikeline_drums_pngSize);
    strikelineConnectorsImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_connectors_png, BinaryData::strikeline_connectors_pngSize);
    kickSmashersImage = juce::ImageCache::getFromMemory(BinaryData::kick_smashers_png, BinaryData::kick_smashers_pngSize);
}

void TrackRenderer::paint(juce::Graphics& g)
{
    if (fadedTrackImage.isValid())
        g.drawImageAt(fadedTrackImage, 0, 0);
}

void TrackRenderer::paintTexture(juce::Graphics& g, float scrollOffset)
{
    if (!textureEnabled || !prebaked.valid)
        return;

    int w = cached.width;
    int h = cached.height;
    int tileH = prebaked.tileHeight;

    // Ensure offscreen buffer
    if (offscreen.getWidth() != w || offscreen.getHeight() != h)
        offscreen = juce::Image(juce::Image::ARGB, w, h, true);
    else
        offscreen.clear({0, 0, w, h});

    juce::Image::BitmapData dst(offscreen, juce::Image::BitmapData::writeOnly);
    juce::Image::BitmapData src(prebaked.mipAtlas, juce::Image::BitmapData::readOnly);

    for (int y = prebaked.yMin; y <= prebaked.yMax; y++)
    {
        auto& sl = prebaked.scanlines[y];
        if (sl.alpha <= 0.0f || sl.rightX <= sl.leftX) continue;

        int span = sl.rightX - sl.leftX;

        // Pick finest mip level that covers the span (never downsample > 2:1)
        int mipIdx = 0;
        for (int m = 1; m < (int)prebaked.mips.size(); m++)
        {
            if (prebaked.mips[m].width >= span)
                mipIdx = m;
            else
                break;
        }
        auto& mip = prebaked.mips[mipIdx];

        float texV = std::fmod((sl.texV + scrollOffset) * textureScale, 1.0f);
        if (texV < 0.0f) texV += 1.0f;

        int srcRow = mip.rowOffset + (int)(texV * tileH) % tileH;
        uint8_t alphaScale = (uint8_t)(sl.alpha * 255.0f);

        auto* srcLine = (juce::PixelARGB*)src.getLinePointer(srcRow);
        auto* dstLine = (juce::PixelARGB*)dst.getLinePointer(y);

        float invSpan = 1.0f / (float)span;

        for (int x = sl.leftX; x <= sl.rightX; x++)
        {
            int srcX = (int)((float)(x - sl.leftX) * invSpan * (float)(mip.width - 1));
            juce::PixelARGB px = srcLine[srcX];
            px.multiplyAlpha(alphaScale);
            dstLine[x] = px;
        }
    }

    // Composite — scanline bounds already define the fretboard shape, no clip path needed
    g.setOpacity(textureOpacity);
    g.drawImageAt(offscreen, 0, 0);
}

void TrackRenderer::compositeLayers(juce::Image& target, int w, int h, bool isDrums,
                                     float wNear, float wMid, float wFar, float posEnd)
{
    juce::Graphics g(target);

    // Fill fretboard polygon with dark background
    juce::Path fretboardPath;
    float effectiveEnd = std::max(posEnd, cached.fadeEnd);

    constexpr int NUM_POLY_POINTS = 60;
    float posRange = effectiveEnd - HIGHWAY_POS_START;

    struct EdgePoint { float leftX, rightX, centerY; };
    std::array<EdgePoint, NUM_POLY_POINTS + 1> polyEdges;

    for (int i = 0; i <= NUM_POLY_POINTS; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)NUM_POLY_POINTS;
        auto edge = PositionMath::getFretboardEdge(isDrums, pos, w, h,
                                                    wNear, wMid, wFar,
                                                    HIGHWAY_POS_START, posEnd);
        polyEdges[i] = {edge.leftX, edge.rightX, edge.centerY};
    }

    fretboardPath.startNewSubPath(polyEdges[0].rightX, polyEdges[0].centerY);
    for (int i = 1; i <= NUM_POLY_POINTS; i++)
        fretboardPath.lineTo(polyEdges[i].rightX, polyEdges[i].centerY);
    for (int i = NUM_POLY_POINTS; i >= 0; i--)
        fretboardPath.lineTo(polyEdges[i].leftX, polyEdges[i].centerY);
    fretboardPath.closeSubPath();

    g.setColour(juce::Colour(0xFF111111));
    g.fillPath(fretboardPath);
}

void TrackRenderer::bakeLayerImage(juce::Image& out, const juce::Image& src, const LayerTransform& t,
                                    int w, int h, bool isDrums, bool tiled,
                                    float farFadeEnd, float farFadeLen, float farFadeCurve,
                                    float wNear, float wMid, float wFar, float posEnd)
{
    if (!src.isValid()) { out = {}; return; }

    out = juce::Image(juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g(out);
        float imgAspect = (float)src.getWidth() / (float)src.getHeight();
        float drawW = (float)w * t.scale;
        float drawH = drawW / imgAspect;
        float drawX = ((float)w - drawW) * 0.5f + t.xOffset * (float)w;
        float drawY = (float)h - drawH + t.yOffset * (float)h;

        if (tiled)
        {
            float tileBottom = drawY + drawH;
            float scale = 1.0f;
            while (tileBottom > 0)
            {
                float tileW = drawW * scale;
                float tileH = drawH * scale;
                float tileX = ((float)w - tileW) * 0.5f + t.xOffset * (float)w;
                float tileY = tileBottom - tileH;

                g.drawImage(src, {tileX, tileY, tileW, tileH});
                tileBottom -= tileH * tileStep;
                scale *= tileScaleStep;
            }
        }
        else
        {
            g.drawImage(src, {drawX, drawY, drawW, drawH});
        }
    }

    applyFarFade(out, w, h, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 wNear, wMid, wFar, posEnd);
}

void TrackRenderer::rebuild(int width, int height,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               float wNear, float wMid, float wFar, float posEnd)
{
    if (width <= 0 || height <= 0) return;

    bool isDrums = isPart(state, Part::DRUMS);

    // Bake dark fill base
    fadedTrackImage = juce::Image(juce::Image::ARGB, width, height, true);
    compositeLayers(fadedTrackImage, width, height, isDrums, wNear, wMid, wFar, posEnd);
    applyFarFade(fadedTrackImage, width, height, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 wNear, wMid, wFar, posEnd);

    // Bake individual overlay layers (drawn at interleaved z-positions by SceneRenderer)
    auto* layers = isDrums ? layersDrums : layersGuitar;
    bakeLayerImage(layerImages[SIDEBARS], sidebarsImage, layers[SIDEBARS],
                   width, height, isDrums, true, farFadeEnd, farFadeLen, farFadeCurve, wNear, wMid, wFar, posEnd);
    bakeLayerImage(layerImages[LANE_LINES], isDrums ? laneLinesDrumsImage : laneLinesGuitarImage, layers[LANE_LINES],
                   width, height, isDrums, true, farFadeEnd, farFadeLen, farFadeCurve, wNear, wMid, wFar, posEnd);
    bakeLayerImage(layerImages[STRIKELINE], isDrums ? strikelineDrumsImage : strikelineGuitarImage, layers[STRIKELINE],
                   width, height, isDrums, false, farFadeEnd, farFadeLen, farFadeCurve, wNear, wMid, wFar, posEnd);
    bakeLayerImage(layerImages[CONNECTORS], isDrums ? kickSmashersImage : strikelineConnectorsImage, layers[CONNECTORS],
                   width, height, isDrums, false, farFadeEnd, farFadeLen, farFadeCurve, wNear, wMid, wFar, posEnd);

    // Rebuild cached geometry for texture scanline LUT
    float effectiveEnd = std::max(posEnd, farFadeEnd);

    if (width != cached.width || height != cached.height ||
        isDrums != cached.isDrums ||
        wNear != cached.wNear || wMid != cached.wMid || wFar != cached.wFar ||
        posEnd != cached.posEnd || farFadeEnd != cached.fadeEnd ||
        farFadeLen != cached.fadeLen || farFadeCurve != cached.fadeCurve)
    {
        float posRange = effectiveEnd - HIGHWAY_POS_START;

        auto edgeNear = PositionMath::getFretboardEdge(isDrums, HIGHWAY_POS_START, width, height,
                                                        wNear, wMid, wFar, HIGHWAY_POS_START, posEnd);
        auto edgeFar = PositionMath::getFretboardEdge(isDrums, effectiveEnd, width, height,
                                                       wNear, wMid, wFar, HIGHWAY_POS_START, posEnd);
        int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
        cached.stripCount = std::clamp(pixelHeight / PIXELS_PER_STRIP, MIN_STRIPS, MAX_STRIPS);

        cached.edges.resize(cached.stripCount + 1);
        for (int i = 0; i <= cached.stripCount; i++)
        {
            float pos = HIGHWAY_POS_START + posRange * (float)i / (float)cached.stripCount;
            cached.edges[i] = {
                PositionMath::getFretboardEdge(isDrums, pos, width, height,
                                               wNear, wMid, wFar, HIGHWAY_POS_START, posEnd),
                pos
            };
        }

        cached.width = width;
        cached.height = height;
        cached.isDrums = isDrums;
        cached.wNear = wNear;
        cached.wMid = wMid;
        cached.wFar = wFar;
        cached.posEnd = posEnd;
        cached.fadeEnd = farFadeEnd;
        cached.fadeLen = farFadeLen;
        cached.fadeCurve = farFadeCurve;
    }

    rebuildPrebake();
}

void TrackRenderer::rebuildPrebake()
{
    if (cached.stripCount == 0 || cached.width <= 0 || cached.height <= 0 || !sourceTexture.isValid())
    {
        prebaked.valid = false;
        return;
    }

    int w = cached.width;
    int h = cached.height;
    int texH = sourceTexture.getHeight();

    // 1. Build per-scanline LUT by interpolating between cached edge pairs
    prebaked.scanlines.assign(h, {0.0f, 0.0f, 0, 0});
    prebaked.yMin = h;
    prebaked.yMax = 0;

    for (int i = 0; i < cached.stripCount; i++)
    {
        auto& [nearC, nearPos] = cached.edges[i];      // nearer to strikeline (bottom, large Y)
        auto& [farC, farPos] = cached.edges[i + 1];    // farther from strikeline (top, small Y)

        int yStart = std::max(0, (int)std::ceil(farC.centerY));
        int yEnd = std::min(h - 1, (int)std::floor(nearC.centerY));

        float yRange = nearC.centerY - farC.centerY;

        for (int y = yStart; y <= yEnd; y++)
        {
            float t = (yRange > 0.0f) ? (nearC.centerY - (float)y) / yRange : 0.0f;
            t = juce::jlimit(0.0f, 1.0f, t);

            float pos = nearPos + t * (farPos - nearPos);
            float lx = nearC.leftX + t * (farC.leftX - nearC.leftX);
            float rx = nearC.rightX + t * (farC.rightX - nearC.rightX);

            prebaked.scanlines[y].texV = 1.0f - pos;
            prebaked.scanlines[y].alpha = calculateFarFade(pos, cached.fadeEnd, cached.fadeLen, cached.fadeCurve);
            prebaked.scanlines[y].leftX = std::max(0, (int)lx);
            prebaked.scanlines[y].rightX = std::min(w - 1, (int)rx);

            prebaked.yMin = std::min(prebaked.yMin, y);
            prebaked.yMax = std::max(prebaked.yMax, y);
        }
    }

    // 2. Pre-bake mip atlas: source texture at multiple horizontal widths (halving each level)
    prebaked.tileHeight = texH * PREBAKE_QUALITY;
    prebaked.mips.clear();

    int mipW = w;
    int rowOffset = 0;
    while (mipW >= MIN_MIP_WIDTH)
    {
        prebaked.mips.push_back({rowOffset, mipW});
        rowOffset += prebaked.tileHeight;
        mipW /= 2;
    }

    prebaked.mipAtlas = juce::Image(juce::Image::ARGB, w, rowOffset, true);
    {
        juce::Graphics tg(prebaked.mipAtlas);
        for (auto& mip : prebaked.mips)
            tg.drawImage(sourceTexture, {0.0f, (float)mip.rowOffset, (float)mip.width, (float)prebaked.tileHeight});
    }

    prebaked.valid = true;
}

void TrackRenderer::setTexture(const juce::Image& texture)
{
    sourceTexture = texture;
    textureEnabled = texture.isValid();
    rebuildPrebake();
}

void TrackRenderer::clearTexture()
{
    sourceTexture = {};
    textureEnabled = false;
    prebaked.valid = false;
}
