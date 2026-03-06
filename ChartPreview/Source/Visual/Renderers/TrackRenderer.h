/*
    ==============================================================================

        TrackRenderer.h
        Author:  Noah Baxter

        Renders the track as composited layers: dark fretboard fill, sidebars,
        lane lines, strikeline, and instrument-specific elements (kick smashers
        or strikeline connectors). Layers are individually positionable via
        debug tuning parameters.

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
    float tileStep = 0.80f;       // Vertical step between tiles (< 1.0 = overlap for baked fade)
    float tileScaleStep = 0.50f;   // Scale multiplier per tile (each tile = previous * this)

    // Per-layer positioning (debug-tunable)
    struct LayerTransform {
        float scale = 1.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
    };

    enum Layer { SIDEBARS = 0, LANE_LINES, STRIKELINE, CONNECTORS, NUM_LAYERS };
    LayerTransform layersGuitar[NUM_LAYERS] = {
        {0.740f, 0.0f,     0.0f},      // SIDEBARS
        {0.450f, 0.0f,     0.0f},      // LANE_LINES
        {0.615f, 0.0f, -0.1825f},      // STRIKELINE
        {0.650f, 0.0f, -0.1960f}       // CONNECTORS
    };
    LayerTransform layersDrums[NUM_LAYERS] = {
        {0.740f, 0.0f,     0.0f},      // SIDEBARS
        {0.350f, 0.0f,     0.0f},      // LANE_LINES
        {0.585f, 0.0f, -0.1815f},      // STRIKELINE
        {0.660f, 0.0f, -0.1600f}       // CONNECTORS
    };

    /** Get a pre-baked layer image (with far-fade applied). Valid after rebuild(). */
    const juce::Image& getLayerImage(Layer layer) const { return layerImages[layer]; }

private:
    juce::ValueTree& state;

    // Layer source images
    juce::Image sidebarsImage;
    juce::Image laneLinesGuitarImage;
    juce::Image laneLinesDrumsImage;
    juce::Image strikelineGuitarImage;
    juce::Image strikelineDrumsImage;
    juce::Image strikelineConnectorsImage;
    juce::Image kickSmashersImage;

    juce::Image fadedTrackImage;       // Dark fill + far-fade (base layer)
    juce::Image layerImages[NUM_LAYERS]; // Per-layer images with far-fade

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
    void compositeLayers(juce::Image& target, int w, int h, bool isDrums,
                         float wNear, float wMid, float wFar, float posEnd);
    void bakeLayerImage(juce::Image& out, const juce::Image& src, const LayerTransform& t,
                        int w, int h, bool isDrums, bool tiled,
                        float farFadeEnd, float farFadeLen, float farFadeCurve,
                        float wNear, float wMid, float wFar, float posEnd);

    static constexpr int PIXELS_PER_STRIP = 1;
    static constexpr int MIN_STRIPS = 40;
    static constexpr int MAX_STRIPS = 150;
    static constexpr int PREBAKE_QUALITY = 4;  // vertical oversampling for pre-baked tile
    static constexpr int MIN_MIP_WIDTH = 8;    // stop mip chain when width drops below this
    static constexpr float HIGHWAY_POS_START = PositionConstants::HIGHWAY_POS_START;
};
