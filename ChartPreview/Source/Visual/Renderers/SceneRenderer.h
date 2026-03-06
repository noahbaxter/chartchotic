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

        bool showGems = true;
        bool showBars = true;
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

        // Note curvature and scaling (runtime-adjustable for debug UI)
        float noteCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
        float noteCurvatureDrums = PositionConstants::NOTE_CURVATURE;
        float gemWidthScale = PositionConstants::GEM_WIDTH_SCALE;
        float gemHeightScale = PositionConstants::GEM_HEIGHT_SCALE;
        float barWidthScale = PositionConstants::BAR_WIDTH_SCALE;
        float barHeightScale = PositionConstants::BAR_HEIGHT_SCALE;
        float hitGemScale = PositionConstants::HIT_GEM_SCALE;
        float hitBarScale = PositionConstants::HIT_BAR_SCALE;
        float hitGemWidthScale = PositionConstants::HIT_GEM_WIDTH_SCALE;
        float hitGemHeightScale = PositionConstants::HIT_GEM_HEIGHT_SCALE;
        float hitBarWidthScale = PositionConstants::HIT_BAR_WIDTH_SCALE;
        float hitBarHeightScale = PositionConstants::HIT_BAR_HEIGHT_SCALE;
        // Hit animation dynamic scales
        float hitGhostScale = PositionConstants::HIT_GHOST_SCALE;
        float hitAccentScale = PositionConstants::HIT_ACCENT_SCALE;
        float hitHopoScale = PositionConstants::HIT_HOPO_SCALE;
        float hitTapScale = PositionConstants::HIT_TAP_SCALE;
        float hitSpScale = PositionConstants::HIT_SP_SCALE;
        bool spWhiteFlare = SP_WHITE_FLARE_DEFAULT;
        bool tapPurpleFlare = TAP_PURPLE_FLARE_DEFAULT;

        // Gem dynamic scales
        float gemGhostScale = PositionConstants::GEM_GHOST_SCALE;
        float gemAccentScale = PositionConstants::GEM_ACCENT_SCALE;
        float gemHopoScale = PositionConstants::GEM_HOPO_SCALE;
        float gemTapScale = PositionConstants::GEM_TAP_SCALE;
        float gemSpScale = PositionConstants::GEM_SP_SCALE;

        // Per-instrument Z offsets (guitar)
        float gridZOffsetGuitar = PositionConstants::GRID_Z_GUITAR;
        float gemZOffsetGuitar = PositionConstants::GEM_Z_GUITAR;
        float barZOffsetGuitar = PositionConstants::BAR_Z_GUITAR;
        float hitGemZOffsetGuitar = PositionConstants::HIT_GEM_Z_GUITAR;
        float hitBarZOffsetGuitar = PositionConstants::HIT_BAR_Z_GUITAR;

        // Per-instrument Z offsets (drums)
        float gridZOffsetDrums = PositionConstants::GRID_Z_DRUMS;
        float gemZOffsetDrums = PositionConstants::GEM_Z_DRUMS;
        float barZOffsetDrums = PositionConstants::BAR_Z_DRUMS;
        float hitGemZOffsetDrums = PositionConstants::HIT_GEM_Z_DRUMS;
        float hitBarZOffsetDrums = PositionConstants::HIT_BAR_Z_DRUMS;

        // Per-column Z offsets (drums only)
        float drumColZOffsets[5] = {};

        // Per-column X offsets (near=strikeline, far=top of highway)
        float guitarGemXOffsets[6] = {
            PositionConstants::GUITAR_X_OFFSETS[0], PositionConstants::GUITAR_X_OFFSETS[1],
            PositionConstants::GUITAR_X_OFFSETS[2], PositionConstants::GUITAR_X_OFFSETS[3],
            PositionConstants::GUITAR_X_OFFSETS[4], PositionConstants::GUITAR_X_OFFSETS[5]};
        float guitarGemXOffsets2[6] = {
            PositionConstants::GUITAR_X_OFFSETS_2[0], PositionConstants::GUITAR_X_OFFSETS_2[1],
            PositionConstants::GUITAR_X_OFFSETS_2[2], PositionConstants::GUITAR_X_OFFSETS_2[3],
            PositionConstants::GUITAR_X_OFFSETS_2[4], PositionConstants::GUITAR_X_OFFSETS_2[5]};
        float drumGemXOffsets[5] = {
            PositionConstants::DRUM_X_OFFSETS[0], PositionConstants::DRUM_X_OFFSETS[1],
            PositionConstants::DRUM_X_OFFSETS[2], PositionConstants::DRUM_X_OFFSETS[3],
            PositionConstants::DRUM_X_OFFSETS[4]};
        float drumGemXOffsets2[5] = {
            PositionConstants::DRUM_X_OFFSETS_2[0], PositionConstants::DRUM_X_OFFSETS_2[1],
            PositionConstants::DRUM_X_OFFSETS_2[2], PositionConstants::DRUM_X_OFFSETS_2[3],
            PositionConstants::DRUM_X_OFFSETS_2[4]};

        // Strike position offset (normalized, shifts clip/trigger/render point)
        float strikePosGemGuitar = PositionConstants::STRIKE_POS_GEM_GUITAR;
        float strikePosBarGuitar = PositionConstants::STRIKE_POS_BAR_GUITAR;
        float strikePosGemDrums = PositionConstants::STRIKE_POS_GEM_DRUMS;
        float strikePosBarDrums = PositionConstants::STRIKE_POS_BAR_DRUMS;

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
        double lastFrameTimeSeconds = 0.0;

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

    public:
        /** Register an image overlay to be drawn at a specific DrawOrder position.
            The image is drawn at (0,0) with full opacity. Call before paint(). */
        void setOverlay(DrawOrder order, const juce::Image* img) { overlays[order] = img; }
        void clearOverlays() { overlays.clear(); }

    private:
        std::map<DrawOrder, const juce::Image*> overlays;
};
