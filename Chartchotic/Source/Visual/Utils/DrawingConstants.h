/*
    ==============================================================================

        DrawingConstants.h
        Author: Noah Baxter

        Central location for drawing and rendering constants used throughout
        the visualization pipeline. Includes opacity values, visual effects,
        and rendering parameters.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <vector>
#include <functional>

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// Opacity & Visual Effects
//==============================================================================

// Gem and note opacity
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
constexpr float LANE_OPACITY = 0.4f;         // Lane sustain opacity

// Gridline opacity by type
constexpr float MEASURE_OPACITY = 1.0f;      // Gridline opacity for measure lines
constexpr float BEAT_OPACITY = 0.3f;         // Gridline opacity for beat lines
constexpr float HALF_BEAT_OPACITY = 0.15f;   // Gridline opacity for half-beat lines

// Highway texture
constexpr float TEXTURE_OPACITY_DEFAULT = 1.0f;  // Default highway texture opacity

// Hit animation rendering
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity
constexpr double HIT_ANIMATION_DURATION = 0.10;   // Hit flash total duration (seconds)
constexpr double KICK_ANIMATION_DURATION = 0.15;  // Kick/bar flash total duration (seconds)
constexpr int HIT_FLARE_MAX_FRAME = 2;            // Show flare for first N frames of hit animation

// Far-end fade (highway length)
constexpr float FAR_FADE_DEFAULT = 1.20f;     // Default user slider value (position space)
constexpr float FAR_FADE_MIN     = 0.50f;     // Minimum user slider value
constexpr float FAR_FADE_MAX     = 5.00f;     // Maximum user slider value
constexpr float FAR_FADE_LEN     = 0.35f;     // Length of fade zone
constexpr float FAR_FADE_CURVE   = 1.0f;      // Fade exponent (1=linear)

// Per-instrument highway scale: farFadeEnd = userLength * scale.
// Compensates for different perspective geometry so both instruments
// appear visually equivalent at the same slider setting.
constexpr float HWY_SCALE_GUITAR = 1.00f;
constexpr float HWY_SCALE_DRUMS  = 1.00f;

// Bemani mode tunables
constexpr float BEMANI_STRIKELINE_POS = 0.90f;   // Strikeline Y fraction (0=top, 1=bottom)
constexpr float BEMANI_NOTE_Y_OFFSET = -0.015f;  // Global note Y offset (baked, not debug-tunable)
constexpr float BEMANI_GEM_NUDGE = 0.05f;        // Gem Y nudge (fraction of gem height)
constexpr float BEMANI_BAR_NUDGE = -0.10f;       // Bar Y nudge (fraction of bar height, negative = up)
constexpr float BEMANI_BAR_FIT = 1.02f;          // Bar note width fit
constexpr float BEMANI_CURVATURE = 0.0f;         // Note curvature multiplier (0=flat)
constexpr float BEMANI_GEM_W = 0.95f;
constexpr float BEMANI_GEM_H = 0.90f;
constexpr float BEMANI_BAR_W = 1.02f;
constexpr float BEMANI_BAR_H = 0.50f;
constexpr float BEMANI_SUSTAIN_WIDTH = 0.20f;    // Sustain width (fraction of lane)
constexpr float BEMANI_BAR_SUSTAIN_WIDTH = 0.90f; // Bar sustain width (fraction of fretboard)
constexpr float BEMANI_SUSTAIN_CAP = 0.20f;      // Lane end-cap curve height (fraction of width)
constexpr float BEMANI_SUST_START_OFF = -0.020f; // Sustain start offset
constexpr float BEMANI_SUST_END_OFF = -0.025f;  // Sustain end offset
constexpr float BEMANI_LANE_START_OFF = -0.010f; // Lane start offset (negative = extends before note)
constexpr float BEMANI_LANE_END_OFF = -0.020f;   // Lane end offset
constexpr float BEMANI_LANE_FILL_W = 0.92f;      // Lane fill width (fraction of column width)
constexpr float BEMANI_BAR_LANE_FILL_W = 0.90f;  // Bar lane fill width (fraction of fretboard)
constexpr float BEMANI_LANE_CAP = 0.10f;         // Lane cap curve height (fraction of width)
constexpr float BEMANI_BAR_CAP = 0.03f;          // Bar lane/sustain cap (smaller for wide bars)
constexpr float BEMANI_LANE_DIV_W = 1.0f;        // Lane divider width multiplier
constexpr float BEMANI_BAR_LANE_W = 1.0f;        // Bar note lane width multiplier
constexpr float BEMANI_GRIDLINE_BOOST = 3.0f;
constexpr float BEMANI_LANE_OPACITY = 0.20f;
constexpr float BEMANI_STRIKELINE_OPACITY = 0.8f;

#ifdef DEBUG
inline float debugHwyScaleGuitar = HWY_SCALE_GUITAR;
inline float debugHwyScaleDrums  = HWY_SCALE_DRUMS;
inline float getHwyScale(bool isDrums) { return isDrums ? debugHwyScaleDrums : debugHwyScaleGuitar; }

inline float debugBemaniStrikelinePos = BEMANI_STRIKELINE_POS;
inline float debugBemaniGemNudge = BEMANI_GEM_NUDGE;
inline float debugBemaniBarNudge = BEMANI_BAR_NUDGE;
inline float debugBemaniTexSpeed = 1.005f;
inline float debugBemaniBarFit = BEMANI_BAR_FIT;
inline float debugBemaniCurvature = BEMANI_CURVATURE;
inline float debugBemaniGemW = BEMANI_GEM_W;
inline float debugBemaniGemH = BEMANI_GEM_H;
inline float debugBemaniBarW = BEMANI_BAR_W;
inline float debugBemaniBarH = BEMANI_BAR_H;
inline float debugBemaniSustainWidth = BEMANI_SUSTAIN_WIDTH;
inline float debugBemaniBarSustainWidth = BEMANI_BAR_SUSTAIN_WIDTH;
inline float debugBemaniSustainCap = BEMANI_SUSTAIN_CAP;
inline float debugBemaniSustStartOff = BEMANI_SUST_START_OFF;
inline float debugBemaniSustEndOff = BEMANI_SUST_END_OFF;
inline float debugBemaniLaneStartOff = BEMANI_LANE_START_OFF;
inline float debugBemaniLaneEndOff = BEMANI_LANE_END_OFF;
inline float debugBemaniLaneFillW = BEMANI_LANE_FILL_W;
inline float debugBemaniBarLaneFillW = BEMANI_BAR_LANE_FILL_W;
inline float debugBemaniLaneCap = BEMANI_LANE_CAP;
inline float debugBemaniBarCap = BEMANI_BAR_CAP;
inline float debugBemaniLaneDivW = BEMANI_LANE_DIV_W;
inline float debugBemaniBarLaneW = BEMANI_BAR_LANE_W;
inline float debugBemaniGridlineBoost = BEMANI_GRIDLINE_BOOST;
inline float debugBemaniLaneOpacity = BEMANI_LANE_OPACITY;
inline float debugBemaniStrikelineOpacity = BEMANI_STRIKELINE_OPACITY;
#else
inline float getHwyScale(bool isDrums) { return isDrums ? HWY_SCALE_DRUMS : HWY_SCALE_GUITAR; }
#endif


inline float calculateFarFade(float position, float fadeEnd, float fadeLen, float fadeCurve)
{
    float fadeStart = fadeEnd - fadeLen;
    if (position <= fadeStart) return 1.0f;
    if (position >= fadeEnd)   return 0.0f;
    float t = (position - fadeStart) / fadeLen;
    return 1.0f - std::pow(t, fadeCurve);
}

//==============================================================================
// Draw Order & Render Pipeline
//==============================================================================

enum class DrawOrder
{
    BACKGROUND,
    TRACK,
    GRID,
    LANE,
    TRACK_SIDEBARS,
    TRACK_LANE_LINES,
    TRACK_STRIKELINE,
    BAR,
    SUSTAIN,
    NOTE,
    TRACK_CONNECTORS,
    OVERLAY,
    BAR_ANIMATION,
    NOTE_ANIMATION,
    TEXT_EVENT,
};

static constexpr int DRAW_ORDER_COUNT = 15;

//==============================================================================
// Text Event Data
//==============================================================================

struct TimeBasedFlipRegion {
    double startTime;
    double endTime;
};
using TimeBasedFlipRegions = std::vector<TimeBasedFlipRegion>;

struct TimeBasedEventMarker {
    double time;
    juce::String label;
};
using TimeBasedEventMarkers = std::vector<TimeBasedEventMarker>;
static constexpr int MAX_DRAW_COLUMNS = 8;
using DrawCallBucket = std::vector<std::function<void(juce::Graphics&)>>;
using DrawCallGrid = std::array<std::array<DrawCallBucket, MAX_DRAW_COLUMNS>, DRAW_ORDER_COUNT>;
using DrawCallMap = DrawCallGrid;
