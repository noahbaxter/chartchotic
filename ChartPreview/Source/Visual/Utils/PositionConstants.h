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
    constexpr float LANE_CLIP = -0.3f;                  // Lanes extend well past strikeline

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
        {0.16f, 0.34f, 0.745f, 0.234f, 0.68f, 0.32f},   // Open note
        {0.205f, 0.353f, 0.73f, 0.22f, 0.125f, 0.065f}, // Col 1 - Green
        {0.320f, 0.412f, 0.73f, 0.22f, 0.125f, 0.065f}, // Col 2 - Red
        {0.440f, 0.465f, 0.73f, 0.22f, 0.125f, 0.065f}, // Col 3 - Yellow
        {0.557f, 0.524f, 0.73f, 0.22f, 0.125f, 0.065f}, // Col 4 - Blue
        {0.673f, 0.580f, 0.73f, 0.22f, 0.125f, 0.065f}  // Col 5 - Orange
    };

    constexpr NormalizedCoordinates drumGlyphCoords[] = {
        {0.16f, 0.34f, 0.75f, 0.239f, 0.68f, 0.32f},     // Kick
        {0.22f, 0.365f, 0.72f, 0.22f, 0.147f, 0.0714f},  // Col 1 - Red
        {0.360f, 0.430f, 0.72f, 0.22f, 0.147f, 0.0714f}, // Col 2 - Yellow
        {0.497f, 0.495f, 0.72f, 0.22f, 0.147f, 0.0714f}, // Col 3 - Blue
        {0.640f, 0.564f, 0.72f, 0.22f, 0.147f, 0.0714f}  // Col 4 - Green
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
        {0.179f, 0.34f, 0.73f, 0.234f, 0.639f, 0.32f},   // Open
        {0.228f, 0.363f, 0.71f, 0.22f, 0.099f, 0.055f},   // Green
        {0.330f, 0.412f, 0.71f, 0.22f, 0.112f, 0.065f},   // Red
        {0.445f, 0.465f, 0.71f, 0.22f, 0.111f, 0.065f},   // Yellow
        {0.558f, 0.524f, 0.71f, 0.22f, 0.112f, 0.065f},   // Blue
        {0.674f, 0.580f, 0.71f, 0.22f, 0.100f, 0.065f}    // Orange
    };
    constexpr NormalizedCoordinates drumBezierLaneCoords[] = {
        {0.182f, 0.34f, 0.735f, 0.239f, 0.636f, 0.32f},   // Kick
        {0.228f, 0.37f, 0.70f, 0.22f, 0.136f, 0.0714f},   // Red
        {0.365f, 0.430f, 0.70f, 0.22f, 0.134f, 0.0714f},  // Yellow
        {0.501f, 0.495f, 0.70f, 0.22f, 0.134f, 0.0714f},  // Blue
        {0.636f, 0.564f, 0.70f, 0.22f, 0.137f, 0.0714f}   // Green
    };

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
