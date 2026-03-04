/*
    ==============================================================================

        GlyphRenderer.cpp
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        Overlay glyph positioning (e.g., drum accent scaling).

    ==============================================================================
*/

#include "GlyphRenderer.h"

using namespace PositionConstants;

//==============================================================================
// Overlay Positioning

juce::Rectangle<float> GlyphRenderer::getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent)
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
