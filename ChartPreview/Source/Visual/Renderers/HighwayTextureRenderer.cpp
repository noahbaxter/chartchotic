/*
  ==============================================================================

    HighwayTextureRenderer.cpp
    Author:  Noah Baxter

    Highway texture overlay rendering, extracted from HighwayRenderer.

  ==============================================================================
*/

#include "HighwayTextureRenderer.h"

using namespace PositionConstants;

void HighwayTextureRenderer::drawTexture(juce::Graphics& g, const Params& p)
{
    int texW = texture.getWidth();
    int texH = texture.getHeight();
    if (texW == 0 || texH == 0) return;

    float effectiveEnd = std::max(p.highwayPosEnd, p.farFadeEnd);

    // Rebuild cached geometry when inputs change
    if (p.width != cached.width || p.height != cached.height ||
        p.isDrums != cached.isDrums ||
        p.wNear != cached.wNear || p.wMid != cached.wMid || p.wFar != cached.wFar ||
        p.highwayPosEnd != cached.posEnd || p.farFadeEnd != cached.fadeEnd)
    {
        float posRange = effectiveEnd - HIGHWAY_POS_START;

        auto edgeNear = PositionMath::getFretboardEdge(p.isDrums, HIGHWAY_POS_START, p.width, p.height, p.wNear, p.wMid, p.wFar, HIGHWAY_POS_START, effectiveEnd);
        auto edgeFar = PositionMath::getFretboardEdge(p.isDrums, effectiveEnd, p.width, p.height, p.wNear, p.wMid, p.wFar, HIGHWAY_POS_START, effectiveEnd);
        int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
        cached.stripCount = std::clamp(pixelHeight / 4, (int)MIN_STRIPS, 150);

        cached.edges.resize(cached.stripCount + 1);
        for (int i = 0; i <= cached.stripCount; i++)
        {
            float pos = HIGHWAY_POS_START + posRange * (float)i / (float)cached.stripCount;
            cached.edges[i] = {PositionMath::getFretboardEdge(p.isDrums, pos, p.width, p.height, p.wNear, p.wMid, p.wFar, HIGHWAY_POS_START, effectiveEnd), pos};
        }

        cached.fretboardPath = PositionMath::getFretboardPath(p.isDrums, HIGHWAY_POS_START, effectiveEnd,
                                                              p.width, p.height, p.wNear, p.wMid, p.wFar);

        cached.width = p.width;
        cached.height = p.height;
        cached.isDrums = p.isDrums;
        cached.wNear = p.wNear;
        cached.wMid = p.wMid;
        cached.wFar = p.wFar;
        cached.posEnd = p.highwayPosEnd;
        cached.fadeEnd = p.farFadeEnd;
    }

    // Render strips into offscreen image
    constexpr int HS = HIGHWAY_RENDER_SCALE;
    int offW = p.width * HS;
    int offH = p.height * HS;
    if (offscreen.getWidth() != offW || offscreen.getHeight() != offH)
        offscreen = juce::Image(juce::Image::ARGB, offW, offH, true);
    else
        offscreen.clear({0, 0, offW, offH});

    {
        juce::Graphics og(offscreen);
        og.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        for (int i = 0; i < cached.stripCount; i++)
        {
            auto& [bot, botPos] = cached.edges[i];
            auto& [top, topPos] = cached.edges[i + 1];

            float destTop = top.centerY * HS;
            float destBottom = bot.centerY * HS;
            if (destBottom <= destTop) continue;

            float destLeft = (bot.leftX + top.leftX) * 0.5f * HS;
            float destRight = (bot.rightX + top.rightX) * 0.5f * HS;
            float destW = destRight - destLeft;
            float destH = destBottom - destTop;
            if (destW <= 0) continue;

            float stripMidPos = (botPos + topPos) * 0.5f;
            float stripAlpha = calculateOpacity(stripMidPos, p.farFadeEnd, p.farFadeLen, p.farFadeCurve);
            if (stripAlpha <= 0.0f) continue;
            og.setOpacity(stripAlpha);

            float vTop = (1.0f - topPos) * TILES_PER_HIGHWAY + (float)scrollOffset;
            float vBot = (1.0f - botPos) * TILES_PER_HIGHWAY + (float)scrollOffset;

            float srcYf = fmod(vTop * texH, (float)texH);
            if (srcYf < 0) srcYf += texH;
            int srcY = (int)srcYf;
            int srcH = std::max(1, (int)((vBot - vTop) * texH));

            if (srcY + srcH > texH)
            {
                int h1 = texH - srcY;
                int h2 = srcH - h1;
                float splitFrac = (float)h1 / (float)srcH;
                float splitY = destTop + destH * splitFrac;

                if (h1 > 0)
                    og.drawImage(texture, (int)destLeft, (int)destTop, (int)std::ceil(destW), (int)std::ceil(splitY - destTop), 0, srcY, texW, h1);
                if (h2 > 0)
                    og.drawImage(texture, (int)destLeft, (int)splitY, (int)std::ceil(destW), (int)std::ceil(destBottom - splitY), 0, 0, texW, h2);
            }
            else
            {
                og.drawImage(texture, (int)destLeft, (int)destTop, (int)std::ceil(destW), (int)std::ceil(destH), 0, srcY, texW, srcH);
            }
        }
    }

    // Composite offscreen with fretboard clip path
    g.saveState();
    g.reduceClipRegion(cached.fretboardPath);
    g.setOpacity(p.highwayTextureOpacity);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(offscreen, juce::Rectangle<float>(0, 0, (float)p.width, (float)p.height));
    g.restoreState();
}

