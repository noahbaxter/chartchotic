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
    // Guitar Lane Coordinates (for column rendering)
    static PositionConstants::LaneCorners getGuitarLaneCoordinates(uint gemColumn, float position, uint width, uint height);

    //==============================================================================
    // Drum Lane Coordinates (for column rendering)
    static PositionConstants::LaneCorners getDrumLaneCoordinates(uint gemColumn, float position, uint width, uint height);

    //==============================================================================
    // Fretboard Boundary
    // widthScaleNear/Mid/Far = width scales at bottom/middle/top of the fretboard range.
    // posStart/posEnd define the full fretboard range for bezier interpolation.
    static PositionConstants::LaneCorners getFretboardEdge(bool isDrums, float position,
                                                           uint width, uint height,
                                                           float widthScaleNear, float widthScaleMid,
                                                           float widthScaleFar,
                                                           float posStart, float posEnd);

    // Builds a closed Path tracing the fretboard boundary from posStart to posEnd.
    static juce::Path getFretboardPath(bool isDrums, float posStart, float posEnd,
                                       uint width, uint height,
                                       float widthScaleNear, float widthScaleMid,
                                       float widthScaleFar, int segments = 50);

    //==============================================================================
    // Public access for GlyphRenderer
    static PositionConstants::NormalizedCoordinates getGuitarOpenNoteCoords();
    static PositionConstants::NormalizedCoordinates getGuitarNoteCoords(uint gemColumn);
    static PositionConstants::NormalizedCoordinates getDrumKickCoords();
    static PositionConstants::NormalizedCoordinates getDrumPadCoords(uint gemColumn);

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

    //==============================================================================
    // Generic lane coordinate lookup
    static const PositionConstants::NormalizedCoordinates& lookupLaneCoords(
        const PositionConstants::NormalizedCoordinates* coordTable,
        uint gemColumn
    );
};
