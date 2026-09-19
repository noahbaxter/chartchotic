/*
    ==============================================================================

        SustainRenderer.cpp
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "SustainRenderer.h"
#include "../Geometry/RenderTypeConfig.h"
#include "../Art/BarGemArt.h"
#include "../../Editor/AuthoringTypes.h"
#include "../PhraseMath.h"

using namespace PositionConstants;

SustainRenderer::SustainRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void SustainRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                               double windowStartTime, double windowEndTime,
                               uint width, uint height, bool showLanes, bool showSustains,
                               float posEnd,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               const NormalizedCoordinates* laneCoords)
{
    currentDrawCallMap = &drawCallMap;
    this->showLanes = showLanes;
    this->showSustains = showSustains;
    this->laneCoords = laneCoords;
    setFrame(activePart, width, height, posEnd, farFadeEnd, farFadeLen, farFadeCurve);

    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, windowStartTime, windowEndTime);
    }
}

void SustainRenderer::drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    if (sustain.sustainType == SustainType::HIHAT)
    {
        drawHiHatSustain(sustain, windowStartTime, windowEndTime);
        return;
    }

    // Phrases from the editor views. Only the caller decides whether they are shown, so the
    // lane and sustain toggles do not hide them.
    if (sustain.sustainType == SustainType::STAR_POWER || sustain.sustainType == SustainType::SOLO
        || sustain.sustainType == SustainType::BRE || sustain.sustainType == SustainType::FILL
        || sustain.sustainType == SustainType::ACTIVATION)
    {
        drawPhrase(sustain, windowStartTime, windowEndTime);
        return;
    }

    double windowTimeSpan = windowEndTime - windowStartTime;

    bool isLane = (sustain.sustainType == SustainType::LANE);

    // The 108 roll lane sits on a virtual pedal column, which has no lane coords of its own, so
    // it borrows the hi-hat's and spans the pedal zone like the bars it covers.
    bool pedalLane = isLane && getRenderType(activePart) == RenderType::ELITE_DRUMS
                            && isElitePedalColumn(sustain.gemColumn);
    uint drawColumn = pedalLane ? (uint)ELITE_HIHAT_COLUMN : sustain.gemColumn;

    // Gate by render toggle
    if (isLane && !showLanes) return;
    if (!isLane && !showSustains) return;

    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // Determine clip position
    float clipPos;
    if (isLane)
    {
        clipPos = HIGHWAY_POS_START;
    }
    else if (hitAnimationsOn)
    {
        clipPos = isBarNote(sustain.gemColumn, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS)
            ? BAR_SUSTAIN_CLIP : SUSTAIN_CLIP;
    }
    else
    {
        clipPos = HIGHWAY_POS_START;
    }

    double clipTime = clipPos * windowTimeSpan;
    if (sustain.endTime < clipTime) return;

    double clippedStartTime = std::max(clipTime, sustain.startTime);

    // Position offsets differ for lanes vs sustains
    float startOffset, endOffset;
    if (isLane)
    {
        startOffset = laneShape.startOffset;
        endOffset = laneShape.endOffset;
    }
    else if (isBarNote(sustain.gemColumn, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS))
    {
        startOffset = BAR_SUSTAIN_START_OFFSET;
        endOffset = BAR_SUSTAIN_END_OFFSET;
    }
    else
    {
        startOffset = SUSTAIN_START_OFFSET;
        endOffset = SUSTAIN_END_OFFSET;
    }

    // In Bemani mode, lanes use pixel-based padding (applied in drawSustainBody),
    // sustains still use position-space offsets
    if (PositionMath::bemaniMode)
    {
        if (isLane)
        {
            // Zero out position-space offsets — pixel padding applied after Y conversion
            startOffset = 0.0f;
            endOffset = 0.0f;
        }
        else
        {
            startOffset = bemaniConfig.sustStartOff;
            endOffset = bemaniConfig.sustEndOff;
        }
    }

    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan) + startOffset;
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan) + endOffset;

    if (endPosition < clipPos || startPosition > farFadeEnd) return;

    startPosition = std::max(clipPos, startPosition);
    endPosition = std::min(farFadeEnd, endPosition);

    bool starPowerActive = state.getProperty("starPower");
    bool shouldBeWhite = starPowerActive && sustain.gemType.starPower;
    auto colour = assetManager.getNoteColour(drawColumn, activePart, sustain.gemType.gem, shouldBeWhite);

    for (const auto& ts : tintedSustains)
    {
        if (ts.lane != (int)sustain.gemColumn) continue;
        bool match = ts.matchStart
            ? std::abs(sustain.startTime - ts.startTime) < kTimeEpsilon
            : (sustain.endTime > ts.startTime - kTimeEpsilon && sustain.startTime < ts.endTime + kTimeEpsilon);
        if (match) { colour = ts.colour; break; }
    }

    float opacity, sustainWidth;
    DrawOrder sustainDrawOrder;
    bool isKickCol = isDrumKick(sustain.gemColumn, activePart);
    switch (sustain.sustainType) {
        case SustainType::LANE:
            opacity = LANE_OPACITY;
            if (pedalLane)      sustainWidth = HIHAT_SUSTAIN_LANE_SPAN / PositionConstants::GEM_SIZE;
            else if (isKickCol) sustainWidth = LANE_OPEN_WIDTH;
            else                sustainWidth = LANE_WIDTH;
            sustainDrawOrder = DrawOrder::LANE;
            break;
        case SustainType::SUSTAIN:
        default:
            opacity = SUSTAIN_OPACITY;
            sustainWidth = isKickCol ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            sustainDrawOrder = isKickCol ? DrawOrder::BAR : DrawOrder::SUSTAIN;
            break;
    }

    (*currentDrawCallMap)[static_cast<int>(sustainDrawOrder)][drawColumn].push_back([=](juce::Graphics& g) {
        this->drawSustainBody(g, drawColumn, startPosition, endPosition, opacity, sustainWidth, colour, isLane);
    });
}

void SustainRenderer::drawPhrase(const TimeBasedSustainEvent& phrase, double windowStartTime, double windowEndTime)
{
    const double windowTimeSpan = windowEndTime - windowStartTime;
    // Same offset gridlines take, so a phrase on a gridline's tick lands on that gridline.
    float startPos = (float)((phrase.startTime - windowStartTime) / windowTimeSpan) + phrasePosOffset;
    float endPos   = (float)((phrase.endTime   - windowStartTime) / windowTimeSpan) + phrasePosOffset;
    startPos = std::max((float)HIGHWAY_POS_START, startPos);
    endPos   = std::min(farFadeEnd, endPos);
    if (endPos <= startPos) return;

    // Star power and solo light the whole board; a fill or BRE is one lane per hand column.
    const bool starPower = phrase.sustainType == SustainType::STAR_POWER;
    const bool solo = phrase.sustainType == SustainType::SOLO;
    const bool bre = phrase.sustainType == SustainType::BRE;
    const bool fill = phrase.sustainType == SustainType::FILL;
    const bool activation = phrase.sustainType == SustainType::ACTIVATION;
    const bool band = starPower || solo;
    const uint column = phrase.gemColumn;

    // The colour spread across the lanes is what these toggles hide. The activation band is not
    // decoration, it says where star power can be taken, so it survives both being off.
    if (fill && !showFills) return;
    if (bre && !showBre)    return;
    if (activation && !showActivationLane) return;

    // The phrase carries the gem it was built from, so the swell pointing at a cymbal is
    // painted as that cymbal rather than as the tom sharing its lane.
    const juce::Colour laneColour =
        assetManager.getNoteColour(column, activePart, phrase.gemType.gem, false);

    const juce::Colour colour = starPower ? PHRASE_STAR_POWER_COLOUR
                              : solo      ? PHRASE_SOLO_COLOUR
                                          : laneColour;
    // In the phrase view a phrase is what you came to look at, so it draws stronger. In the note
    // views it is context sitting over the gems, and stays faint enough to read them through.
    const float base = starPower  ? PHRASE_STAR_POWER_OPACITY
                     : solo       ? PHRASE_SOLO_OPACITY
                     : activation ? PHRASE_ACTIVATION_OPACITY
                                  : LANE_OPACITY;
    const float opacity = !phraseViewActive ? base
                        : starPower         ? PHRASE_STAR_POWER_VIEW_OPACITY
                                            : std::min(1.0f, base * PHRASE_VIEW_OPACITY_BOOST);
    const float gridZ = phraseGridZ;

    (*currentDrawCallMap)[(int)DrawOrder::LANE][band ? 0 : column].push_back([=](juce::Graphics& g) {
        const auto* config = currentConfig;
        const auto& fbCoords = *config->fretboardCoords;
        const RenderType rt = getRenderType(activePart);

        // Left and right of what the phrase covers at a depth: the board, or one hand column.
        auto spanAt = [&](float pos) {
            if (band)
            {
                auto e = getColumnEdge(pos, fbCoords, 1.0f);
                return std::pair{ e.leftX, e.rightX };
            }
            uint laneIdx = resolveLaneIndex(column);
            auto e = getColumnEdge(pos, laneCoords[laneIdx], 1.0f, PositionConstants::FRETBOARD_SCALE,
                                   PositionMath::bemaniMode ? (int)laneIdx - 1 : -1);
            return std::pair{ e.leftX, e.rightX };
        };

        // The measure gridline's centreline at a depth, reproduced from how GridlineRenderer
        // sizes the baked marker: GRIDLINE_WIDTH_SCALE wide, its height off the strikeline
        // aspect, scaled by depth, nudged by the grid z offset.
        auto strike = getColumnEdge(0.0f, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
        const float strikeW = strike.rightX - strike.leftX;
        const auto persp = config->getPerspectiveParams();
        const float strikeH = (persp.barNoteHeightRatio > 0.0f ? strikeW / persp.barNoteHeightRatio : strikeW)
                            / config->boardWidthScale;
        const float arch = GemArt::gridlineArch();
        const float thickness = GemArt::kGridlineMeasureThickness;
        const float canvasH = (float)GemArt::gridlineCanvasHeight(thickness, arch);

        auto lineY = [&](float pos, float x) {
            if (PositionMath::bemaniMode)
                return PositionMath::getFretboardEdge(rt, pos, width, height, HIGHWAY_POS_START, posEnd).centerY
                     + bemaniConfig.gridlineZ;

            auto gl = getColumnEdge(pos, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
            const float w = gl.rightX - gl.leftX;
            const float wr = strikeW > 0.0f ? w / strikeW : 1.0f;
            const float h = strikeH * wr;
            const float top = gl.centerY + gridZ * wr - h * 0.5f;
            const float canvasX = w > 0.0f ? (x - gl.leftX) * (float)GemArt::kGridlineWidth / w : 0.0f;
            return top + GemArt::gridlineCentreY(canvasX, thickness, arch) * h / canvasH;
        };

        constexpr int samples = 24;

        // A quad of the phrase between two depths and two fractions across its width, curved to
        // follow the gridlines at both ends. The whole width from end to end for most phrases;
        // the activation band builds a run of narrow ones to fade across its rails.
        auto quad = [&](float pFar, float pNear, float f0, float f1) {
            auto [fl, fr] = spanAt(pFar);
            auto [nl, nr] = spanAt(pNear);
            juce::Path path;
            for (int i = 0; i <= samples; ++i)
            {
                float f = f0 + (f1 - f0) * (float)i / (float)samples;
                float x = fl + (fr - fl) * f;
                if (i == 0) path.startNewSubPath(x, lineY(pFar, x));
                else        path.lineTo(x, lineY(pFar, x));
            }
            for (int i = 0; i <= samples; ++i)
            {
                float f = f1 - (f1 - f0) * (float)i / (float)samples;
                float x = nl + (nr - nl) * f;
                path.lineTo(x, lineY(pNear, x));
            }
            path.closeSubPath();
            return path;
        };

        const float fadeStart = farFadeEnd - farFadeLen;

        // The activation band fades two ways at once: along its length, nothing at the far end and
        // full where it meets the gem, and across each rail, full at the lane's outer edge and
        // nothing toward the middle. A fill takes one gradient, so the crossing fade comes from
        // banding each rail into strips and scaling the length gradient's alpha per strip.
        if (activation)
        {
            const bool barLane = isBarNote(column, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
            const float hole = (barLane ? PHRASE_ACTIVATION_HOLE_FRACTION_BAR
                                        : PHRASE_ACTIVATION_HOLE_FRACTION) * 0.5f;

            // Whichever way the band is cut, one of its two fades gets a real gradient and the
            // other is stepped per strip. A lane's rails are narrow, so cutting them lengthwise
            // keeps the strips thin and the length fade smooth; a kick's lane is the whole board,
            // where those same strips are wide enough to read one by one, so it cuts across depth
            // instead and takes its gradient across the width.
            auto rail = [&](float fOuter, float fInner) {
                if (barLane)
                {
                    constexpr int bands = PHRASE_ACTIVATION_DEPTH_BANDS;
                    for (int i = 0; i < bands; ++i)
                    {
                        const float u0 = (float)i / (float)bands;
                        const float u1 = (float)(i + 1) / (float)bands;
                        const float pNear = startPos + (endPos - startPos) * u0;
                        const float pFar = startPos + (endPos - startPos)
                                         * std::min(1.0f, u1 + PHRASE_ACTIVATION_EDGE_OVERLAP / (float)bands);

                        // Length fade, stepped: sampled at the band's midpoint.
                        const float mid = (u0 + u1) * 0.5f;
                        const float a = opacity * mid;

                        if (PositionMath::bemaniMode)
                        {
                            g.setColour(colour.withAlpha(a));
                        }
                        else
                        {
                            // Crossing fade as a real gradient across the rail at this depth.
                            const float pMid = startPos + (endPos - startPos) * mid;
                            auto [ml, mr] = spanAt(pMid);
                            juce::ColourGradient grad(colour.withAlpha(a), ml + (mr - ml) * fOuter, 0.0f,
                                                      colour.withAlpha(0.0f), ml + (mr - ml) * fInner, 0.0f,
                                                      false);
                            // Stops bend the ramp: a plain gradient is linear, the curve is not.
                            for (int s = 1; s < PHRASE_ACTIVATION_EDGE_CURVE_STOPS; ++s)
                            {
                                const float t = (float)s / (float)PHRASE_ACTIVATION_EDGE_CURVE_STOPS;
                                grad.addColour(t, colour.withAlpha(a * std::pow(1.0f - t,
                                                                   PHRASE_ACTIVATION_EDGE_FADE_CURVE)));
                            }
                            g.setGradientFill(grad);
                        }
                        g.fillPath(quad(pFar, pNear, fOuter, fInner));
                    }
                    return;
                }

                // Strip count follows the rail's width in pixels, so a wider lane gets more strips
                // rather than wider ones.
                auto [wl, wr] = spanAt(endPos);
                const float railPx = std::abs(wr - wl) * std::abs(fInner - fOuter);
                const int strips = juce::jlimit(PHRASE_ACTIVATION_EDGE_STRIPS_MIN,
                                                PHRASE_ACTIVATION_EDGE_STRIPS_MAX,
                                                (int)(railPx / PHRASE_ACTIVATION_EDGE_STRIP_PX));

                const auto bounds = quad(endPos, startPos, 0.0f, 1.0f).getBounds();
                for (int i = 0; i < strips; ++i)
                {
                    const float t0 = (float)i / (float)strips;
                    const float t1 = (float)(i + 1) / (float)strips;
                    const float fa = fOuter + (fInner - fOuter) * t0;
                    const float fb = fOuter + (fInner - fOuter)
                                   * std::min(1.0f, t1 + PHRASE_ACTIVATION_EDGE_OVERLAP / (float)strips);

                    // Crossing fade, stepped per strip; the length fade stays a real gradient.
                    const float edge = std::pow(1.0f - (t0 + t1) * 0.5f, PHRASE_ACTIVATION_EDGE_FADE_CURVE);
                    const float a = opacity * edge;

                    if (PositionMath::bemaniMode)
                        g.setColour(colour.withAlpha(a));
                    else
                        g.setGradientFill(juce::ColourGradient(colour.withAlpha(a), 0.0f, bounds.getY(),
                                                               colour.withAlpha(0.0f), 0.0f, bounds.getBottom(),
                                                               false));
                    g.fillPath(quad(endPos, startPos, fa, fb));
                }
            };

            rail(0.0f, 0.5f - hole);
            rail(1.0f, 0.5f + hole);

            // A cap across the top end, where the band meets the note. Without it the rails just
            // run off the end as two separate strips; the cap closes them into one lane that stops
            // at the gem. Full width and no crossing fade, so it reads as the band's terminus.
            auto [cl, cr] = spanAt(endPos);
            // Thickness off the lane's width, so a kick -- whose lane is the whole board -- needs
            // its own far smaller fraction. The same number there is a slab deep enough to bury
            // the bars behind it and hide where they start.
            const float capH = (cr - cl) * (barLane ? PHRASE_ACTIVATION_CAP_THICKNESS_BAR
                                                    : PHRASE_ACTIVATION_CAP_THICKNESS);
            juce::Path cap;
            for (int i = 0; i <= samples; ++i)
            {
                const float x = cl + (cr - cl) * (float)i / (float)samples;
                if (i == 0) cap.startNewSubPath(x, lineY(endPos, x));
                else        cap.lineTo(x, lineY(endPos, x));
            }
            for (int i = samples; i >= 0; --i)
            {
                const float x = cl + (cr - cl) * (float)i / (float)samples;
                cap.lineTo(x, lineY(endPos, x) + capH);
            }
            cap.closeSubPath();

            g.setColour(colour.withAlpha(opacity));
            g.fillPath(cap);
            return;
        }

        juce::Path path = quad(endPos, startPos, 0.0f, 1.0f);

        auto paint = [&](const juce::Path& p, juce::Colour c, float alpha)
        {
            if (!PositionMath::bemaniMode && endPos > fadeStart)
            {
                float fadeFrom = std::max(fadeStart, startPos);
                float yFrom = PositionMath::getFretboardEdge(rt, fadeFrom, width, height, HIGHWAY_POS_START, posEnd).centerY;
                float yTo   = PositionMath::getFretboardEdge(rt, farFadeEnd, width, height, HIGHWAY_POS_START, posEnd).centerY;
                g.setGradientFill(juce::ColourGradient(c.withAlpha(calculateOpacity(fadeFrom) * alpha), 0.0f, yFrom,
                                                       c.withAlpha(0.0f), 0.0f, yTo, false));
            }
            else
            {
                g.setColour(c.withAlpha(alpha));
            }
            g.fillPath(p);
        };

        if (fill)
        {
            // Gutters. The lanes are not evenly spaced to begin with -- neighbours sit further
            // apart than an outer lane sits from the highway edge -- so each side is brought in by
            // whatever its own gap is short of the target, half of it per lane where two fills
            // share a gap. Insetting every side the same amount would only widen the wider gap.
            float gutL = 0.0f, gutR = 0.0f;
            if (laneCoords != nullptr && laneCount > 0)
            {
                auto edgeOf = [&](uint i) {
                    return getColumnEdge(startPos, laneCoords[i], 1.0f, PositionConstants::FRETBOARD_SCALE,
                                         PositionMath::bemaniMode ? (int)i - 1 : -1);
                };
                const auto hands = PhraseMath::handColumns(activePart);
                const uint idx = resolveLaneIndex(column);
                const uint firstIdx = resolveLaneIndex(hands.first);
                const uint lastIdx = resolveLaneIndex(hands.second);
                auto board = getColumnEdge(startPos, fbCoords, 1.0f);
                auto me = edgeOf(idx);
                const float laneW = me.rightX - me.leftX;

                if (laneW > 0.0f)
                {
                    const float target = laneW * PHRASE_FILL_GUTTER_FRACTION;
                    // A gap shared with the next fill is closed from both sides, so each lane owes
                    // half of it; the gap at the highway edge is this lane's alone.
                    const bool sharedL = idx > firstIdx, sharedR = idx < lastIdx;
                    const float haveL = sharedL ? me.leftX - edgeOf(idx - 1).rightX
                                                : me.leftX - board.leftX;
                    const float haveR = sharedR ? edgeOf(idx + 1).leftX - me.rightX
                                                : board.rightX - me.rightX;
                    gutL = std::max(0.0f, (target - haveL) * (sharedL ? 0.5f : 1.0f)) / laneW;
                    gutR = std::max(0.0f, (target - haveR) * (sharedR ? 0.5f : 1.0f)) / laneW;
                }
            }

            // What the border costs in pixels at a depth, turned into the depth that buys it, so
            // the two ends come in as far as the sides do.
            auto depthInset = [&](float pos) {
                auto [l, r] = spanAt(pos);
                const float px = std::abs(r - l) * PHRASE_FILL_BORDER_FRACTION;
                auto midY = [&](float p) { auto [a, b] = spanAt(p); return lineY(p, (a + b) * 0.5f); };
                const float dy = std::abs(midY(pos + PHRASE_FILL_DEPTH_PROBE) - midY(pos));
                return dy > 0.0f ? px * PHRASE_FILL_DEPTH_PROBE / dy : 0.0f;
            };
            const float innerFar  = endPos   - depthInset(endPos);
            const float innerNear = startPos + depthInset(startPos);

            if (innerFar > innerNear)
            {
                juce::Path outer = quad(endPos, startPos, gutL, 1.0f - gutR);
                juce::Path inner = quad(innerFar, innerNear, gutL + PHRASE_FILL_BORDER_FRACTION,
                                        1.0f - gutR - PHRASE_FILL_BORDER_FRACTION);
                // Border as a ring rather than a slab under the interior: two translucent layers
                // stacked would tint the interior back toward the border colour.
                juce::Path border = outer;
                border.addPath(inner);
                border.setUsingNonZeroWinding(false);
                paint(border, colour, opacity);
                paint(inner, colour.darker(PHRASE_FILL_INNER_DARKEN), PHRASE_FILL_INNER_OPACITY);
                return;
            }
        }

        paint(path, colour, opacity);
    });
}

void SustainRenderer::drawHiHatSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    if (!showSustains) return;

    const uint col = (uint)ELITE_HIHAT_COLUMN;
    double windowTimeSpan = windowEndTime - windowStartTime;
    double clipTime = HIGHWAY_POS_START * windowTimeSpan;
    if (sustain.endTime < clipTime) return;

    auto toPos = [&](double t) {
        return (float)((std::max(clipTime, t) - windowStartTime) / windowTimeSpan);
    };
    float startPos = std::max((float)HIGHWAY_POS_START, toPos(sustain.startTime));
    float endPos   = std::min(farFadeEnd, toPos(sustain.endTime));
    float fadePos  = juce::jlimit(startPos, endPos, toPos(sustain.fadeStartTime));

    if (endPos <= (float)HIGHWAY_POS_START || startPos >= farFadeEnd) return;

    // One colour for every generator: a ring means the hat is open, however it got opened. Under
    // strict an Open hat and a Splash both fire on one tick, and two colours there just stack.
    const juce::Colour body = GemArt::kStompBarFace;

    // drawSustainBody measures off a GEM_SIZE-scaled lane, so divide it back out for a true span.
    const float w = HIHAT_SUSTAIN_LANE_SPAN / PositionConstants::GEM_SIZE;

    if (fadePos > startPos)
        (*currentDrawCallMap)[(int)DrawOrder::LANE][col].push_back([=](juce::Graphics& g) {
            this->drawSustainBody(g, col, startPos, fadePos, HIHAT_SUSTAIN_OPACITY, w, body, true);
        });
    if (endPos > fadePos)
        (*currentDrawCallMap)[(int)DrawOrder::LANE][col].push_back([=](juce::Graphics& g) {
            this->drawSustainBody(g, col, fadePos, endPos, HIHAT_SUSTAIN_FADE_OPACITY, w, body, true);
        });
}

void SustainRenderer::drawSustainBody(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour, bool isLane)
{
    bool isDrums = isDrumLike(activePart);
    const auto* config = currentConfig;
    // Guitar-like keeps GUITAR bar semantics (bar = col 0); drums pass the real
    // part so elite's kick columns (0 and the 2x virtual col) resolve correctly.
    bool isBar = isBarNote(gemColumn, isDrums ? activePart : Part::GUITAR);

    // Look up lane coords
    NormalizedCoordinates colCoords;
    float laneScale;
    int bemaniIdx = -1;
    uint laneCoordIdx = resolveLaneIndex(gemColumn);
    colCoords = laneCoords[laneCoordIdx];
    bemaniIdx = (int)laneCoordIdx - 1;
    if (isDrums)
        laneScale = isDrumKick(gemColumn, activePart) ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
    else
        laneScale = (gemColumn == 0) ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;

    int laneIdx = PositionMath::bemaniMode ? bemaniIdx : -1;
    auto startLane = getColumnEdge(startPosition, colCoords, laneScale, PositionConstants::FRETBOARD_SCALE, laneIdx);
    auto endLane = getColumnEdge(endPosition, colCoords, laneScale, PositionConstants::FRETBOARD_SCALE, laneIdx);

    // Gems sit on a curvature arc; a sustain running off one has to sit on the same arc or it
    // meets its gem at only one depth. Taken per end, because the lift scales with depth: a single
    // offset for the whole body would line the start up and leave the far end out by the
    // difference. Kicks and open bars are drawn flat, so they take no lift.
    if (!PositionMath::bemaniMode && !isBar)
    {
        const float arc = noteArcOffsetStrike(gemColumn, isDrums ? noteCurvatureDrums
                                                                : noteCurvatureGuitar);
        startLane.centerY += arc * depthWidthRatio(startPosition);
        endLane.centerY   += arc * depthWidthRatio(endPosition);
    }

    // Bemani mode: simple shapes, no perspective geometry
    if (PositionMath::bemaniMode)
    {
        float bSustW = bemaniConfig.sustainWidth;
        float bBarSustW = bemaniConfig.barSustainWidth;
        float capFrac = bemaniConfig.sustainCap;

        float laneWidth = startLane.rightX - startLane.leftX;
        float centerX = (startLane.leftX + startLane.rightX) * 0.5f;
        float topY = std::min(startLane.centerY, endLane.centerY);
        float botY = std::max(startLane.centerY, endLane.centerY);

        float zNudge = isLane ? bemaniConfig.laneZ : bemaniConfig.sustainZ;
        topY += zNudge;
        botY += zNudge;

        // Pixel-based lane padding — extends lane past note edges by fixed pixels
        if (isLane)
        {
            float laneEndPxVal = isBar ? config->bemaniBarLaneEndPx() : config->bemaniLaneEndPx();
            topY -= laneEndPxVal;
            botY += isBar ? bemaniConfig.barLaneStartPx : bemaniConfig.laneStartPx;
        }

        if (isBar)
        {
            auto fb = PositionMath::getFretboardEdge(getRenderType(activePart), startPosition, width, height,
                PositionConstants::HIGHWAY_POS_START, posEnd);
            centerX = (fb.leftX + fb.rightX) * 0.5f;
            laneWidth = fb.rightX - fb.leftX;
        }

        if (isLane)
        {
            // Lanes: wider fill with caps on both ends
            float w = isBar ? laneWidth * bemaniConfig.barLaneFillW : laneWidth * bemaniConfig.laneFillW;
            float lx = centerX - w * 0.5f;
            float laneCapFrac = isBar ? bemaniConfig.barCap : bemaniConfig.laneCap;
            float capH = laneCapFrac * w;

            g.setColour(colour.withAlpha(opacity));
            juce::Path path;
            path.startNewSubPath(lx, topY);
            path.quadraticTo(centerX, topY - capH, lx + w, topY);
            path.lineTo(lx + w, botY);
            path.quadraticTo(centerX, botY + capH, lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        else
        {
            // Sustains: narrower, no front cap, small back cap (bar sustains get less cap)
            float w = isBar ? laneWidth * bBarSustW * 0.6f : laneWidth * bSustW;
            float lx = centerX - w * 0.5f;
            float sustCapFrac = isBar ? bemaniConfig.barCap : capFrac;
            float backCapH = sustCapFrac * w * 0.3f;

            g.setColour(colour.withAlpha(opacity));
            juce::Path path;
            // Top (far end) — small back cap
            path.startNewSubPath(lx, topY);
            if (backCapH > 0.5f)
                path.quadraticTo(centerX, topY - backCapH, lx + w, topY);
            else
                path.lineTo(lx + w, topY);
            // Right side down
            path.lineTo(lx + w, botY);
            // Bottom (strikeline end) — flat, no cap
            path.lineTo(lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        return;
    }

    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;

    float startCenterX = (startLane.leftX + startLane.rightX) * 0.5f;
    float endCenterX = (endLane.leftX + endLane.rightX) * 0.5f;

    float startLeftX  = startCenterX - startWidth * 0.5f;
    float startRightX = startCenterX + startWidth * 0.5f;
    float endLeftX    = endCenterX - endWidth * 0.5f;
    float endRightX   = endCenterX + endWidth * 0.5f;

    float startY = startLane.centerY;
    float endY   = endLane.centerY;

    float startCurve, endCurve;
    if (isLane)
    {
        startCurve = laneShape.innerStartArc;
        endCurve = laneShape.innerEndArc;
    }
    else if (isBar)
    {
        startCurve = BAR_SUSTAIN_START_CURVE;
        endCurve = BAR_SUSTAIN_END_CURVE;
    }
    else
    {
        startCurve = SUSTAIN_START_CURVE;
        endCurve = SUSTAIN_END_CURVE;
    }

    const auto& fbCoords = *config->fretboardCoords;
    auto startFretboard = getColumnEdge(startPosition, fbCoords, 1.0f);
    auto endFretboard = getColumnEdge(endPosition, fbCoords, 1.0f);

    float startFretboardWidth = startFretboard.rightX - startFretboard.leftX;
    float endFretboardWidth = endFretboard.rightX - endFretboard.leftX;

    float startArcY = startCurve * startFretboardWidth;
    float endArcY = endCurve * endFretboardWidth;

    float laneStartParabolaY = 0.0f;
    float laneEndParabolaY = 0.0f;
    if (isLane)
    {
        float startT = (startFretboardWidth > 0.0f) ? (startCenterX - startFretboard.leftX) / startFretboardWidth : 0.5f;
        float endT = (endFretboardWidth > 0.0f) ? (endCenterX - endFretboard.leftX) / endFretboardWidth : 0.5f;

        laneStartParabolaY = laneShape.outerStartArc * startFretboardWidth * 4.0f * startT * (1.0f - startT);
        laneEndParabolaY = laneShape.outerEndArc * endFretboardWidth * 4.0f * endT * (1.0f - endT);
    }

    juce::Path path;

    float adjStartY = startY + laneStartParabolaY;
    float adjEndY = endY + laneEndParabolaY;

    path.startNewSubPath(startLeftX, adjStartY);

    float startMidX = (startLeftX + startRightX) * 0.5f;
    path.quadraticTo(startMidX, adjStartY + startArcY, startRightX, adjStartY);
    path.lineTo(endRightX, adjEndY);

    float endMidX = (endLeftX + endRightX) * 0.5f;
    path.quadraticTo(endMidX, adjEndY + endArcY, endLeftX, adjEndY);
    path.closeSubPath();

    // Fill the path — use gradient if sustain extends into the fade zone
    float fadeStart = farFadeEnd - farFadeLen;
    if (endPosition > fadeStart)
    {
        float fadeStartClamped = std::max(fadeStart, startPosition);
        auto fadeStartEdge = PositionMath::getFretboardEdge(getRenderType(activePart), fadeStartClamped, width, height,
            PositionConstants::HIGHWAY_POS_START, posEnd);
        auto fadeEndEdge = PositionMath::getFretboardEdge(getRenderType(activePart), farFadeEnd, width, height,
            PositionConstants::HIGHWAY_POS_START, posEnd);

        float gradStartY = fadeStartEdge.centerY;
        float gradEndY   = fadeEndEdge.centerY;

        float startOpacity = calculateOpacity(fadeStartClamped) * opacity;

        juce::ColourGradient gradient(
            colour.withAlpha(startOpacity), 0.0f, gradStartY,
            colour.withAlpha(0.0f), 0.0f, gradEndY,
            false);

        if (startPosition < fadeStart)
        {
            float fullRange = adjStartY - gradEndY;
            float opaqueRange = adjStartY - gradStartY;
            float opaqueT = (fullRange > 0.0f) ? opaqueRange / fullRange : 0.0f;

            gradient = juce::ColourGradient(
                colour.withAlpha(opacity), 0.0f, adjStartY,
                colour.withAlpha(0.0f), 0.0f, gradEndY,
                false);
            gradient.addColour(opaqueT, colour.withAlpha(opacity));
        }

        g.setGradientFill(gradient);
    }
    else
    {
        g.setColour(colour.withAlpha(opacity));
    }
    g.fillPath(path);
}
