/*
    ==============================================================================

        SceneRenderer.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

        Coordinator for chart element rendering. Owns sub-renderers and the
        shared AssetManager. Delegates note/sustain/gridline/animation rendering
        to focused single-responsibility classes.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Midi/Processing/MidiInterpreter.h"
#include "../../Utils/Utils.h"
#include "../../Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "AnimationRenderer.h"
#include "NoteRenderer.h"
#include "SustainRenderer.h"
#include "GridlineRenderer.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/RenderTiming.h"


class SceneRenderer
{
    public:
        SceneRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~SceneRenderer();

        void paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying = true);

        bool showNotes = true;
        bool showSustains = true;
        bool showLanes = true;
        bool showGridlines = true;

        bool collectPhaseTiming = false;
        PhaseTiming lastPhaseTiming;

        // Tunable fretboard boundary scales (mutable for debug UI, defaults from PositionConstants)
        float fretboardWidthScaleNearGuitar = PositionConstants::FB_WIDTH_NEAR_GUITAR;
        float fretboardWidthScaleMidGuitar  = PositionConstants::FB_WIDTH_MID_GUITAR;
        float fretboardWidthScaleFarGuitar  = PositionConstants::FB_WIDTH_FAR_GUITAR;
        float fretboardWidthScaleNearDrums  = PositionConstants::FB_WIDTH_NEAR_DRUMS;
        float fretboardWidthScaleMidDrums   = PositionConstants::FB_WIDTH_MID_DRUMS;
        float fretboardWidthScaleFarDrums   = PositionConstants::FB_WIDTH_FAR_DRUMS;
        float highwayPosEnd = PositionConstants::HIGHWAY_POS_END;

        // Gridline position nudge (normalized position space, exposed for debug UI)
        float gridlinePosOffset = PositionConstants::GRIDLINE_POS_OFFSET;

        // Far-end fade: farFadeEnd is user-controlled ("highway length")
        float farFadeEnd = FAR_FADE_DEFAULT;
        static constexpr float farFadeLen   = FAR_FADE_LEN;
        static constexpr float farFadeCurve = FAR_FADE_CURVE;

        // Mutable lane coord arrays (mutable for debug UI, defaults from PositionConstants)
        PositionConstants::NormalizedCoordinates guitarLaneCoordsLocal[6] = {
            PositionConstants::guitarBezierLaneCoords[0],
            PositionConstants::guitarBezierLaneCoords[1],
            PositionConstants::guitarBezierLaneCoords[2],
            PositionConstants::guitarBezierLaneCoords[3],
            PositionConstants::guitarBezierLaneCoords[4],
            PositionConstants::guitarBezierLaneCoords[5]
        };
        PositionConstants::NormalizedCoordinates drumLaneCoordsLocal[5] = {
            PositionConstants::drumBezierLaneCoords[0],
            PositionConstants::drumBezierLaneCoords[1],
            PositionConstants::drumBezierLaneCoords[2],
            PositionConstants::drumBezierLaneCoords[3],
            PositionConstants::drumBezierLaneCoords[4]
        };

    private:
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;
        AssetManager assetManager;
        NoteRenderer noteRenderer;
        SustainRenderer sustainRenderer;
        GridlineRenderer gridlineRenderer;
        AnimationRenderer animationRenderer;

        uint width = 0, height = 0;

        // Bezier positioning helper (kept for future callers)
        using LaneCorners = PositionConstants::LaneCorners;
        using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

        LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                                  float sizeScale, float fretboardScale = 1.0f)
        {
            bool isDrums = isPart(state, Part::DRUMS);
            float wNear = isDrums ? fretboardWidthScaleNearDrums : fretboardWidthScaleNearGuitar;
            float wMid  = isDrums ? fretboardWidthScaleMidDrums  : fretboardWidthScaleMidGuitar;
            float wFar  = isDrums ? fretboardWidthScaleFarDrums  : fretboardWidthScaleFarGuitar;
            return PositionMath::getColumnPosition(isDrums, position, width, height,
                                                   wNear, wMid, wFar,
                                                   PositionConstants::HIGHWAY_POS_START, highwayPosEnd,
                                                   colCoords, sizeScale, fretboardScale);
        }

        DrawCallMap drawCallMap;
};
