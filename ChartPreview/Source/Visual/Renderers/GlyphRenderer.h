/*
    ==============================================================================

        GlyphRenderer.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        Overlay glyph positioning (e.g., drum accent scaling).

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PositionConstants.h"

class GlyphRenderer
{
public:
    GlyphRenderer() = default;
    ~GlyphRenderer() = default;

    //==============================================================================
    // Overlay Positioning
    juce::Rectangle<float> getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent);
};
