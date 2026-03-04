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

//==============================================================================
// Opacity & Visual Effects
//==============================================================================

// Gem and note opacity
constexpr float OPACITY_FADE_START = 0.9f;   // Position where gem opacity starts fading
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
constexpr float LANE_OPACITY = 0.4f;         // Lane sustain opacity

// Gridline opacity
constexpr float MEASURE_OPACITY = 1.0f;
constexpr float BEAT_OPACITY = 0.6f;
constexpr float HALF_BEAT_OPACITY = 0.4f;

// Hit animation rendering opacity
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity

//==============================================================================
// Deprecated (PNG fallback — remove when PNG gridlines are dropped)
//==============================================================================

constexpr float PNG_MEASURE_OPACITY = 1.0f;
constexpr float PNG_BEAT_OPACITY = 0.4f;
constexpr float PNG_HALF_BEAT_OPACITY = 0.3f;
