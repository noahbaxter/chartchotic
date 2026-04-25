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

    float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
    float adjustedPosition = barNote ? position + PositionConstants::BAR_NOTE_POS_OFFSET : position;

    // Perspective foreshortening: reduce note height toward the far end.
    // Normalized against vanishingPointDepth for consistent perspective.
    float foreshorten = 1.0f;
    if (depthForeshorten > 0.0f && !PositionMath::bemaniMode)
    {
        auto pp = config->getPerspectiveParams();
        float depth = std::max(0.0f, adjustedPosition) / pp.vanishingPointDepth;
        float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
        float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
        float rawRatio = psCur / scaleNear;
        foreshorten = 1.0f - (1.0f - rawRatio) * depthForeshorten;
    }

    if (barNote)
    {
        // Bar notes span the full fretboard polygon (no FRETBOARD_SCALE)
        if (PositionMath::bemaniMode)
        {
            glyphRect = PositionMath::computeBemaniBarRect(
                isDrums, adjustedPosition, width, height, posEnd,
                sizeScale, imageAspect, foreshorten);
        }
        else
        {
            auto fbEdge = PositionMath::getFretboardEdge(isDrums, adjustedPosition, width, height,
                                                          PositionConstants::HIGHWAY_POS_START, posEnd);
            float fbWidth = fbEdge.rightX - fbEdge.leftX;
            float colWidth = fbWidth * BAR_FRETBOARD_FIT * sizeScale;
            float colHeight = (colWidth / imageAspect) * foreshorten;
            float cx = (fbEdge.leftX + fbEdge.rightX) * 0.5f;
            glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, fbEdge.centerY - colHeight * 0.5f, colWidth, colHeight);
        }
    }
    else if (isGuitarLike(activePart))
    {
        int idx = (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1;
        const auto& colCoords = laneCoordsGuitar[idx];
        int bemaniIdx = idx - 1;  // skip open (0) → green=0, red=1, yel=2, blu=3, org=4
        auto edge = getColumnEdge(adjustedPosition, colCoords, 1.0f,
                                  PositionConstants::FRETBOARD_SCALE, bemaniIdx);
        float laneWidth = edge.rightX - edge.leftX;
        float colWidth = laneWidth * sizeScale;
        float colHeight = (colWidth / imageAspect) * foreshorten;
        float cx = (edge.leftX + edge.rightX) * 0.5f;
        glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }
    else
    {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& colCoords = laneCoordsDrums[drumIdx];
        int bemaniIdx = (int)drumIdx - 1;  // skip kick (0) → red=0, yel=1, blu=2, grn=3
        auto edge = getColumnEdge(adjustedPosition, colCoords, 1.0f,
                                  PositionConstants::FRETBOARD_SCALE, bemaniIdx);
        float laneWidth = edge.rightX - edge.leftX;
        float colWidth = laneWidth * sizeScale;
        float colHeight = (colWidth / imageAspect) * foreshorten;
        float cx = (edge.leftX + edge.rightX) * 0.5f;
        glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

    // Apply user gem/bar scale (from Settings popup)
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

    // Bemani mode: nudge gems/bars using a width-independent reference
    // (pixelsPerUnit is constant regardless of viewport dimensions)
    if (PositionMath::bemaniMode)
    {
        float nudge = barNote ? bemaniConfig.barNudge : config->bemaniGemNudge();
        float pixelsPerUnit = PositionConstants::REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                            / std::max(0.1f, PositionMath::bemaniHwyScale);
        glyphRect.translate(0.0f, pixelsPerUnit * nudge);
    }

    float opacity = calculateOpacity(position);
    float noteCurv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float baseCurv = barNote ? PositionConstants::BAR_CURVATURE : noteCurv;
    float curvature = PositionMath::bemaniMode ? baseCurv * bemaniConfig.curvature : baseCurv;

    // Debug scale factors (separate note vs bar)
    const auto& baseScale = barNote ? barScale : gemScale;
    float baseW, baseH;
    if (PositionMath::bemaniMode)
    {
        baseW = barNote ? bemaniConfig.barW : bemaniConfig.gemW;
        baseH = barNote ? bemaniConfig.barH : bemaniConfig.gemH;
    }
    else
    {
        baseW = baseScale.width;
        baseH = baseScale.height;
    }

    // Per-note-type scale (applied uniformly to base + overlay)
    float typeScale = 1.0f;
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

    // Bar notes maintain constant size regardless of SP or gem-type scaling
    float spMul = 1.0f;
    if (!barNote)
    {
        if (gemWrapper.starPower)
        {
            float spScale = gemTypeScales.spGem;
            if (std::abs(spScale - 1.0f) > 0.001f)
                spMul = spScale;
        }
    }
    else
    {
        typeScale = 1.0f;
    }
    float wScale = baseW * typeScale * spMul;
    float hScale = baseH * typeScale * spMul;
    float oWScale = wScale;
    float oHScale = hScale;

    // --- Compute strike-reference sprite dimensions ---
    // Strike-reference = sprite dimensions at position=0 (strikeline) before
    // perspective foreshorten. frameScale below is the ratio of current → strike.
    float strikeColWidth;
    if (barNote)
    {
        auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f, width, height,
                                                        PositionConstants::HIGHWAY_POS_START, posEnd);
        strikeColWidth = (fbStrike.rightX - fbStrike.leftX)
                       * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
    }
    else
    {
        const auto& colCoordsRef = isGuitarLike(activePart)
            ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
            : laneCoordsDrums[drumColumnIndex(gemColumn)];
        auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, PositionConstants::GEM_SIZE,
                                         PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = strikeEdge.rightX - strikeEdge.leftX;
    }
    float strikeColHeight = strikeColWidth / imageAspect;

    // --- Compute per-axis frameScale ---
    // Width tracks the lane-width curve; height tracks lane-width × foreshorten
    // (legacy non-isotropic perspective — sprites get squatter at depth).
    // Both axes multiply offsetX/Y and width/height symmetrically inside drawFrame,
    // so the drift invariant offsetY/height = constant holds at every depth.
    //
    // depthForeshorten (debug-tunable) controls how much height compresses
    // beyond width: 0 = no compression (heights match widths), 1 = full
    // foreshorten. Default 0.80.
    //
    // curWidthPx already includes userScale (applied to glyphRect above);
    // strikeColWidth does not — so frameScale naturally encodes userScale.
    float curWidthPx = glyphRect.getWidth();
    float widthRatio = (strikeColWidth > 0.0f) ? (curWidthPx / strikeColWidth) : 1.0f;
    juce::Point<float> frameScale(widthRatio, widthRatio * foreshorten);

    // --- Per-column adjustments + zOff (in strike-reference pixels) ---
    // In Bemani mode: no perspective Z offsets or depth-based column scaling — everything is flat.
    float zOff = 0.0f;
    if (!PositionMath::bemaniMode)
    {
        // Cymbals get their own Z so they can be tuned independently from toms
        // (cym artwork sits higher in the rect, so it tends to need a different lift).
        bool isCymbalGem = !barNote && isDrums
            && (gemWrapper.gem == Gem::CYM
                || gemWrapper.gem == Gem::CYM_GHOST
                || gemWrapper.gem == Gem::CYM_ACCENT);
        zOff = barNote ? barZOffset : (isCymbalGem ? cymZOffset : gemZOffset);
        float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
        if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
            const auto& ca = guitarColAdjust[gemColumn];
            colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
        } else if (isDrums) {
            uint drumIdx = drumColumnIndex(gemColumn);
            const auto& ca = drumColAdjust[drumIdx];
            colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
            if (!barNote)
                zOff += ca.z;
        }

        // Per-column scale: interpolate uniform scale, multiply with per-axis
        float vpDepth = config->getPerspectiveParams().vanishingPointDepth;
        float t = juce::jlimit(0.0f, 1.0f, position / vpDepth);
        float colScale = colSNear + (colSFar - colSNear) * t;
        wScale *= colScale * colW;
        hScale *= colScale * colH;
        oWScale *= colScale * colW;
        oHScale *= colScale * colH;
    }

    // Compute global arc Y offset so adjacent notes form a continuous parabola.
    // Fretboard width is queried directly (not scaled from per-lane normWidth1)
    // so the arc amplitude is independent of lane coordinate tuning.
    // arcOffset is in current-pixel space, so it's applied to the anchor (not
    // subject to frameScale — matches legacy behavior).
    float arcOffset = 0.0f;
    if (curvature != 0.0f && !barNote)
    {
        float dist = getColumnDistFromCenter(gemColumn, isDrums);
        auto fbEdge = PositionMath::getFretboardEdge(isDrums, adjustedPosition, width, height,
                                                      PositionConstants::HIGHWAY_POS_START, posEnd);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * PositionConstants::FRETBOARD_SCALE;
        arcOffset = fbWidthPx * curvature * (1.0f - dist * dist);
    }

    // --- Anchor: screen-space origin of this frame ---
    juce::Point<float> anchor(glyphRect.getCentreX(),
                              glyphRect.getCentreY() + arcOffset);

    // --- Build the frame in strike-reference pixel space ---
    PositionConstants::Frame frame;
    frame.position = position;
    frame.column   = (int)gemColumn;
    frame.isBar    = barNote;

    // Gem sprite
    {
        PositionConstants::FrameSprite s;
        s.image      = glyphImage;
        s.offsetX    = 0.0f;
        s.offsetY    = zOff;  // strike-reference pixel lift; drawFrame scales uniformly
        s.width      = strikeColWidth  * wScale;
        s.height     = strikeColHeight * hScale;
        s.drawOrder  = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
        s.drawColumn = (int)gemColumn;
        s.opacity    = opacity;
        frame.sprites.push_back(s);
    }

    // Overlay sprite (accent/ghost ring) — present for specific gem types
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

        // Overlay's "rect scale" (from getOverlayGlyphRect) = scaleX*scale × scaleY*scale
        float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
        float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;

        PositionConstants::FrameSprite s;
        s.image      = overlayImage;
        // Width/height in strike-reference pixels, before offsetX/offsetY translation
        s.width      = strikeColWidth  * oWScale * ovlRectSX;
        s.height     = strikeColHeight * oHScale * ovlRectSY;
        // offsetX/offsetY are expressed as a fraction of the overlay's own drawn
        // width/height; they translate with frameScale because sprite size does too.
        s.offsetX    = overlayAdj.offsetX * s.width;
        s.offsetY    = zOff + overlayAdj.offsetY * s.height;
        s.drawOrder  = (int)DrawOrder::OVERLAY;
        s.drawColumn = (int)gemColumn;
        s.opacity    = opacity;
        frame.sprites.push_back(s);
    }

    // --- Curvature swap: replace each sprite's image with the cached curved
    // variant; adjust height to the curved aspect and re-add the content Y
    // offset so the opaque cone sits where the straight asset's cone would. ---
    if (curvature != 0.0f)
    {
        const auto& gemEntry = getCurvedImage(glyphImage, gemColumn, isDrums);
        float gemCurvedAspect = (float)gemEntry.image.getWidth()
                              / (float)gemEntry.image.getHeight();
        // Legacy: curvedH = glyphRect.getWidth() / cachedAspect, then scaleRect(_, wScale, hScale)
        // → painted height = curvedH * hScale = (glyphRect.getWidth() / cachedAspect) * hScale.
        // In strike-ref: (strikeColWidth / gemCurvedAspect) * hScale. wScale is NOT in this factor.
        float gemCurvedStrikeHeight = (strikeColWidth / gemCurvedAspect) * hScale;
        // contentYOff multiplier in legacy = glyphRect.getHeight() = strikeColHeight at strike
        // (NO hScale — it's the pre-scaleRect rect height).
        float gemContentYBaseStrike = strikeColHeight;
        frame.sprites[0].image   = const_cast<juce::Image*>(&gemEntry.image);
        frame.sprites[0].height  = gemCurvedStrikeHeight;
        frame.sprites[0].offsetY += gemEntry.yOffsetFraction * gemContentYBaseStrike;

        if (overlayImage != nullptr && overlayAdjPtr != nullptr)
        {
            const auto& ovlEntry = getCurvedImage(overlayImage, gemColumn, isDrums);
            float ovlCurvedAspect = (float)ovlEntry.image.getWidth()
                                  / (float)ovlEntry.image.getHeight();
            auto& ovlSprite = frame.sprites[1];
            float ovlRectSX = overlayAdjPtr->scaleX * overlayAdjPtr->scale;
            float ovlRectSY = overlayAdjPtr->scaleY * overlayAdjPtr->scale;
            // Legacy contentYOff multiplier = overlayRect.getHeight() = glyphRect.getHeight()
            // * (scaleY*scale), which in strike-ref = strikeColHeight * ovlRectSY
            // (WITHOUT oHScale — overlayRect is pre-scaleRect).
            float ovlContentYBaseStrike = strikeColHeight * ovlRectSY;
            // Curved overlay strike height = (strikeColWidth * scaleX*scale) / curvedAspect * oHScale
            // (legacy: curvedH = overlayRect.getWidth() / cachedAspect, then scaleRect by oHScale).
            float ovlCurvedStrikeHeight = (strikeColWidth * ovlRectSX) / ovlCurvedAspect * oHScale;

            ovlSprite.image  = const_cast<juce::Image*>(&ovlEntry.image);
            ovlSprite.height = ovlCurvedStrikeHeight;
            // offsetX uses curved width (same as straight width — width scale unchanged).
            // offsetY uses the curved height for overlayAdj.offsetY translation
            // (legacy used curvedOverlayRect.getHeight(), which is the curved height).
            ovlSprite.offsetX = overlayAdjPtr->offsetX * ovlSprite.width;
            ovlSprite.offsetY = zOff
                              + ovlEntry.yOffsetFraction * ovlContentYBaseStrike
                              + overlayAdjPtr->offsetY * ovlSprite.height;
        }
    }

    // --- Hand off to frame renderer: one scale applied uniformly to all sprites ---
    PositionConstants::drawFrame(frame, anchor, frameScale, *currentDrawCallMap);
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
