/*
    ==============================================================================

        TrackFade.h
        Author:  Noah Baxter

        Creates a faded track image with far-end fade baked in per-pixel.
        Extracted from PluginEditor.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PositionMath.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/DrawingConstants.h"

inline juce::Image createFadedTrackImage(const juce::Image& source, int w, int h,
                                          bool isDrums, float fadeEnd, float fadeLen, float fadeCurve,
                                          float wNear, float wMid, float wFar, float posEnd)
{
    if (w <= 0 || h <= 0) return {};

    juce::Image fadedImage(juce::Image::ARGB, w, h, true);
    {
        juce::Graphics offG(fadedImage);
        offG.drawImage(source, juce::Rectangle<float>(0, 0, (float)w, (float)h),
                       juce::RectanglePlacement::centred);
    }

    float fadeStart = fadeEnd - fadeLen;

    auto edgeAtFadeStart = PositionMath::getFretboardEdge(isDrums, fadeStart, w, h, wNear, wMid, wFar,
        PositionConstants::HIGHWAY_POS_START, posEnd);
    auto edgeAtFadeEnd = PositionMath::getFretboardEdge(isDrums, fadeEnd, w, h, wNear, wMid, wFar,
        PositionConstants::HIGHWAY_POS_START, posEnd);

    int fadeStartRow = juce::jlimit(0, h - 1, (int)edgeAtFadeStart.centerY);
    int fadeEndRow   = juce::jlimit(0, h - 1, (int)edgeAtFadeEnd.centerY);

    if (fadeEndRow > fadeStartRow) std::swap(fadeEndRow, fadeStartRow);

    juce::Image::BitmapData pixels(fadedImage, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < h; ++y)
    {
        float alpha;
        if (y >= fadeStartRow)
        {
            alpha = 1.0f;
        }
        else if (y <= fadeEndRow)
        {
            alpha = 0.0f;
        }
        else
        {
            float t = (float)(fadeStartRow - y) / (float)(fadeStartRow - fadeEndRow);
            alpha = 1.0f - std::pow(t, fadeCurve);
        }

        if (alpha >= 1.0f) continue;

        auto* row = pixels.getLinePointer(y);
        for (int x = 0; x < w; ++x)
        {
            auto* pixel = row + x * pixels.pixelStride;
            pixel[0] = (uint8_t)(pixel[0] * alpha);
            pixel[1] = (uint8_t)(pixel[1] * alpha);
            pixel[2] = (uint8_t)(pixel[2] * alpha);
            pixel[3] = (uint8_t)(pixel[3] * alpha);
        }
    }

    return fadedImage;
}
