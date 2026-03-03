/*
    ==============================================================================

        HighwayTextureRenderer.h
        Author:  Noah Baxter

        Highway texture overlay rendering, extracted from HighwayRenderer.
        Handles scrolling texture mapping and per-strip fade for images.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"

class HighwayTextureRenderer
{
public:
    struct Params {
        int width, height;
        bool isDrums;
        float wNear, wMid, wFar;
        float highwayPosEnd, farFadeEnd, farFadeLen, farFadeCurve;
        float highwayTextureOpacity;
    };

    HighwayTextureRenderer() = default;
    ~HighwayTextureRenderer() = default;

    void setTexture(const juce::Image& tex) { texture = tex; }
    void clearTexture() { texture = juce::Image(); }
    void setScrollOffset(double offset) { scrollOffset = offset; }
    bool hasTexture() const { return texture.isValid(); }

    void drawTexture(juce::Graphics& g, const Params& p);
    void drawImageWithFade(juce::Graphics& g, const juce::Image& img,
                           juce::Rectangle<float> destRect, const Params& p);

private:
    juce::Image texture;
    juce::Image offscreen;
    double scrollOffset = 0.0;

    static constexpr int MIN_STRIPS = 40;
    static constexpr float TILES_PER_HIGHWAY = 1.0f;
    static constexpr float HIGHWAY_POS_START = -0.3f;

    float calculateOpacity(float position, float fadeEnd, float fadeLen, float fadeCurve)
    {
        float fadeStart = fadeEnd - fadeLen;
        if (position <= fadeStart) return 1.0f;
        if (position >= fadeEnd) return 0.0f;
        float t = (position - fadeStart) / fadeLen;
        return 1.0f - std::pow(t, fadeCurve);
    }

    struct CachedGeometry {
        std::vector<std::pair<PositionConstants::LaneCorners, float>> edges;
        juce::Path fretboardPath;
        int stripCount = 0;
        int width = 0, height = 0;
        bool isDrums = false;
        float wNear = 0, wMid = 0, wFar = 0;
        float posEnd = 0, fadeEnd = 0;
    } cached;
};
