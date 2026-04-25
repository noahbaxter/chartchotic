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

namespace Render
{
    /** Enqueue draw calls for every sprite in `frame` into `drawCalls`.
        Each sprite's pixel center = `anchor + (offsetX * scale.x, offsetY * scale.y)`.
        Each sprite's pixel size   = `(width * scale.x, height * scale.y)`.

        Both `offsetY` and `height` scale by the same `scale.y`, so within a
        frame the offset-to-size ratio between any two sprites is invariant
        with depth — composite members can't drift apart by construction.

        anchor: projected screen position of the frame's logical origin
        scale:  (scaleX, scaleY) — per-axis perspective scale factors */
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   juce::Point<float> scale,
                   DrawCallMap& drawCalls);
}
