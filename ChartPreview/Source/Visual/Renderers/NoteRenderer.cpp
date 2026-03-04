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

    // When hit animations are on, notes clip at the strikeline (position 0).
    // When off, notes flow past the strikeline to the bottom of the highway.
    double clipTime = hitAnimationsOn ? 0.0 : (HIGHWAY_POS_START * windowTimeSpan);

    for (const auto& frameItem : trackWindow)
    {
        double frameTime = frameItem.first;

        if (frameTime < clipTime) continue;

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
    auto perspParams = PositionConstants::getPerspectiveParams();

    if (isPart(state, Part::GUITAR))
    {
        barNote = isBarNote(gemColumn, Part::GUITAR);
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);

        const auto& colCoords = PositionConstants::guitarGlyphCoords[
            (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1];
        float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        float adjustedPosition = barNote ? position + PositionConstants::BAR_NOTE_POS_OFFSET : position;
        auto edge = getColumnEdge(adjustedPosition, colCoords, sizeScale, PositionConstants::FRETBOARD_SCALE);
        float colWidth = edge.rightX - edge.leftX;
        float colHeight = colWidth / (barNote ? perspParams.barNoteHeightRatio : perspParams.regularNoteHeightRatio);
        glyphRect = juce::Rectangle<float>(edge.leftX, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }
    else
    {
        barNote = isBarNote(gemColumn, Part::DRUMS);
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);

        uint drumIdx = (gemColumn == 6) ? 0 : ((gemColumn < DRUM_LANE_COUNT) ? gemColumn : 1);
        const auto& colCoords = PositionConstants::drumGlyphCoords[drumIdx];
        float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        float adjustedPosition = barNote ? position + PositionConstants::BAR_NOTE_POS_OFFSET : position;
        auto edge = getColumnEdge(adjustedPosition, colCoords, sizeScale, PositionConstants::FRETBOARD_SCALE);
        float colWidth = edge.rightX - edge.leftX;
        float colHeight = colWidth / (barNote ? perspParams.barNoteHeightRatio : perspParams.regularNoteHeightRatio);
        glyphRect = juce::Rectangle<float>(edge.leftX, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

    if (glyphImage == nullptr)
        return;

    // Apply gem scale from state
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
    DrawOrder layer = barNote ? DrawOrder::BAR : DrawOrder::NOTE;
    (*currentDrawCallMap)[layer][gemColumn].push_back([=](juce::Graphics& g) {
        g.setOpacity(opacity);
        g.drawImage(*glyphImage, glyphRect);
    });

    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        bool isDrumAccent = !isPart(state, Part::GUITAR) && gemWrapper.gem == Gem::TAP_ACCENT;
        juce::Rectangle<float> overlayRect = getOverlayGlyphRect(glyphRect, isDrumAccent);

        (*currentDrawCallMap)[DrawOrder::OVERLAY][gemColumn].push_back([=](juce::Graphics& g) {
            g.setOpacity(opacity);
            g.drawImage(*overlayImage, overlayRect);
        });
    }
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
