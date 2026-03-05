/*
    ==============================================================================

        TrackRenderer.h
        Author:  Noah Baxter

        Renders the track background texture with far-end fade.
        Will be expanded with additional track visuals (e.g., overlays, effects).

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"
#include "TrackFade.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

class TrackRenderer
{
public:
    TrackRenderer(juce::ValueTree& state);

    void paint(juce::Graphics& g);

    /** Paint the scrolling highway texture overlay. Call between track and scene rendering. */
    void paintTexture(juce::Graphics& g, float scrollOffset);

    /** Rebuild the cached faded track image. Call on resize, instrument change, or highway length change. */
    void rebuild(int width, int height,
                 float farFadeEnd, float farFadeLen, float farFadeCurve,
                 float wNear, float wMid, float wFar, float posEnd);

    /** Set the source texture for highway overlay. */
    void setTexture(const juce::Image& texture);

    /** Clear the highway texture overlay. */
    void clearTexture();

    float textureScale = 1.0f;    // >1 = more tiles (squished), <1 = fewer (stretched)
    float textureOpacity = 0.45f; // Overlay opacity (0..1)

private:
    juce::ValueTree& state;

    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;
    juce::Image fadedTrackImage;

    // Highway texture overlay
    juce::Image sourceTexture;
    juce::Image offscreen;
    bool textureEnabled = false;

    // Cached geometry (used by both track fade and texture scanline LUT)
    struct CachedGeometry {
        std::vector<std::pair<PositionConstants::LaneCorners, float>> edges;
        int stripCount = 0;
        int width = 0, height = 0;
        bool isDrums = false;
        float wNear = 0, wMid = 0, wFar = 0;
        float posEnd = 0, fadeEnd = 0, fadeLen = 0, fadeCurve = 0;
    } cached;

    // Pre-baked scanline data for fast per-frame texture rendering
    struct ScanlineInfo {
        float texV;    // base texture V coordinate (1.0 - position, before scroll/scale)
        float alpha;   // fade alpha at this scanline
        int leftX;     // fretboard left pixel bound
        int rightX;    // fretboard right pixel bound
    };

    struct MipLevel { int rowOffset; int width; };

    struct PrebakedData {
        std::vector<ScanlineInfo> scanlines;
        juce::Image mipAtlas;           // all mip levels stacked vertically in one image
        std::vector<MipLevel> mips;     // per-level: row offset into atlas + width
        int yMin = 0, yMax = 0;
        int tileHeight = 0;            // rows per mip level (same for all levels)
        bool valid = false;
    } prebaked;

    void rebuildPrebake();

    static constexpr int PIXELS_PER_STRIP = 1;
    static constexpr int MIN_STRIPS = 40;
    static constexpr int MAX_STRIPS = 150;
    static constexpr int PREBAKE_QUALITY = 4;  // vertical oversampling for pre-baked tile
    static constexpr int MIN_MIP_WIDTH = 8;    // stop mip chain when width drops below this
    static constexpr float HIGHWAY_POS_START = PositionConstants::HIGHWAY_POS_START;
};
