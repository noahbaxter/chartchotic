/*
    ==============================================================================

        NoteRenderer.cpp
        Author:  Noah Baxter

        Note/gem rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "NoteRenderer.h"
#include "../Utils/RenderTypeConfig.h"

using namespace PositionConstants;

NoteRenderer::NoteRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
    std::copy_n(OVERLAY_DEFAULTS, NUM_OVERLAY_TYPES, overlayAdjusts);
}

void NoteRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                            double windowStartTime, double windowEndTime,
                            uint width, uint height,
                            float posEnd,
                            float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    currentDrawCallMap = &drawCallMap;
    this->width = width;
    this->height = height;
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
    bool isDrums = isDrumLike(activePart);

    // Shared anchor + scale for the whole row: one projection of the lane plane
    // at the musical position. Every sprite (bar + gems + overlays) is laid out
    // in strike-reference pixels relative to this anchor and scaled by the same
    // frameScale, so the row renders as a single composite — no per-sprite
    // depth offset, no drift between stacked elements.
    auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f, width, height,
                                                    PositionConstants::HIGHWAY_POS_START, posEnd);
    auto fbCur = PositionMath::getFretboardEdge(isDrums, position, width, height,
                                                  PositionConstants::HIGHWAY_POS_START, posEnd);
    float fbStrikeWidth = fbStrike.rightX - fbStrike.leftX;
    float fbStrikeCenterX = (fbStrike.leftX + fbStrike.rightX) * 0.5f;
    float fbCurWidth = fbCur.rightX - fbCur.leftX;
    float fbCurCenterX = (fbCur.leftX + fbCur.rightX) * 0.5f;
    float widthRatio = (fbStrikeWidth > 0.0f) ? (fbCurWidth / fbStrikeWidth) : 1.0f;

    SharedFrameContext ctx;
    ctx.anchor = juce::Point<float>(fbCurCenterX, fbCur.centerY);
    ctx.frameScale = juce::Point<float>(widthRatio, widthRatio);
    ctx.fbStrikeWidth = fbStrikeWidth;
    ctx.fbStrikeCenterX = fbStrikeCenterX;

    PositionConstants::Frame composite;
    composite.position = position;
    composite.column = -1;

    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            appendGemSprites(gemColumn, gems[gemColumn], position, frameTime, ctx, composite);
        }
    }

    if (!composite.sprites.empty())
        PositionConstants::drawFrame(composite, ctx.anchor, ctx.frameScale, *currentDrawCallMap);
}

void NoteRenderer::appendGemSprites(uint gemColumn, const GemWrapper& gemWrapper, float position,
                                     double frameTime, const SharedFrameContext& ctx,
                                     PositionConstants::Frame& outFrame)
{
    juce::Image* glyphImage;
    bool barNote;

    bool starPowerActive = state.getProperty("starPower");
    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    bool isDrums = isDrumLike(activePart);

    if (isGuitarLike(activePart))
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
    float opacity = calculateOpacity(position);

    // ============================================================================
    // BEMANI PATH: flat / no perspective — keep the legacy per-gem direct-draw
    // path. Bemani mode doesn't have the chord-drift problem and uses its own
    // bar-rect math + nudge constants that don't fit the composite model.
    // ============================================================================
    if (PositionMath::bemaniMode)
    {
        float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        juce::Rectangle<float> glyphRect;

        if (barNote)
        {
            glyphRect = PositionMath::computeBemaniBarRect(
                isDrums, position, width, height, posEnd,
                sizeScale, imageAspect);
        }
        else if (isGuitarLike(activePart))
        {
            int idx = (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1;
            const auto& colCoords = laneCoordsGuitar[idx];
            int bemaniIdx = idx - 1;
            auto edge = getColumnEdge(position, colCoords, 1.0f,
                                      PositionConstants::FRETBOARD_SCALE, bemaniIdx);
            float laneWidth = edge.rightX - edge.leftX;
            float colWidth = laneWidth * sizeScale;
            float colHeight = colWidth / imageAspect;
            float cx = (edge.leftX + edge.rightX) * 0.5f;
            glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
        }
        else
        {
            uint drumIdx = drumColumnIndex(gemColumn);
            const auto& colCoords = laneCoordsDrums[drumIdx];
            int bemaniIdx = (int)drumIdx - 1;
            auto edge = getColumnEdge(position, colCoords, 1.0f,
                                      PositionConstants::FRETBOARD_SCALE, bemaniIdx);
            float laneWidth = edge.rightX - edge.leftX;
            float colWidth = laneWidth * sizeScale;
            float colHeight = colWidth / imageAspect;
            float cx = (edge.leftX + edge.rightX) * 0.5f;
            glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
        }

        float userScale = barNote
            ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
            : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);
        if (std::abs(userScale - 1.0f) > 0.001f)
        {
            float cx = glyphRect.getCentreX();
            float cy = glyphRect.getCentreY();
            float newW = glyphRect.getWidth() * userScale;
            float newH = glyphRect.getHeight() * userScale;
            glyphRect = juce::Rectangle<float>(cx - newW / 2.0f, cy - newH / 2.0f, newW, newH);
        }

        float nudge = barNote ? bemaniConfig.barNudge : config->bemaniGemNudge();
        float pixelsPerUnit = PositionConstants::REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                            / std::max(0.1f, PositionMath::bemaniHwyScale);
        glyphRect.translate(0.0f, pixelsPerUnit * nudge);

        float baseW = barNote ? bemaniConfig.barW : bemaniConfig.gemW;
        float baseH = barNote ? bemaniConfig.barH : bemaniConfig.gemH;

        float typeScale = 1.0f;
        if (!barNote)
        {
            if (!isDrums)
            {
                switch (gemWrapper.gem) {
                case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
                case Gem::HOPO_GHOST:  typeScale = gemTypeScales.hopo; break;
                case Gem::TAP_ACCENT:  typeScale = gemTypeScales.gTap; break;
                default: break;
                }
            }
            else
            {
                switch (gemWrapper.gem) {
                case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
                case Gem::HOPO_GHOST:  typeScale = gemTypeScales.dGhost; break;
                case Gem::TAP_ACCENT:  typeScale = gemTypeScales.dAccent; break;
                case Gem::CYM:         typeScale = gemTypeScales.cymbal; break;
                case Gem::CYM_GHOST:   typeScale = gemTypeScales.cGhost; break;
                case Gem::CYM_ACCENT:  typeScale = gemTypeScales.cAccent; break;
                default: break;
                }
            }
        }

        float spMul = 1.0f;
        if (!barNote && gemWrapper.starPower)
        {
            float spScale = gemTypeScales.spGem;
            if (std::abs(spScale - 1.0f) > 0.001f) spMul = spScale;
        }

        float wScale = baseW * typeScale * spMul;
        float hScale = baseH * typeScale * spMul;

        float baseCurv = barNote ? PositionConstants::BAR_CURVATURE
                                  : (isDrums ? noteCurvatureDrums : noteCurvatureGuitar);
        float curvature = baseCurv * bemaniConfig.curvature;

        // Single-sprite frame, scale = 1 (flat). zOff packed as a sprite-level offsetY.
        PositionConstants::Frame frame;
        frame.position = position;
        frame.column = (int)gemColumn;
        frame.isBar = barNote;

        PositionConstants::FrameSprite s;
        s.image = glyphImage;
        s.offsetX = 0.0f;
        s.offsetY = 0.0f;
        s.width = glyphRect.getWidth() * wScale;
        s.height = glyphRect.getHeight() * hScale;
        s.drawOrder = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
        s.drawColumn = (int)gemColumn;
        s.opacity = opacity;
        frame.sprites.push_back(s);

        juce::Image* overlayImage = assetManager.getOverlayImage(
            gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
        const OverlayAdjust* overlayAdjPtr = nullptr;
        if (overlayImage != nullptr)
        {
            const auto& overlayAdj = [&]() -> const OverlayAdjust& {
                if (!isDrums) return overlayAdjusts[OVERLAY_GUITAR_TAP];
                switch (gemWrapper.gem) {
                case Gem::HOPO_GHOST: return overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST];
                case Gem::TAP_ACCENT: return overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT];
                case Gem::CYM_GHOST:  return overlayAdjusts[OVERLAY_DRUM_CYM_GHOST];
                case Gem::CYM_ACCENT: return overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT];
                default: { static const OverlayAdjust none; return none; }
                }
            }();
            overlayAdjPtr = &overlayAdj;

            float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
            float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;

            PositionConstants::FrameSprite ov;
            ov.image = overlayImage;
            ov.width = glyphRect.getWidth() * wScale * ovlRectSX;
            ov.height = glyphRect.getHeight() * hScale * ovlRectSY;
            ov.offsetX = overlayAdj.offsetX * ov.width;
            ov.offsetY = overlayAdj.offsetY * ov.height;
            ov.drawOrder = (int)DrawOrder::OVERLAY;
            ov.drawColumn = (int)gemColumn;
            ov.opacity = opacity;
            frame.sprites.push_back(ov);
        }

        if (curvature != 0.0f)
        {
            const auto& gemEntry = getCurvedImage(glyphImage, gemColumn, isDrums);
            float gemCurvedAspect = (float)gemEntry.image.getWidth()
                                  / (float)gemEntry.image.getHeight();
            float gemCurvedH = (glyphRect.getWidth() / gemCurvedAspect) * hScale;
            frame.sprites[0].image = const_cast<juce::Image*>(&gemEntry.image);
            frame.sprites[0].height = gemCurvedH;
            frame.sprites[0].offsetY += gemEntry.yOffsetFraction * glyphRect.getHeight();

            if (overlayImage != nullptr && overlayAdjPtr != nullptr)
            {
                const auto& ovlEntry = getCurvedImage(overlayImage, gemColumn, isDrums);
                float ovlCurvedAspect = (float)ovlEntry.image.getWidth()
                                      / (float)ovlEntry.image.getHeight();
                auto& ovlSprite = frame.sprites[1];
                float ovlRectSX = overlayAdjPtr->scaleX * overlayAdjPtr->scale;
                float ovlRectSY = overlayAdjPtr->scaleY * overlayAdjPtr->scale;
                float ovlCurvedH = (glyphRect.getWidth() * ovlRectSX) / ovlCurvedAspect * hScale;
                float ovlContentYBase = glyphRect.getHeight() * ovlRectSY;

                ovlSprite.image = const_cast<juce::Image*>(&ovlEntry.image);
                ovlSprite.height = ovlCurvedH;
                ovlSprite.offsetX = overlayAdjPtr->offsetX * ovlSprite.width;
                ovlSprite.offsetY = ovlEntry.yOffsetFraction * ovlContentYBase
                                  + overlayAdjPtr->offsetY * ovlSprite.height;
            }
        }

        juce::Point<float> bemaniAnchor(glyphRect.getCentreX(), glyphRect.getCentreY());
        juce::Point<float> bemaniScale(1.0f, 1.0f);
        PositionConstants::drawFrame(frame, bemaniAnchor, bemaniScale, *currentDrawCallMap);
        return;
    }

    // ============================================================================
    // PERSPECTIVE PATH: append sprites to the shared composite frame.
    // The shared anchor + frameScale come from ctx (one projection of the lane
    // plane at this musical position). All sprite offsets and sizes below are
    // expressed in strike-reference pixels relative to ctx.anchor; drawFrame
    // applies ctx.frameScale uniformly so the bar and its stacked gems can't
    // drift apart.
    // ============================================================================

    // Strike-reference width + horizontal offset from shared anchor
    float strikeColWidth, strikeOffsetX;
    if (barNote)
    {
        strikeColWidth = ctx.fbStrikeWidth
                       * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
        strikeOffsetX = 0.0f;  // bar is centered on fretboard
    }
    else
    {
        const auto& colCoordsRef = isGuitarLike(activePart)
            ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
            : laneCoordsDrums[drumColumnIndex(gemColumn)];
        auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, PositionConstants::GEM_SIZE,
                                         PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = strikeEdge.rightX - strikeEdge.leftX;
        strikeOffsetX = (strikeEdge.leftX + strikeEdge.rightX) * 0.5f - ctx.fbStrikeCenterX;
    }
    float strikeColHeight = strikeColWidth / imageAspect;

    // userScale (settings popup) — sprite-size multiplier; center stays at lane
    float userScale = barNote
        ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
        : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);

    // Per-gem-type scale + star-power multiplier
    float typeScale = 1.0f;
    if (!barNote)
    {
        if (!isDrums)
        {
            switch (gemWrapper.gem) {
            case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
            case Gem::HOPO_GHOST:  typeScale = gemTypeScales.hopo; break;
            case Gem::TAP_ACCENT:  typeScale = gemTypeScales.gTap; break;
            default: break;
            }
        }
        else
        {
            switch (gemWrapper.gem) {
            case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
            case Gem::HOPO_GHOST:  typeScale = gemTypeScales.dGhost; break;
            case Gem::TAP_ACCENT:  typeScale = gemTypeScales.dAccent; break;
            case Gem::CYM:         typeScale = gemTypeScales.cymbal; break;
            case Gem::CYM_GHOST:   typeScale = gemTypeScales.cymbal * gemTypeScales.cGhost; break;
            case Gem::CYM_ACCENT:  typeScale = gemTypeScales.cymbal * gemTypeScales.cAccent; break;
            default: break;
            }
        }
    }
    float spMul = 1.0f;
    if (!barNote && gemWrapper.starPower)
    {
        float spScale = gemTypeScales.spGem;
        if (std::abs(spScale - 1.0f) > 0.001f) spMul = spScale;
    }

    const auto& baseScale = barNote ? barScale : gemScale;
    float wScale = baseScale.width  * typeScale * spMul * userScale;
    float hScale = baseScale.height * typeScale * spMul * userScale;
    float oWScale = wScale;
    float oHScale = hScale;

    // zOff (Z lift in strike-reference pixels) + per-column adjustment
    bool isCymbalGem = !barNote && isDrums
        && (gemWrapper.gem == Gem::CYM
            || gemWrapper.gem == Gem::CYM_GHOST
            || gemWrapper.gem == Gem::CYM_ACCENT);
    float zOff = barNote ? barZOffset : (isCymbalGem ? cymZOffset : gemZOffset);

    float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
    if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
        const auto& ca = guitarColAdjust[gemColumn];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
    } else if (isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& ca = drumColAdjust[drumIdx];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
        if (!barNote) zOff += ca.z;
    }

    float vpDepth = config->getPerspectiveParams().vanishingPointDepth;
    float t = juce::jlimit(0.0f, 1.0f, position / vpDepth);
    float colScale = colSNear + (colSFar - colSNear) * t;
    wScale *= colScale * colW;
    hScale *= colScale * colH;
    oWScale *= colScale * colW;
    oHScale *= colScale * colH;

    // Curvature arc, in strike-reference pixels (ctx.frameScale carries it to
    // current pixels at draw time, matching legacy current-pixel-space arc).
    float noteCurv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float curvature = barNote ? PositionConstants::BAR_CURVATURE : noteCurv;
    float arcOffsetStrike = 0.0f;
    if (curvature != 0.0f && !barNote)
    {
        float dist = getColumnDistFromCenter(gemColumn, isDrums);
        arcOffsetStrike = ctx.fbStrikeWidth * PositionConstants::FRETBOARD_SCALE
                        * curvature * (1.0f - dist * dist);
    }

    // --- Append gem sprite ---
    int gemIdx = (int)outFrame.sprites.size();
    {
        PositionConstants::FrameSprite s;
        s.image     = glyphImage;
        s.offsetX   = strikeOffsetX;
        s.offsetY   = zOff + arcOffsetStrike;
        s.width     = strikeColWidth  * wScale;
        s.height    = strikeColHeight * hScale;
        s.drawOrder = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
        s.drawColumn = (int)gemColumn;
        s.opacity   = opacity;
        outFrame.sprites.push_back(s);
    }

    // --- Append overlay sprite if present ---
    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    const OverlayAdjust* overlayAdjPtr = nullptr;
    int ovlIdx = -1;
    if (overlayImage != nullptr)
    {
        const auto& overlayAdj = [&]() -> const OverlayAdjust& {
            if (!isDrums) return overlayAdjusts[OVERLAY_GUITAR_TAP];
            switch (gemWrapper.gem) {
            case Gem::HOPO_GHOST: return overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST];
            case Gem::TAP_ACCENT: return overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT];
            case Gem::CYM_GHOST:  return overlayAdjusts[OVERLAY_DRUM_CYM_GHOST];
            case Gem::CYM_ACCENT: return overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT];
            default: { static const OverlayAdjust none; return none; }
            }
        }();
        overlayAdjPtr = &overlayAdj;

        float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
        float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;

        PositionConstants::FrameSprite s;
        s.image     = overlayImage;
        s.width     = strikeColWidth  * oWScale * ovlRectSX;
        s.height    = strikeColHeight * oHScale * ovlRectSY;
        s.offsetX   = strikeOffsetX + overlayAdj.offsetX * s.width;
        s.offsetY   = zOff + arcOffsetStrike + overlayAdj.offsetY * s.height;
        s.drawOrder = (int)DrawOrder::OVERLAY;
        s.drawColumn = (int)gemColumn;
        s.opacity   = opacity;
        ovlIdx = (int)outFrame.sprites.size();
        outFrame.sprites.push_back(s);
    }

    // --- Curvature swap: replace each sprite's image with the cached curved
    // variant; adjust height to the curved aspect and re-add the content Y
    // offset so the opaque cone sits where the straight asset's cone would. ---
    if (curvature != 0.0f)
    {
        const auto& gemEntry = getCurvedImage(glyphImage, gemColumn, isDrums);
        float gemCurvedAspect = (float)gemEntry.image.getWidth()
                              / (float)gemEntry.image.getHeight();
        float gemCurvedStrikeHeight = (strikeColWidth / gemCurvedAspect) * hScale;
        outFrame.sprites[gemIdx].image  = const_cast<juce::Image*>(&gemEntry.image);
        outFrame.sprites[gemIdx].height = gemCurvedStrikeHeight;
        outFrame.sprites[gemIdx].offsetY += gemEntry.yOffsetFraction * strikeColHeight;

        if (overlayImage != nullptr && overlayAdjPtr != nullptr && ovlIdx >= 0)
        {
            const auto& ovlEntry = getCurvedImage(overlayImage, gemColumn, isDrums);
            float ovlCurvedAspect = (float)ovlEntry.image.getWidth()
                                  / (float)ovlEntry.image.getHeight();
            auto& ovlSprite = outFrame.sprites[ovlIdx];
            float ovlRectSX = overlayAdjPtr->scaleX * overlayAdjPtr->scale;
            float ovlRectSY = overlayAdjPtr->scaleY * overlayAdjPtr->scale;
            float ovlContentYBaseStrike = strikeColHeight * ovlRectSY;
            float ovlCurvedStrikeHeight = (strikeColWidth * ovlRectSX) / ovlCurvedAspect * oHScale;

            ovlSprite.image  = const_cast<juce::Image*>(&ovlEntry.image);
            ovlSprite.height = ovlCurvedStrikeHeight;
            ovlSprite.offsetX = strikeOffsetX + overlayAdjPtr->offsetX * ovlSprite.width;
            ovlSprite.offsetY = zOff + arcOffsetStrike
                              + ovlEntry.yOffsetFraction * ovlContentYBaseStrike
                              + overlayAdjPtr->offsetY * ovlSprite.height;
        }
    }
}

float NoteRenderer::getColumnDistFromCenter(int column, bool isDrums)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfW = fbCoords.normWidth1 * 0.5f;

    const auto& colCoords = isDrums
        ? laneCoordsDrums[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : laneCoordsGuitar[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

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
        ? laneCoordsDrums[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : laneCoordsGuitar[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

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

juce::Rectangle<float> NoteRenderer::getOverlayGlyphRect(juce::Rectangle<float> glyphRect, const OverlayAdjust& adj)
{
    float sx = adj.scaleX * adj.scale;
    float sy = adj.scaleY * adj.scale;

    if (std::abs(sx - 1.0f) < 0.0001f && std::abs(sy - 1.0f) < 0.0001f)
        return glyphRect;

    // Center-scale only — offsets are applied after curvature/scaleRect
    float cx = glyphRect.getCentreX();
    float cy = glyphRect.getCentreY();
    float newW = glyphRect.getWidth() * sx;
    float newH = glyphRect.getHeight() * sy;
    return juce::Rectangle<float>(cx - newW / 2.0f, cy - newH / 2.0f, newW, newH);
}
