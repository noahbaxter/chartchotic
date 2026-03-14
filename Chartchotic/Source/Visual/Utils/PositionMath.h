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

    static bool bemaniMode;
    static float bemaniHwyScale;  // farFadeEnd, controls vertical spread in flat mode

#ifdef DEBUG
    static PositionConstants::PerspectiveParams debugPerspParamsGuitar;
    static PositionConstants::PerspectiveParams debugPerspParamsDrums;
    static bool debugPolyShade;

    static const PositionConstants::PerspectiveParams& perspParams(bool isDrums)
    {
        return isDrums ? debugPerspParamsDrums : debugPerspParamsGuitar;
    }
#endif

    //==============================================================================
    // Bezier positioning system
    static PositionConstants::LaneCorners getFretboardEdge(
        bool isDrums, float position, uint width, uint height,
        float posStart, float posEnd);

    static PositionConstants::LaneCorners getColumnPosition(
        bool isDrums, float position, uint width, uint height,
        float posStart, float posEnd,
        const PositionConstants::NormalizedCoordinates& colCoords,
        float sizeScale, float fretboardScale = 1.0f,
        int bemaniLaneIdx = -1);


private:
    //==============================================================================
    // Core perspective calculation
    static juce::Rectangle<float> createPerspectiveGlyphRect(
        const PositionConstants::PerspectiveParams& perspParams,
        float position,
        float normY1, float normY2,
        float normX1, float normX2,
        float normWidth1, float normWidth2,
        bool isBarNote,
        uint width, uint height
    );

};
