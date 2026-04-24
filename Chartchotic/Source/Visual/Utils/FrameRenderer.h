/*
    ==============================================================================

        FrameRenderer.h
        Created by Claude Code
        Author: Noah Baxter

        Renders a Frame: iterates its sprites, applies anchor + frameScale
        uniformly, enqueues draw calls.

    ==============================================================================
*/

#pragma once

#include "Frame.h"
#include "DrawingConstants.h"

namespace PositionConstants
{
    /** Enqueue draw calls for every sprite in `frame` into `drawCalls`.
        Each sprite's pixel center = `anchor + (offsetX, offsetY) * frameScale`.
        Each sprite's pixel size   = `(width, height) * frameScale`.

        anchor:     projected screen position of the frame's logical origin
        frameScale: single perspective scale factor applied uniformly */
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   float frameScale,
                   DrawCallMap& drawCalls);
}
