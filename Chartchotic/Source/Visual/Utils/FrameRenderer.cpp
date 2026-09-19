/*
    ==============================================================================

        FrameRenderer.cpp
        Created by Claude Code
        Author: Noah Baxter

    ==============================================================================
*/

#include "FrameRenderer.h"

namespace Render
{
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   juce::Point<float> scale,
                   DrawCallMap& drawCalls)
    {
        // Below half of one 8-bit alpha step the blend cannot change a destination pixel, so the
        // sprite is a guaranteed no-op. Gems keep fading to nothing at the far end of the board
        // and were paying a full transformed blit all the way down.
        constexpr float invisibleAlpha = 0.5f / 255.0f;

        for (const auto& sprite : frame.sprites)
        {
            if (sprite.image == nullptr) continue;
            if (sprite.opacity < invisibleAlpha && sprite.tint.getAlpha() == 0) continue;

            float cx = anchor.x + sprite.offsetX * scale.x;
            float cy = anchor.y + sprite.offsetY * scale.y;
            float w  = sprite.width  * scale.x;
            float h  = sprite.height * scale.y;

            juce::Rectangle<float> rect(cx - w * 0.5f, cy - h * 0.5f, w, h);

            int order = sprite.drawOrder;
            int col   = juce::jlimit(0, MAX_DRAW_COLUMNS - 1, sprite.drawColumn);
            float opacity = sprite.opacity;
            const juce::Image* img = sprite.image;

            juce::Colour tint = sprite.tint;
            auto clipHalf = sprite.clipHalf;
            drawCalls[order][col].push_back([img, opacity, rect, tint, clipHalf](juce::Graphics& g) {
                g.setOpacity(opacity);
                if (clipHalf != ClipHalf::None)
                {
                    juce::Graphics::ScopedSaveState save(g);
                    float halfW = rect.getWidth() * 0.5f;
                    auto clip = (clipHalf == ClipHalf::Left)
                        ? rect.withWidth(halfW)
                        : rect.withX(rect.getX() + halfW).withWidth(halfW);
                    g.reduceClipRegion(clip.toNearestInt());
                    g.drawImage(*img, rect);
                    if (tint.getAlpha() > 0)
                    {
                        auto transform = juce::AffineTransform::scale(
                            rect.getWidth()  / (float)img->getWidth(),
                            rect.getHeight() / (float)img->getHeight())
                            .translated(rect.getX(), rect.getY());
                        g.reduceClipRegion(*img, transform);
                        // setColour replaces the current colour, alpha and all, discarding the
                        // setOpacity above, so the sprite's opacity has to be folded in by hand
                        // or the tint ignores every fade the image itself honours.
                        g.setColour(tint.withMultipliedAlpha(opacity));
                        g.fillAll();
                    }
                }
                else
                {
                    g.drawImage(*img, rect);
                    if (tint.getAlpha() > 0)
                    {
                        auto transform = juce::AffineTransform::scale(
                            rect.getWidth()  / (float)img->getWidth(),
                            rect.getHeight() / (float)img->getHeight())
                            .translated(rect.getX(), rect.getY());
                        juce::Graphics::ScopedSaveState save(g);
                        g.reduceClipRegion(*img, transform);
                        g.setColour(tint.withMultipliedAlpha(opacity));
                        g.fillAll();
                    }
                }
            });
        }
    }
}
