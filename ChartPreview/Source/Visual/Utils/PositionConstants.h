/*
    ==============================================================================

        PositionConstants.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains positioning constants and coordinate lookup tables.
        All mathematical calculations are in PositionMath.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

namespace PositionConstants
{
    // Lane coordinate system for sustain rendering
    struct LaneCorners
    {
        float leftX, rightX, centerY;
    };

    // Normalized coordinate data for positioning elements
    // x,y
    // 0,0 ----- 1,0
    // |          |
    // |          |
    // |          |
    // 0,1 ----- 1,1

    // x2, y2, w2
    // | | | | | |  FAR END
    // | | | | | |
    // | | | | | |
    // | | | | | |
    // | | | | | |
    // v v v v v v  STRIKELINE
    // x1, y1, w1
    
    struct NormalizedCoordinates
    {
        float normX1, normX2;
        float normY1, normY2;
        float normWidth1, normWidth2;
    };

    struct CoordinateOffset
    {
        float xOffset;      // X offset in pixels from glyph center
        float yOffset;      // Y offset in pixels from glyph center
        float widthScale;   // Width multiplier
        float heightScale;  // Height multiplier
    };

    // 3D perspective calculation parameters
    struct PerspectiveParams
    {
        float highwayDepth;
        float playerDistance;
        float perspectiveStrength;
        float exponentialCurve;
        float xOffsetMultiplier;
        float barNoteHeightRatio;
        float regularNoteHeightRatio;
    };

    //==============================================================================
    // Size & Scale Factors
    constexpr float GEM_SIZE = 0.9f;                    // Regular gem/note scaling factor
    constexpr float BAR_SIZE = 0.95f;                   // Bar note (kick/open) scaling factor
    constexpr float GRIDLINE_WIDTH_SCALE = 1.12f;       // Gridline width relative to fretboard
    constexpr float GRIDLINE_POS_OFFSET = -0.020f;      // Nudge gridlines forward in position space
    constexpr float BAR_NOTE_POS_OFFSET = -0.020f;      // Nudge bar notes forward in position space
    constexpr float SUSTAIN_WIDTH = 0.15f;              // Sustain width multiplier
    constexpr float SUSTAIN_OPEN_WIDTH = 0.7f;          // Open sustain width multiplier (narrower)
    constexpr float LANE_WIDTH = 1.1f;                  // Lane width multiplier
    constexpr float LANE_OPEN_WIDTH = 0.9f;             // Open lane width multiplier

    //==============================================================================
    // Sustain Geometry Constants
    constexpr float SUSTAIN_WIDTH_MULTIPLIER = 0.8f;    // Sustain slightly narrower than gems
    constexpr float SUSTAIN_CAP_RADIUS_SCALE = 0.25f;   // Scale for rounded sustain caps
    constexpr float SUSTAIN_MARGIN_SCALE = 0.1f;        // Small margin above/below center

    //==============================================================================
    // Curved Sustain & Lane Constants
    // Curve values: positive = convex bow outward, negative = concave bow inward
    constexpr float SUSTAIN_START_CURVE = 0.015f;       // Near edge (strikeline side)
    constexpr float SUSTAIN_END_CURVE = -0.010f;        // Far edge
    constexpr float BAR_SUSTAIN_START_CURVE = -0.015f;  // Bar (open/kick) near edge
    constexpr float BAR_SUSTAIN_END_CURVE = -0.015f;    // Bar (open/kick) far edge

    constexpr float SUSTAIN_START_OFFSET = 0.0f;        // Position nudge for sustain start
    constexpr float SUSTAIN_END_OFFSET = -0.05f;        // Sustain ends slightly before note
    constexpr float BAR_SUSTAIN_START_OFFSET = 0.0f;
    constexpr float BAR_SUSTAIN_END_OFFSET = -0.05f;

    constexpr float SUSTAIN_CLIP = -0.015f;             // How far past strikeline before clamping
    constexpr float BAR_SUSTAIN_CLIP = -0.015f;

    // Lane curve constants (fretboard-wide parabolic arcs)
    constexpr float LANE_START_CURVE = -0.025f;         // Fretboard-wide parabola for start edge
    constexpr float LANE_END_CURVE = -0.035f;           // Fretboard-wide parabola for end edge
    constexpr float LANE_INNER_START_CURVE = 0.040f;    // Lane-local arc at start
    constexpr float LANE_INNER_END_CURVE = -0.040f;     // Lane-local arc at end
    constexpr float LANE_SIDE_CURVE = 0.0f;             // Side edge curvature (disabled)
    constexpr float LANE_START_OFFSET = -0.010f;        // Lane position nudge
    constexpr float LANE_END_OFFSET = -0.010f;

    //==============================================================================
    // Special Visual Scales
    constexpr float DRUM_ACCENT_OVERLAY_SCALE = 1.1232876712f;  // Drum accent overlay base scale

    //==============================================================================
    // Lane Counts
    constexpr size_t GUITAR_LANE_COUNT = 6;             // Open + 5 frets
    constexpr size_t DRUM_LANE_COUNT = 5;               // Kick + 4 pads

    //==============================================================================
    // Coordinate lookup tables (column 0 is always open/kick, columns 1-5 are pads)
    // { normX1, normX2, normY1, normY2, normWidth1, normWidth2 }

    constexpr NormalizedCoordinates guitarGlyphCoords[] = {
        {0.16f, 0.34f, 0.745f, 0.234f, 0.68f, 0.32f},     // Open note
        {0.2035f, 0.354f, 0.73f, 0.22f, 0.125f, 0.065f},   // Col 1 - Green
        {0.3205f, 0.4115f, 0.73f, 0.22f, 0.125f, 0.065f},  // Col 2 - Red
        {0.4375f, 0.4675f, 0.73f, 0.22f, 0.125f, 0.065f},  // Col 3 - Yellow
        {0.5545f, 0.5235f, 0.73f, 0.22f, 0.125f, 0.065f},  // Col 4 - Blue
        {0.6715f, 0.581f, 0.73f, 0.22f, 0.125f, 0.065f}    // Col 5 - Orange
    };

    constexpr NormalizedCoordinates drumGlyphCoords[] = {
        {0.16f, 0.34f, 0.75f, 0.239f, 0.68f, 0.32f},      // Kick
        {0.2165f, 0.3648f, 0.72f, 0.22f, 0.147f, 0.0714f}, // Col 1 - Red
        {0.358f, 0.4318f, 0.72f, 0.22f, 0.147f, 0.0714f},  // Col 2 - Yellow
        {0.495f, 0.4968f, 0.72f, 0.22f, 0.147f, 0.0714f},  // Col 3 - Blue
        {0.6365f, 0.5638f, 0.72f, 0.22f, 0.147f, 0.0714f}  // Col 4 - Green
    };

    //==============================================================================
    // Animation Positioning & Scaling Factors

    constexpr CoordinateOffset GUITAR_ANIMATION_OFFSETS[] = {
        {0.0f, 0.0f, 1.4f, 8.0f},   // Bar 0 - Open note
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 1 - Green
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 2 - Red
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 3 - Yellow
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 4 - Blue
        {0.0f, 0.0f, 1.6f, 3.5f}    // Col 5 - Orange
    };

    constexpr CoordinateOffset DRUM_ANIMATION_OFFSETS[] = {
        {0.0f, -8.0f, 1.4f, 10.0f}, // Bar 0 - Kick/2x Kick
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 1 - Red
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 2 - Yellow
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 3 - Blue
        {0.0f, 0.0f, 1.6f, 3.5f}    // Col 4 - Green
    };

    //==============================================================================
    // Fretboard Boundary Coordinates (for bezier positioning system)
    constexpr NormalizedCoordinates guitarFretboardCoords =
        {0.16f, 0.34f, 0.73f, 0.234f, 0.68f, 0.32f};
    constexpr NormalizedCoordinates drumFretboardCoords =
        {0.16f, 0.34f, 0.735f, 0.239f, 0.68f, 0.32f};
    constexpr float FRETBOARD_SCALE = 1.25f;

    //==============================================================================
    // Highway Range & Fretboard Width Scales (bezier system defaults)
    constexpr float HIGHWAY_POS_START = -0.3f;
    constexpr float HIGHWAY_POS_END = 1.12f;

    constexpr float FB_WIDTH_NEAR_GUITAR = 0.785f;
    constexpr float FB_WIDTH_MID_GUITAR  = 0.820f;
    constexpr float FB_WIDTH_FAR_GUITAR  = 0.855f;
    constexpr float FB_WIDTH_NEAR_DRUMS  = 0.800f;
    constexpr float FB_WIDTH_MID_DRUMS   = 0.820f;
    constexpr float FB_WIDTH_FAR_DRUMS   = 0.840f;

    //==============================================================================
    // Bezier Lane Coordinate Tables (sustain/lane rendering, slightly different from glyph tables)
    constexpr NormalizedCoordinates guitarBezierLaneCoords[] = {
        {0.182f, 0.358f, 0.73f, 0.234f, 0.632f, 0.280f},     // Open
        {0.226f, 0.378f, 0.71f, 0.22f, 0.095f, 0.040f},      // Green
        {0.324f, 0.418f, 0.71f, 0.22f, 0.116f, 0.054f},      // Red
        {0.442f, 0.472f, 0.71f, 0.22f, 0.118f, 0.054f},      // Yellow
        {0.558f, 0.526f, 0.71f, 0.22f, 0.118f, 0.056f},      // Blue
        {0.678f, 0.580f, 0.71f, 0.22f, 0.096f, 0.040f}       // Orange
    };
    constexpr NormalizedCoordinates drumBezierLaneCoords[] = {
        {0.190f, 0.354f, 0.735f, 0.239f, 0.620f, 0.290f},    // Kick (1x and 2x)
        {0.232f, 0.376f, 0.70f, 0.22f, 0.132f, 0.060f},      // Red
        {0.366f, 0.436f, 0.70f, 0.22f, 0.134f, 0.064f},      // Yellow
        {0.500f, 0.500f, 0.70f, 0.22f, 0.134f, 0.066f},      // Blue
        {0.636f, 0.564f, 0.70f, 0.22f, 0.132f, 0.060f}       // Green
    };

    //==============================================================================
    // Curved Note Rendering (pre-baked image cache)
    constexpr float NOTE_CURVATURE = -0.02f;       // Arc height as fraction of fretboard width
    constexpr float BAR_CURVATURE = 0.0f;          // Bars stay flat (span full fretboard)
    constexpr int NOTE_CACHE_DOWNSAMPLE = 2;       // Source resolution divisor (2 = 1/2 res)

    //==============================================================================
    // Per-instrument Z offsets (reference pixels at 720px height, positive = down)
    // Scaled by (height / REFERENCE_HEIGHT) at render time for resolution independence.
    constexpr float REFERENCE_HEIGHT = 720.0f;

    constexpr float GRID_Z_GUITAR = 0.0f;
    constexpr float GEM_Z_GUITAR = 9.0f;
    constexpr float BAR_Z_GUITAR = 0.0f;
    constexpr float HIT_GEM_Z_GUITAR = 14.0f;
    constexpr float HIT_BAR_Z_GUITAR = 9.0f;

    constexpr float GRID_Z_DRUMS = 0.0f;
    constexpr float GEM_Z_DRUMS = 4.0f;
    constexpr float BAR_Z_DRUMS = 0.0f;
    constexpr float HIT_GEM_Z_DRUMS = 6.0f;
    constexpr float HIT_BAR_Z_DRUMS = 10.0f;

    // Per-column X offsets (pixels at strikeline, perspective-scaled)
    constexpr float GUITAR_X_OFFSETS[6]    = {0.0f, 1.0f, -2.5f, 0.0f, 2.5f, -2.5f};
    constexpr float GUITAR_X_OFFSETS_2[6] = {0.0f, 2.5f,  0.0f, 0.0f, 0.0f, -1.0f};
    constexpr float DRUM_X_OFFSETS[5]     = {0.0f, 2.5f, -0.5f, 0.5f, -2.5f};
    constexpr float DRUM_X_OFFSETS_2[5]   = {0.0f, 1.0f, -0.5f, 0.5f, -1.0f};

    //==============================================================================
    // Gem dynamic scales (per gem type, applied in NoteRenderer)
    constexpr float GEM_GHOST_SCALE = 1.0f;             // Drum ghost gems
    constexpr float GEM_ACCENT_SCALE = 1.0f;            // Drum accent gems
    constexpr float GEM_HOPO_SCALE = 1.05f;             // Guitar HOPO gems
    constexpr float GEM_TAP_SCALE = 0.90f;              // Guitar tap gems (overlay is wider than HOPO art)
    constexpr float GEM_SP_SCALE = 1.0f;                // Star power gems

    //==============================================================================
    // Hit animation dynamic scales (per gem type, applied in AnimationRenderer)
    constexpr float HIT_GHOST_SCALE = 0.7f;             // Drum ghost hit
    constexpr float HIT_ACCENT_SCALE = 1.2f;            // Drum accent hit
    constexpr float HIT_HOPO_SCALE = 0.9f;              // Guitar HOPO hit
    constexpr float HIT_TAP_SCALE = 0.9f;               // Guitar tap hit
    constexpr float HIT_SP_SCALE = 1.0f;                // Star power hit

    //==============================================================================
    // Gem width/height scales
    constexpr float GEM_WIDTH_SCALE = 1.0f;
    constexpr float GEM_HEIGHT_SCALE = 1.15f;
    constexpr float BAR_WIDTH_SCALE = 1.0f;
    constexpr float BAR_HEIGHT_SCALE = 1.0f;

    //==============================================================================
    // Hit animation scales
    constexpr float HIT_GEM_SCALE = 1.5f;
    constexpr float HIT_BAR_SCALE = 1.0f;
    constexpr float HIT_GEM_WIDTH_SCALE = 1.0f;
    constexpr float HIT_GEM_HEIGHT_SCALE = 1.20f;
    constexpr float HIT_BAR_WIDTH_SCALE = 1.0f;
    constexpr float HIT_BAR_HEIGHT_SCALE = 1.0f;

    //==============================================================================
    // Strike position offsets (normalized, shifts clip/trigger point)
    constexpr float STRIKE_POS_GEM_GUITAR = 0.0f;
    constexpr float STRIKE_POS_BAR_GUITAR = 0.0f;
    constexpr float STRIKE_POS_GEM_DRUMS = -0.020f;
    constexpr float STRIKE_POS_BAR_DRUMS = -0.020f;

    //==============================================================================
    // Perspective Parameters (compile-time constant)
    constexpr inline PerspectiveParams getPerspectiveParams()
    {
        return {
            100.0f,   // highwayDepth
            50.0f,    // playerDistance
            0.7f,     // perspectiveStrength
            0.5f,     // exponentialCurve
            0.5f,     // xOffsetMultiplier
            16.0f,    // barNoteHeightRatio
            2.0f      // regularNoteHeightRatio
        };
    }

}
