/*
    ==============================================================================

        NoteRenderer.cpp
        Author:  Noah Baxter

        Note/gem rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "NoteRenderer.h"

using namespace PositionConstants;

NoteRenderer::NoteRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void NoteRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                            double windowStartTime, double windowEndTime,
                            uint width, uint height,
                            float wNear, float wMid, float wFar, float posEnd,
                            float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    currentDrawCallMap = &drawCallMap;
    this->width = width;
    this->height = height;
    this->wNear = wNear;
    this->wMid = wMid;
    this->wFar = wFar;
    this->posEnd = posEnd;
    this->farFadeEnd = farFadeEnd;
    this->farFadeLen = farFadeLen;
    this->farFadeCurve = farFadeCurve;

    double windowTimeSpan = windowEndTime - windowStartTime;
    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // When hit animations are on, notes clip at the strike position.
    // strikePosGem/strikePosBar shift the clip point (negative = past strikeline = lower on screen).
    // When off, notes flow past the strikeline to the bottom of the highway.
    double noteClipTime = hitAnimationsOn ? (strikePosGem * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    double barClipTime = hitAnimationsOn ? (strikePosBar * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    // Use the more permissive clip for frame-level skip; per-gem clip happens in drawGem
    double frameClipTime = std::min(noteClipTime, barClipTime);
    cachedNoteClipTime = noteClipTime;
    cachedBarClipTime = barClipTime;

    for (const auto& frameItem : trackWindow)
    {
        double frameTime = frameItem.first;

        if (frameTime < frameClipTime) continue;

        float normalizedPosition = (float)((frameTime - windowStartTime) / windowTimeSpan);

        drawFrame(frameItem.second, normalizedPosition, frameTime);
    }
}

void NoteRenderer::drawFrame(const TimeBasedTrackFrame& gems, float position, double frameTime)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, frameTime);
        }
    }
}

void NoteRenderer::drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime)
{
    juce::Rectangle<float> glyphRect;
    juce::Image* glyphImage;
    bool barNote;

    bool starPowerActive = state.getProperty("starPower");

    if (isPart(state, Part::GUITAR))
    {
        barNote = isBarNote(gemColumn, Part::GUITAR);
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }
    else
    {
        barNote = isBarNote(gemColumn, Part::DRUMS);
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }

    // Per-gem clip: bars and notes can have different strike positions
    double clipTime = barNote ? cachedBarClipTime : cachedNoteClipTime;
    if (frameTime < clipTime) return;

    if (barNote && !showBars) return;
    if (!barNote && !showGems) return;

    if (glyphImage == nullptr)
        return;

    float imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();

    float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
    float adjustedPosition = barNote ? position + PositionConstants::BAR_NOTE_POS_OFFSET : position;

    if (isPart(state, Part::GUITAR))
    {
        const auto& colCoords = PositionConstants::guitarGlyphCoords[
            (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1];
        auto edge = getColumnEdge(adjustedPosition, colCoords, sizeScale, PositionConstants::FRETBOARD_SCALE);
        float colWidth = edge.rightX - edge.leftX;
        float colHeight = colWidth / imageAspect;
        glyphRect = juce::Rectangle<float>(edge.leftX, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }
    else
    {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& colCoords = PositionConstants::drumGlyphCoords[drumIdx];
        auto edge = getColumnEdge(adjustedPosition, colCoords, sizeScale, PositionConstants::FRETBOARD_SCALE);
        float colWidth = edge.rightX - edge.leftX;
        float colHeight = colWidth / imageAspect;
        glyphRect = juce::Rectangle<float>(edge.leftX, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

    // Apply user gem scale (from Display popup, uniform)
    float gemScale = state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f;
    if (std::abs(gemScale - 1.0f) > 0.001f)
    {
        float cx = glyphRect.getCentreX();
        float cy = glyphRect.getCentreY();
        float newW = glyphRect.getWidth() * gemScale;
        float newH = glyphRect.getHeight() * gemScale;
        glyphRect = juce::Rectangle<float>(cx - newW / 2.0f, cy - newH / 2.0f, newW, newH);
    }

    float opacity = calculateOpacity(position);
    bool isDrums = isPart(state, Part::DRUMS);
    float noteCurv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float curvature = barNote ? PositionConstants::BAR_CURVATURE : noteCurv;

    // Debug scale factors (separate note vs bar)
    float baseW = barNote ? barWidthScale : gemWidthScale;
    float baseH = barNote ? barHeightScale : gemHeightScale;

    // Per-gem-type dynamic scales (base image vs overlay, independently)
    float baseScale = 1.0f;
    float overlayDynScale = 1.0f;
    bool hasOverlay = false;

    if (!isDrums)
    {
        // Guitar
        switch (gemWrapper.gem) {
        case Gem::NOTE:        baseScale = gemNoteScale; break;
        case Gem::HOPO_GHOST:  baseScale = gemHopoScale; break;
        case Gem::TAP_ACCENT:  baseScale = gemHopoBaseScale; overlayDynScale = gemTapOverlayScale; hasOverlay = true; break;
        default: break;
        }
    }
    else
    {
        // Drums
        switch (gemWrapper.gem) {
        case Gem::NOTE:        baseScale = gemNoteScale; break;
        case Gem::HOPO_GHOST:  baseScale = gemNoteBaseScale; overlayDynScale = gemGhostOverlayScale; hasOverlay = true; break;
        case Gem::TAP_ACCENT:  baseScale = gemNoteBaseScale; overlayDynScale = gemAccentOverlayScale; hasOverlay = true; break;
        case Gem::CYM:         baseScale = gemCymScale; break;
        case Gem::CYM_GHOST:   baseScale = gemCymBaseScale; overlayDynScale = gemGhostOverlayScale; hasOverlay = true; break;
        case Gem::CYM_ACCENT:  baseScale = gemCymBaseScale; overlayDynScale = gemAccentOverlayScale; hasOverlay = true; break;
        default: break;
        }
    }

    float spMul = (gemWrapper.starPower && std::abs(gemSpScale - 1.0f) > 0.001f) ? gemSpScale : 1.0f;
    float wScale = baseW * baseScale * spMul;
    float hScale = baseH * baseScale * spMul;
    float oWScale = baseW * overlayDynScale * spMul;
    float oHScale = baseH * overlayDynScale * spMul;

    // Per-column Z offset (screen pixels at strikeline, scaled by perspective)
    float zOff = barNote ? barZOffset : gemZOffset;
    if (!barNote && isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        zOff += drumColZOffsets[drumIdx];
    }

    // Per-column X offset: interpolate between near (X1) and far (X2) based on position
    float xOff1 = 0.0f, xOff2 = 0.0f;
    if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
        xOff1 = guitarColXOffsets[gemColumn];
        xOff2 = guitarColXOffsets2[gemColumn];
    } else if (isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        xOff1 = drumColXOffsets[drumIdx];
        xOff2 = drumColXOffsets2[drumIdx];
    }

    // Scale Z offset by perspective
    {
        float sizeScaleRef = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        const auto& colCoordsRef = isPart(state, Part::GUITAR)
            ? PositionConstants::guitarGlyphCoords[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
            : PositionConstants::drumGlyphCoords[drumColumnIndex(gemColumn)];
        auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, sizeScaleRef, PositionConstants::FRETBOARD_SCALE);
        float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
        float curWidth = glyphRect.getWidth();
        if (strikeWidth > 0.0f)
        {
            float perspScale = curWidth / strikeWidth;
            zOff *= perspScale;
        }
    }

    // Interpolate X offset: position 0 = strikeline (X1), position ~1 = far end (X2)
    float t = juce::jlimit(0.0f, 1.0f, position);
    float xOff = xOff1 + (xOff2 - xOff1) * t;

    // Apply X offset to glyphRect
    if (std::abs(xOff) > 0.01f)
        glyphRect.translate(xOff, 0.0f);

    // Compute global arc Y offset so adjacent notes form a continuous parabola.
    float arcOffset = 0.0f;
    if (curvature != 0.0f && !barNote)
    {
        float dist = getColumnDistFromCenter(gemColumn, isDrums);
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        const auto& colC = isDrums
            ? drumGlyphCoords[drumColumnIndex(gemColumn)]
            : guitarGlyphCoords[(gemColumn < (int)GUITAR_LANE_COUNT) ? gemColumn : 1];
        float fbWidthPx = glyphRect.getWidth() * (fbCoords.normWidth1 / colC.normWidth1);
        arcOffset = fbWidthPx * curvature * (1.0f - dist * dist);
    }

    DrawOrder layer = barNote ? DrawOrder::BAR : DrawOrder::NOTE;

    // Helper: scale a rect around its center with w/h factors and Y offset
    auto scaleRect = [](juce::Rectangle<float> r, float ws, float hs, float yOff) {
        float cx = r.getCentreX();
        float cy = r.getCentreY() + yOff;
        float fw = r.getWidth() * ws;
        float fh = r.getHeight() * hs;
        return juce::Rectangle<float>(cx - fw * 0.5f, cy - fh * 0.5f, fw, fh);
    };

    if (curvature != 0.0f)
    {
        const auto& entry = getCurvedImage(glyphImage, gemColumn, isDrums);
        const juce::Image* curvedImgPtr = &entry.image;
        float contentYOff = entry.yOffsetFraction;

        float cachedAspect = (float)curvedImgPtr->getWidth() / (float)curvedImgPtr->getHeight();
        float curvedH = glyphRect.getWidth() / cachedAspect;
        float curvedY = glyphRect.getCentreY() - curvedH * 0.5f
                        + contentYOff * glyphRect.getHeight()
                        + arcOffset;
        auto curvedRect = juce::Rectangle<float>(glyphRect.getX(), curvedY,
                                                  glyphRect.getWidth(), curvedH);
        curvedRect = scaleRect(curvedRect, wScale, hScale, zOff);

        (*currentDrawCallMap)[static_cast<int>(layer)][gemColumn].push_back([curvedImgPtr, opacity, curvedRect](juce::Graphics& g) {
            g.setOpacity(opacity);
            g.drawImage(*curvedImgPtr, curvedRect);
        });
    }
    else
    {
        auto drawRect = scaleRect(glyphRect, wScale, hScale, zOff);

        (*currentDrawCallMap)[static_cast<int>(layer)][gemColumn].push_back([=](juce::Graphics& g) {
            g.setOpacity(opacity);
            g.drawImage(*glyphImage, drawRect);
        });
    }

    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        bool isDrumAccent = isDrums && (gemWrapper.gem == Gem::TAP_ACCENT || gemWrapper.gem == Gem::CYM_ACCENT);
        juce::Rectangle<float> overlayRect = getOverlayGlyphRect(glyphRect, isDrumAccent);

        if (curvature != 0.0f)
        {
            const auto& entry = getCurvedImage(overlayImage, gemColumn, isDrums);
            const juce::Image* curvedOverlayPtr = &entry.image;
            float contentYOff = entry.yOffsetFraction;

            float cachedAspect = (float)curvedOverlayPtr->getWidth() / (float)curvedOverlayPtr->getHeight();
            float curvedH = overlayRect.getWidth() / cachedAspect;
            float curvedY = overlayRect.getCentreY() - curvedH * 0.5f
                            + contentYOff * overlayRect.getHeight()
                            + arcOffset;
            auto curvedOverlayRect = juce::Rectangle<float>(overlayRect.getX(), curvedY,
                                                             overlayRect.getWidth(), curvedH);
            curvedOverlayRect = scaleRect(curvedOverlayRect, oWScale, oHScale, zOff);

            (*currentDrawCallMap)[static_cast<int>(DrawOrder::OVERLAY)][gemColumn].push_back([curvedOverlayPtr, opacity, curvedOverlayRect](juce::Graphics& g) {
                g.setOpacity(opacity);
                g.drawImage(*curvedOverlayPtr, curvedOverlayRect);
            });
        }
        else
        {
            auto drawRect = scaleRect(overlayRect, oWScale, oHScale, zOff);

            (*currentDrawCallMap)[static_cast<int>(DrawOrder::OVERLAY)][gemColumn].push_back([=](juce::Graphics& g) {
                g.setOpacity(opacity);
                g.drawImage(*overlayImage, drawRect);
            });
        }
    }
}

float NoteRenderer::getColumnDistFromCenter(int column, bool isDrums)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfW = fbCoords.normWidth1 * 0.5f;

    const auto& colCoords = isDrums
        ? drumGlyphCoords[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : guitarGlyphCoords[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

    float colCenter = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
    return (colCenter - fbCenter) / fbHalfW;
}

const NoteRenderer::CurvedImageEntry& NoteRenderer::getCurvedImage(
    juce::Image* src, int column, bool isDrums)
{
    if (noteCurvatureGuitar != lastCachedCurvatureGuitar ||
        noteCurvatureDrums != lastCachedCurvatureDrums)
    {
        curvedCache.clear();
        lastCachedCurvatureGuitar = noteCurvatureGuitar;
        lastCachedCurvatureDrums = noteCurvatureDrums;
    }

    CurveKey key{src, column, isDrums};
    auto it = curvedCache.find(key);
    if (it != curvedCache.end())
        return it->second;

    int srcW = src->getWidth() / NOTE_CACHE_DOWNSAMPLE;
    int srcH = src->getHeight() / NOTE_CACHE_DOWNSAMPLE;
    if (srcW < 1) srcW = 1;
    if (srcH < 1) srcH = 1;

    // Downsampled source
    juce::Image downSrc(juce::Image::ARGB, srcW, srcH, true);
    {
        juce::Graphics gDown(downSrc);
        gDown.drawImage(*src, juce::Rectangle<float>(0.0f, 0.0f, (float)srcW, (float)srcH));
    }

    // Compute per-column Y offsets: arcHeight * (1 - dist²) matching snapshot math
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

    const auto& colCoords = isDrums
        ? drumGlyphCoords[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : guitarGlyphCoords[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

    float fbWidthInCache = (float)srcW * (fbCoords.normWidth1 / colCoords.normWidth1);
    float curv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float arcHeight = fbWidthInCache * curv;

    float noteLeftNorm = colCoords.normX1;
    float noteRightNorm = colCoords.normX1 + colCoords.normWidth1;

    // Compute Y offset for every pixel column
    std::vector<float> colOffsets(srcW);

    for (int x = 0; x < srcW; x++)
    {
        float t = ((float)x + 0.5f) / (float)srcW;
        float xNorm = noteLeftNorm + t * (noteRightNorm - noteLeftNorm);
        float dist = (xNorm - fbCenterNorm) / fbHalfWNorm;
        colOffsets[x] = arcHeight * (1.0f - dist * dist);
    }

    // Global reference: the arc value at fretboard edge (dist=1), where yOff=0.
    // colOffsets[x] - globalRef gives the displacement from the edge baseline.
    // Positive curvature: center has most displacement (pushed down on screen = convex).
    // Negative curvature: center has most negative offset, edges at 0.
    float globalRef = std::min(0.0f, arcHeight);
    float maxShift = 0.0f;
    for (int x = 0; x < srcW; x++)
    {
        float shift = colOffsets[x] - globalRef;
        if (shift > maxShift) maxShift = shift;
    }

    int extraPx = (int)std::ceil(maxShift) + 2;
    int destH = srcH + extraPx;

    juce::Image dest(juce::Image::ARGB, srcW, destH, true);

    // Per-pixel warp via BitmapData — inverse mapping: for each dest pixel, sample source
    {
        juce::Image::BitmapData srcData(downSrc, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dest, juce::Image::BitmapData::writeOnly);

        for (int x = 0; x < srcW; x++)
        {
            // Absolute Y shift: all columns share the same global reference
            float yShift = colOffsets[x] - globalRef;

            for (int dy = 0; dy < destH; dy++)
            {
                float sy = (float)dy - yShift;

                int sy0 = (int)std::floor(sy);
                int sy1 = sy0 + 1;
                float frac = sy - (float)sy0;

                if (sy0 < 0 || sy1 >= srcH) {
                    if (sy0 >= 0 && sy0 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy0));
                    } else if (sy1 >= 0 && sy1 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy1));
                    }
                    continue;
                }

                auto c0 = srcData.getPixelColour(x, sy0);
                auto c1 = srcData.getPixelColour(x, sy1);
                dstData.setPixelColour(x, dy, c0.interpolatedWith(c1, frac));
            }
        }
    }

    // yOffsetFraction: where this column's content center sits relative to dest image center
    float centerColShift = colOffsets[srcW / 2] - globalRef;
    float srcCenterInDest = centerColShift + (float)srcH * 0.5f;
    float destCenter = (float)destH * 0.5f;
    float yOffsetFraction = (srcCenterInDest - destCenter) / (float)srcH;

    auto [insertIt, _] = curvedCache.emplace(key, CurvedImageEntry{std::move(dest), yOffsetFraction});
    return insertIt->second;
}

juce::Rectangle<float> NoteRenderer::getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent)
{
    if (isDrumAccent)
    {
        float scaleFactor = DRUM_ACCENT_OVERLAY_SCALE * GEM_SIZE;
        float newWidth = glyphRect.getWidth() * scaleFactor;
        float newHeight = glyphRect.getHeight() * scaleFactor;
        float widthIncrease = newWidth - glyphRect.getWidth();
        float heightIncrease = newHeight - glyphRect.getHeight();

        float xPos = glyphRect.getX() - widthIncrease / 2;
        float yPos = glyphRect.getY() - heightIncrease / 2;

        return juce::Rectangle<float>(xPos, yPos, newWidth, newHeight);
    }

    return glyphRect;
}
