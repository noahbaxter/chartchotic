/*
    ==============================================================================

        FrameRenderer.h
        Created by Claude Code
        Author: Noah Baxter

        Renders a Frame: iterates its sprites, applies anchor + per-axis
        scale, enqueues draw calls.

    ==============================================================================
*/

#pragma once

#include "Frame.h"
#include "DrawingConstants.h"

namespace PositionConstants
{
    /** Enqueue draw calls for every sprite in `frame` into `drawCalls`.
        Each sprite's pixel center = `anchor + (offsetX * scale.x, offsetY * scale.y)`.
        Each sprite's pixel size   = `(width * scale.x, height * scale.y)`.

        Per-axis scale lets the caller decouple width and height shrinkage —
        Chartchotic's perspective shrinks sprite width with the lane curve and
        sprite height with that curve × `foreshorten`. Both `offsetY` and
        `height` multiply by `scale.y`, so the drift-fix invariant
        (`offsetY / height = constant`) holds regardless of the scale values.

        anchor: projected screen position of the frame's logical origin
        scale:  (scaleX, scaleY) — per-axis perspective scale factors */
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   juce::Point<float> scale,
                   DrawCallMap& drawCalls);
}
