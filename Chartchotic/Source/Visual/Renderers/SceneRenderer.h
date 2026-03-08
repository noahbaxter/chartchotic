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

        void paint(juce::Graphics &g, int viewportWidth, int viewportHeight, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying = true);

        // Pre-scale assets for the current viewport size. Call on window resize.
        void rescaleAssets(int viewportWidth)
        {
            assetManager.rescaleForWidth(viewportWidth);
            noteRenderer.clearCurvedCache();
        }

        bool showGems = true;
        bool showBars = true;
        bool showSustains = true;
        bool showLanes = true;
        bool showGridlines = true;
        bool showTrack = true;
        bool showLaneSeparators = true;
        bool showStrikeline = true;

        bool collectPhaseTiming = false;
        PhaseTiming lastPhaseTiming;

        // Tunable fretboard boundary scales (mutable for debug UI, defaults from PositionConstants)
        PositionConstants::FretboardWidths fbWidthsGuitar = PositionConstants::FB_WIDTHS_GUITAR;
        PositionConstants::FretboardWidths fbWidthsDrums  = PositionConstants::FB_WIDTHS_DRUMS;
        float highwayPosEnd = PositionConstants::HIGHWAY_POS_END;

        // Note curvature and scaling (runtime-adjustable for debug UI)
        float noteCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
        float noteCurvatureDrums = PositionConstants::NOTE_CURVATURE;
        PositionConstants::ElementScale gemScale = PositionConstants::GEM_SCALE;
        PositionConstants::ElementScale barScale = PositionConstants::BAR_SCALE;
        float depthForeshorten = PositionConstants::NOTE_DEPTH_FORESHORTEN;
        PositionConstants::HitScale hitGemScale = PositionConstants::HIT_GEM_SCALE;
        PositionConstants::HitScale hitBarScale = PositionConstants::HIT_BAR_SCALE;
        PositionConstants::HitTypeConfig hitTypeConfig;
        PositionConstants::GemTypeScales gemTypeScales;

        // Overlay adjustments
        PositionConstants::OverlayAdjust overlayAdjusts[PositionConstants::NUM_OVERLAY_TYPES];

        // Per-instrument offsets (Z positions + strike positions)
        PositionConstants::InstrumentOffsets guitarOffsets = PositionConstants::GUITAR_OFFSETS;
        PositionConstants::InstrumentOffsets drumOffsets = PositionConstants::DRUM_OFFSETS;

        // Per-column adjustments (X near/far + Z offsets)
        PositionConstants::ColumnAdjust guitarColAdjust[6] = {
            PositionConstants::GUITAR_COL_ADJUST[0], PositionConstants::GUITAR_COL_ADJUST[1],
            PositionConstants::GUITAR_COL_ADJUST[2], PositionConstants::GUITAR_COL_ADJUST[3],
            PositionConstants::GUITAR_COL_ADJUST[4], PositionConstants::GUITAR_COL_ADJUST[5]};
        PositionConstants::ColumnAdjust drumColAdjust[5] = {
            PositionConstants::DRUM_COL_ADJUST[0], PositionConstants::DRUM_COL_ADJUST[1],
            PositionConstants::DRUM_COL_ADJUST[2], PositionConstants::DRUM_COL_ADJUST[3],
            PositionConstants::DRUM_COL_ADJUST[4]};

        // Gridline position nudge (normalized position space, exposed for debug UI)
        float gridlinePosOffset = PositionConstants::GRIDLINE_POS_OFFSET;

        // Lane shape config (tuneable)
        PositionConstants::LaneShapeConfig laneShape;


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
        double lastFrameTimeSeconds = 0.0;

        // Bezier positioning helper (kept for future callers)
        using LaneCorners = PositionConstants::LaneCorners;
        using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

        LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                                  float sizeScale, float fretboardScale = 1.0f)
        {
            bool isDrums = isPart(state, Part::DRUMS);
            const auto& fbw = isDrums ? fbWidthsDrums : fbWidthsGuitar;
            float wNear = fbw.near, wMid = fbw.mid, wFar = fbw.far;
            return PositionMath::getColumnPosition(isDrums, position, width, height,
                                                   wNear, wMid, wFar,
                                                   PositionConstants::HIGHWAY_POS_START, highwayPosEnd,
                                                   colCoords, sizeScale, fretboardScale);
        }

        DrawCallMap drawCallMap;

    public:
        /** Register an image overlay to be drawn at a specific DrawOrder position.
            The image is drawn at (0,0) with full opacity. Call before paint(). */
        void setOverlay(DrawOrder order, const juce::Image* img) { overlays[order] = img; }
        void clearOverlays() { overlays.clear(); }

    private:
        std::map<DrawOrder, const juce::Image*> overlays;
};
