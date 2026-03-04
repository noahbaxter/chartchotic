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
#include "../Utils/RenderTiming.h"


class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

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
        AnimationRenderer animationRenderer;
        GlyphRenderer glyphRenderer;

        uint width = 0, height = 0;

        // Bezier positioning helpers
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

        float calculateOpacity(float position)
        {
            // Make the gem fade out as it gets closer to the end
            if (position >= OPACITY_FADE_START)
            {
                return 1.0 - ((position - OPACITY_FADE_START) / (1.0f - OPACITY_FADE_START));
            }

            return 1.0;
        }

        DrawCallMap drawCallMap;
        void drawGridlinesFromMap(juce::Graphics &g, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime);
        void drawGridline(juce::Graphics &g, float position, juce::Image *markerImage, Gridline gridlineType);

        void drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime);
        void drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime);
        void drawGem(uint gemColumn, const GemWrapper& gemMods, float position, double frameTime);

        void drawSustainFromWindow(juce::Graphics &g, const TimeBasedSustainWindow& sustainWindow, double windowStartTime, double windowEndTime);
        void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
        void drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour, bool isLane);
        void draw(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> position, float opacity)
        {
            g.setOpacity(opacity);
            g.drawImage(*image, position);
        };

        // Testing helper functions
        TrackWindow generateFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            TrackWindow fakeTrackWindow;

            // Use PPQ values, not floats - create notes every 0.25 PPQ starting from trackWindowStartPPQ
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.25)] = {Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.50)] = {Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.75)] = {Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.00)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.25)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.50)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.75)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE};

            return fakeTrackWindow;
        }

        TrackWindow generateFullFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            TrackWindow fakeTrackWindow;

            // Use PPQ values, not floats - create notes every 0.25 PPQ starting from trackWindowStartPPQ
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(4.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};

            return fakeTrackWindow;
        }

        SustainWindow generateFakeSustainWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            SustainWindow fakeSustainWindow;

            if (isPart(state, Part::GUITAR)) {
                // Guitar uses lanes 0-5 (6 lanes total)
                for (uint i = 0; i < 6; i++)
                {
                    SustainEvent sustain;
                    sustain.startPPQ = trackWindowStartPPQ + PPQ(0.0);
                    sustain.endPPQ = trackWindowStartPPQ + PPQ(2.0);
                    sustain.gemColumn = i;
                    sustain.gemType = Gem::NOTE;
                    fakeSustainWindow.push_back(sustain);
                }
            } else {
                // Drums uses lanes 0,1,2,3,4 (5 lanes total)
                for (uint i = 0; i < 5; i++)
                {
                    SustainEvent sustain;
                    sustain.startPPQ = trackWindowStartPPQ + PPQ(0.0);
                    sustain.endPPQ = trackWindowStartPPQ + PPQ(2.0);
                    sustain.gemColumn = i;
                    sustain.gemType = Gem::NOTE;
                    fakeSustainWindow.push_back(sustain);
                }
            }

            return fakeSustainWindow;
        }

};