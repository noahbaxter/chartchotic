/*
    ==============================================================================

        FrameRenderer.cpp
        Created by Claude Code
        Author: Noah Baxter

    ==============================================================================
*/

#include "FrameRenderer.h"

namespace PositionConstants
{
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   float frameScale,
                   DrawCallMap& drawCalls)
    {
        for (const auto& sprite : frame.sprites)
        {
            if (sprite.image == nullptr) continue;

            float cx = anchor.x + sprite.offsetX * frameScale;
            float cy = anchor.y + sprite.offsetY * frameScale;
            float w  = sprite.width  * frameScale;
            float h  = sprite.height * frameScale;

            juce::Rectangle<float> rect(cx - w * 0.5f, cy - h * 0.5f, w, h);

            int order = sprite.drawOrder;
            int col   = juce::jlimit(0, MAX_DRAW_COLUMNS - 1, sprite.drawColumn);
            float opacity = sprite.opacity;
            const juce::Image* img = sprite.image;

            drawCalls[order][col].push_back([img, opacity, rect](juce::Graphics& g) {
                g.setOpacity(opacity);
                g.drawImage(*img, rect);
            });
        }
    }
}