//==============================================================================
// Draw image with per-strip highway fade

void HighwayTextureRenderer::drawImageWithFade(juce::Graphics& g, const juce::Image& img,
                                               juce::Rectangle<float> destRect, const Params& p)
{
    if (!img.isValid()) return;

    float effectiveEnd = std::max(p.highwayPosEnd, p.farFadeEnd);
    float posRange = effectiveEnd - HIGHWAY_POS_START;

    // Build position->Y edge table (same approach as highway texture strips)
    constexpr int stripCount = 40;
    std::pair<float, float> strips[stripCount + 1]; // {Y, position}
    for (int i = 0; i <= stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)stripCount;
        auto edge = PositionMath::getFretboardEdge(p.isDrums, pos, p.width, p.height, p.wNear, p.wMid, p.wFar, HIGHWAY_POS_START, effectiveEnd);
        strips[i] = {edge.centerY, pos};
    }

    int imgW = img.getWidth(), imgH = img.getHeight();
    float dstTop = destRect.getY();
    float dstBot = destRect.getBottom();
    float dstH = destRect.getHeight();

    for (int i = 0; i < stripCount; i++)
    {
        float yBot = strips[i].first;    // bottom (near strikeline, large Y)
        float yTop = strips[i + 1].first; // top (far end, small Y)
        float posBot = strips[i].second;
        float posTop = strips[i + 1].second;

        // Skip strips entirely outside the destination rect
        if (yBot <= dstTop || yTop >= dstBot) continue;

        float midPos = (posBot + posTop) * 0.5f;
        float opacity = calculateOpacity(midPos, p.farFadeEnd, p.farFadeLen, p.farFadeCurve);
        if (opacity <= 0.0f) continue;

        // Clamp strip bounds to destination rect
        float clampedTop = std::max(yTop, dstTop);
        float clampedBot = std::min(yBot, dstBot);

        // Map screen Y to source image coordinates
        float srcFracTop = (clampedTop - dstTop) / dstH;
        float srcFracBot = (clampedBot - dstTop) / dstH;
        int srcY = (int)(srcFracTop * imgH);
        int srcH = std::max(1, (int)(srcFracBot * imgH) - srcY);

        g.setOpacity(opacity);
        g.drawImage(img,
                    (int)destRect.getX(), (int)clampedTop,
                    (int)std::ceil(destRect.getWidth()), (int)std::ceil(clampedBot - clampedTop),
                    0, srcY, imgW, srcH);
    }
    g.setOpacity(1.0f);
}
