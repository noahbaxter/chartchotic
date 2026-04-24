/*
    ==============================================================================

        Frame.h
        Created by Claude Code
        Author: Noah Baxter

        Frame abstraction: a rhythm-game object as a collection of sprites
        sharing one perspective scale. Built in pixel space at strike
        reference; rendered by applying a single frameScale uniformly to
        every member's offset and size.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace PositionConstants
{
    /** One sprite in a frame. Offset + size are in pixels at the frame's
        strike reference scale. Multiplied by frameScale at render time. */
    struct FrameSprite
    {
        juce::Image* image = nullptr;
        float offsetX   = 0.0f;   // pixels from anchor.x at strike reference
        float offsetY   = 0.0f;   // pixels from anchor.y at strike reference
        float width     = 0.0f;   // strike-reference width in pixels
        float height    = 0.0f;   // strike-reference height in pixels
        int   drawOrder = 0;      // DrawOrder enum value (cast to int)
        int   drawColumn = 0;     // DrawCallMap column bucket (0..MAX_DRAW_COLUMNS-1)
        float opacity   = 1.0f;
    };

    /** A rhythm-game object. Only the sprites list carries layout info;
        position/column/isBar are metadata for the caller's convenience
        (the caller projects the anchor and computes frameScale). */
    struct Frame
    {
        float position = 0.0f;   // music position (depth along highway)
        int   column   = -1;     // -1 = full-width (bar/gridline)
        bool  isBar    = false;  // selects fretboard vs lane perspective path
        std::vector<FrameSprite> sprites;
    };
}
