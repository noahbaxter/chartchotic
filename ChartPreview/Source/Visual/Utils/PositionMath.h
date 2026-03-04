/*
    ==============================================================================

        PositionMath.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains mathematical functions for computing glyph positions
        and lane coordinates using 3D perspective calculations.
        All coordinate data is defined in PositionConstants.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PositionConstants.h"

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

class PositionMath
{
public:
    PositionMath() = default;
    ~PositionMath() = default;

    //==============================================================================
    // Bezier positioning system
    static PositionConstants::LaneCorners getFretboardEdge(
        bool isDrums, float position, uint width, uint height,
        float wNear, float wMid, float wFar,
        float posStart, float posEnd);

    static PositionConstants::LaneCorners getColumnPosition(
        bool isDrums, float position, uint width, uint height,
        float wNear, float wMid, float wFar,
        float posStart, float posEnd,
        const PositionConstants::NormalizedCoordinates& colCoords,
        float sizeScale, float fretboardScale = 1.0f);

private:
    //==============================================================================
    // Core perspective calculation
    static juce::Rectangle<float> createPerspectiveGlyphRect(
        float position,
        float normY1, float normY2,
        float normX1, float normX2,
        float normWidth1, float normWidth2,
        bool isBarNote,
        uint width, uint height
    );

    //==============================================================================
    // Helper to apply scaling and centering to coordinates
    static PositionConstants::NormalizedCoordinates applyWidthScaling(
        const PositionConstants::NormalizedCoordinates& coords,
        float scaler
    );

};
