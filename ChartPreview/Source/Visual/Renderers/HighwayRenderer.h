/*
    ==============================================================================

        HighwayRenderer.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Midi/Processing/MidiInterpreter.h"
#include "../../Utils/Utils.h"
#include "../../Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "AnimationRenderer.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "GlyphRenderer.h"
#include "ColumnRenderer.h"
#include "HighwayTextureRenderer.h"

#include <chrono>
#include <map>

struct PhaseTiming
{
    double texture_us = 0;
    double build_notes_us = 0;
    double build_sustains_us = 0;
    double build_gridlines_us = 0;
    double animation_detect_us = 0;
    double execute_draws_us = 0;
    double after_lanes_us = 0;
    double animation_advance_us = 0;
    std::map<DrawOrder, double> layer_us;  // per-DrawOrder execution breakdown
    double total_us = 0;
};

struct ScopedPhaseMeasure
{
    using Clock = std::chrono::high_resolution_clock;
    double& target;
    bool active;
    Clock::time_point start;
    ScopedPhaseMeasure(double& t, bool collect)
        : target(t), active(collect), start(collect ? Clock::now() : Clock::time_point{}) {}
    ~ScopedPhaseMeasure() {
        if (active) target = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    }
};

class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

        void paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying = true);

        void setHighwayTexture(const juce::Image& texture) { highwayTextureRenderer.setTexture(texture); }
        void clearHighwayTexture() { highwayTextureRenderer.clearTexture(); }
        void setScrollOffset(double offset) { highwayTextureRenderer.setScrollOffset(offset); }
        void paintHighwayTextureOnly(juce::Graphics& g) { if (highwayTextureRenderer.hasTexture()) highwayTextureRenderer.drawTexture(g, buildTextureParams()); }
        bool skipHighwayTexture = false;
        float highwayTextureOpacity = 0.7f;

        // Opt-in per-phase profiling (zero overhead when false)
        bool collectPhaseTiming = false;
        PhaseTiming lastPhaseTiming;

        // Called after lanes+gridlines are drawn but before notes/sustains.
        // Used by SVG track mode to insert the track overlay behind notes.
        std::function<void(juce::Graphics&)> onAfterLanes;

        // Draw an image at destRect with per-strip highway fade applied.
        // Uses the same position-based opacity as notes/gridlines/highway texture.
        void drawImageWithFade(juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> destRect)
        {
            highwayTextureRenderer.drawImageWithFade(g, img, destRect, buildTextureParams());
        }

    private:
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;
        AssetManager assetManager;
        AnimationRenderer animationRenderer;
        GlyphRenderer glyphRenderer;
        ColumnRenderer columnRenderer;
        HighwayTextureRenderer highwayTextureRenderer;

        HighwayTextureRenderer::Params buildTextureParams() const
        {
            bool isDrums = isPart(state, Part::DRUMS);
            return {
                (int)width, (int)height, isDrums,
                isDrums ? fretboardWidthScaleNearDrums : fretboardWidthScaleNearGuitar,
                isDrums ? fretboardWidthScaleMidDrums  : fretboardWidthScaleMidGuitar,
                isDrums ? fretboardWidthScaleFarDrums  : fretboardWidthScaleFarGuitar,
                highwayPosEnd, farFadeEnd, farFadeLen, farFadeCurve,
                highwayTextureOpacity
            };
        }

        uint width = 0, height = 0;

        bool isBarNote(uint gemColumn, Part part)
        {
            if (part == Part::GUITAR)
            {
                return gemColumn == 0;
            }
            else // if (part == Part::DRUMS)
            {
                return gemColumn == 0 || gemColumn == 6;
            }
        }

        // Per-element opacity fade based on far-end position
        float calculateOpacity(float position)
        {
            float fadeStart = farFadeEnd - farFadeLen;
            if (position <= fadeStart) return 1.0f;
            if (position >= farFadeEnd) return 0.0f;
            float t = (position - fadeStart) / farFadeLen;
            return 1.0f - std::pow(t, farFadeCurve);
        }

        juce::Image curvedOffscreen;

    public:
        static constexpr float HIGHWAY_POS_START = -0.3f;
        // Tunable fretboard boundary scales (exposed for debug UI)
        // Near = strikeline (bottom), Mid = midpoint, Far = far end (top)
        float fretboardWidthScaleNearGuitar = 0.785f;
        float fretboardWidthScaleMidGuitar  = 0.820f;
        float fretboardWidthScaleFarGuitar  = 0.855f;
        float fretboardWidthScaleNearDrums  = 0.800f;
        float fretboardWidthScaleMidDrums   = 0.820f;
        float fretboardWidthScaleFarDrums   = 0.840f;
        float highwayPosEnd = 1.12f;

        // Far-end fade parameters (exposed for debug UI)
        // farFadeLen: length of fade zone (fade starts at farFadeEnd - farFadeLen)
        // farFadeEnd: normalized position where elements are fully transparent and clipped
        // farFadeCurve: exponent for fade curve (1=linear, >1=late fade, <1=early fade)
        float farFadeLen   = 0.35f;
        float farFadeEnd   = 1.05f;
        float farFadeCurve = 1.0f;

        // Gridline position nudge (normalized position space, exposed for debug UI)
        float gridlinePosOffset = -0.020f;

        // Tunable sustain arc curves (exposed for debug UI)
        float sustainStartCurve    = 0.015f;
        float sustainEndCurve      = -0.010f;
        float barSustainStartCurve = -0.015f;
        float barSustainEndCurve   = -0.015f;

        // Tunable sustain position offsets (normalized position nudge)
        float sustainStartOffset    = 0.0f;
        float sustainEndOffset      = -0.050f;
        float barSustainStartOffset = 0.0f;
        float barSustainEndOffset   = -0.050f;

        // Tunable strikeline clip (how far past strikeline before clamping, in normalized position)
        float sustainClip    = -0.015f;
        float barSustainClip = -0.015f;

        // Tunable lane arc curves (exposed for debug UI)
        // Fretboard-wide: offsets lane corners to follow gridline curve
        float laneStartCurve = -0.025f;
        float laneEndCurve   = -0.035f;
        // Individual: lane-local arc within each lane's own width
        float laneInnerStartCurve = 0.040f;
        float laneInnerEndCurve   = -0.040f;
        float laneSideCurve  = 0.0f;    // Side edge curvature (follows neck curve)

        // Tunable lane position offsets
        float laneStartOffset = -0.010f;
        float laneEndOffset   = -0.010f;

        // Tunable lane clip (how far past strikeline lanes extend)
        // -0.3 matches gridline range (HIGHWAY_POS_START)
        float laneClip = -0.3f;

        // Tunable note/bar curvature (exposed for debug UI)
        float noteCurvature = PositionConstants::NOTE_CURVATURE;
        float barCurvature  = PositionConstants::BAR_CURVATURE;

        // Mutable lane coord arrays (exposed for debug UI tuning)
        PositionConstants::NormalizedCoordinates guitarLaneCoordsLocal[6] = {
            {0.179f, 0.34f, 0.73f, 0.234f, 0.639f, 0.32f},   // Open
            {0.228f, 0.363f, 0.71f, 0.22f, 0.099f, 0.055f}, // Green
            {0.330f, 0.412f, 0.71f, 0.22f, 0.112f, 0.065f}, // Red
            {0.445f, 0.465f, 0.71f, 0.22f, 0.111f, 0.065f}, // Yellow
            {0.558f, 0.524f, 0.71f, 0.22f, 0.112f, 0.065f}, // Blue
            {0.674f, 0.580f, 0.71f, 0.22f, 0.100f, 0.065f}  // Orange
        };
        PositionConstants::NormalizedCoordinates drumLaneCoordsLocal[5] = {
            {0.182f, 0.34f, 0.735f, 0.239f, 0.636f, 0.32f},  // Kick
            {0.228f, 0.37f, 0.70f, 0.22f, 0.136f, 0.0714f},  // Red
            {0.365f, 0.430f, 0.70f, 0.22f, 0.134f, 0.0714f}, // Yellow
            {0.501f, 0.495f, 0.70f, 0.22f, 0.134f, 0.0714f}, // Blue
            {0.636f, 0.564f, 0.70f, 0.22f, 0.137f, 0.0714f}  // Green
        };

    private:

        DrawCallMap drawCallMap;
        void drawGridlinesFromMap(juce::Graphics &g, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime);
        void drawGridline(juce::Graphics &g, float position, juce::Image *markerImage, Gridline gridlineType, float fadeOpacity = 1.0f);

        void drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime);
        void drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime);
        void drawGem(uint gemColumn, const GemWrapper& gemMods, float position, double frameTime);

        void drawSustainFromWindow(juce::Graphics &g, const TimeBasedSustainWindow& sustainWindow, double windowStartTime, double windowEndTime);
        void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
        void drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour, bool isLane = false);
        void draw(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> position, float opacity)
        {
            g.setOpacity(opacity);
            g.drawImage(*image, position);
        };

        void drawCurved(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> rect,
                        float opacity, float fbCenterX, float fbHalfWidth, float arcHeight);

        // Sustain rendering helper functions (delegated to ColumnRenderer)
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
                                                    HIGHWAY_POS_START, highwayPosEnd,
                                                    colCoords, sizeScale, fretboardScale);
        }

};