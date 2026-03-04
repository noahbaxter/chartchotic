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
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
constexpr float LANE_OPACITY = 0.4f;         // Lane sustain opacity

// Gridline opacity by type
constexpr float MEASURE_OPACITY = 1.0f;      // Gridline opacity for measure lines
constexpr float BEAT_OPACITY = 0.4f;         // Gridline opacity for beat lines
constexpr float HALF_BEAT_OPACITY = 0.3f;    // Gridline opacity for half-beat lines

// Hit animation rendering opacity
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity

// Far-end fade (highway length)
constexpr float FAR_FADE_DEFAULT = 1.20f;     // Default farFadeEnd (normalized position)
constexpr float FAR_FADE_MIN     = 0.50f;     // Minimum highway length
constexpr float FAR_FADE_MAX     = 3.00f;     // Maximum highway length
constexpr float FAR_FADE_LEN     = 0.35f;     // Length of fade zone
constexpr float FAR_FADE_CURVE   = 1.0f;      // Fade exponent (1=linear)

inline float calculateFarFade(float position, float fadeEnd, float fadeLen, float fadeCurve)
{
    float fadeStart = fadeEnd - fadeLen;
    if (position <= fadeStart) return 1.0f;
    if (position >= fadeEnd)   return 0.0f;
    float t = (position - fadeStart) / fadeLen;
    return 1.0f - std::pow(t, fadeCurve);
}
