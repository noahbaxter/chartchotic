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


class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

        void paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying = true);

        void setHighwayTexture(const juce::Image& texture) { highwayTexture = texture; }
        void clearHighwayTexture() { highwayTexture = juce::Image(); }
        void setScrollOffset(double offset) { scrollOffset = offset; }

    private:
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;
        AssetManager assetManager;
        AnimationRenderer animationRenderer;
        GlyphRenderer glyphRenderer;
        ColumnRenderer columnRenderer;

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

        float calculateOpacity(float position)
        {
            // Make the gem fade out as it gets closer to the end
            if (position >= OPACITY_FADE_START)
            {
                return 1.0 - ((position - OPACITY_FADE_START) / (1.0f - OPACITY_FADE_START));
            }

            return 1.0;
        }

        // Highway texture overlay
        juce::Image highwayTexture;
        juce::Image highwayOffscreen;
        double scrollOffset = 0.0;
        static constexpr int HIGHWAY_MIN_STRIPS = 40;
        static constexpr float HIGHWAY_TILES_PER_HIGHWAY = 1.0f;
        static constexpr float HIGHWAY_OPACITY = 0.45f;
        static constexpr float HIGHWAY_POS_START = -0.3f;
        void drawHighwayTexture(juce::Graphics &g);

    public:
        // Tunable fretboard boundary scales (exposed for debug UI)
        // Near = strikeline (bottom), Mid = midpoint, Far = far end (top)
        float fretboardWidthScaleNearGuitar = 0.785f;
        float fretboardWidthScaleMidGuitar  = 0.820f;
        float fretboardWidthScaleFarGuitar  = 0.855f;
        float fretboardWidthScaleNearDrums  = 0.800f;
        float fretboardWidthScaleMidDrums   = 0.820f;
        float fretboardWidthScaleFarDrums   = 0.840f;
        float highwayPosEnd = 1.12f;

    private:

        DrawCallMap drawCallMap;
        void drawGridlinesFromMap(juce::Graphics &g, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime);
        void drawGridline(juce::Graphics &g, float position, juce::Image *markerImage, Gridline gridlineType);

        void drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime);
        void drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime);
        void drawGem(uint gemColumn, const GemWrapper& gemMods, float position, double frameTime);

        void drawSustainFromWindow(juce::Graphics &g, const TimeBasedSustainWindow& sustainWindow, double windowStartTime, double windowEndTime);
        void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
        void drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour);
        void draw(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> position, float opacity)
        {
            g.setOpacity(opacity);
            g.drawImage(*image, position);
        };

        // Draw image curved along the fretboard-wide arc. Renders strips into a
        // supersampled offscreen image for clean compositing.
        void drawCurved(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> rect,
                        float opacity, float fbCenterX, float fbHalfWidth, float arcHeight)
        {
            if (arcHeight == 0.0f || fbHalfWidth == 0.0f) { draw(g, image, rect, opacity); return; }

            constexpr int S = PositionConstants::NOTE_RENDER_SCALE;
            constexpr int STRIPS = 12;
            float absArc = std::abs(arcHeight);
            int offW = ((int)std::ceil(rect.getWidth()) + 2) * S;
            int offH = ((int)std::ceil(rect.getHeight() + absArc) + 2) * S;
            if (offW <= 0 || offH <= 0) return;

            juce::Image offscreen(juce::Image::ARGB, offW, offH, true);
            {
                juce::Graphics og(offscreen);
                og.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

                float stripW = rect.getWidth() * S / STRIPS;
                int imgW = image->getWidth();
                float srcStripW = (float)imgW / STRIPS;
                float baseY = ((arcHeight > 0.0f) ? absArc : 0.0f) * S;

                for (int i = 0; i < STRIPS; i++)
                {
                    float stripCenterX = rect.getX() + (rect.getWidth() / STRIPS) * (i + 0.5f);
                    float dist = (stripCenterX - fbCenterX) / fbHalfWidth;
                    float yOff = arcHeight * (1.0f - dist * dist) * S;

                    int srcX = (int)(srcStripW * i);
                    int srcEnd = std::min((int)(srcStripW * (i + 1) + 0.5f), imgW);

                    og.drawImage(*image,
                                 (int)(stripW * i), (int)(baseY + yOff),
                                 (int)std::ceil(stripW), (int)std::ceil(rect.getHeight() * S),
                                 srcX, 0, srcEnd - srcX, image->getHeight());
                }
            }

            float drawY = rect.getY() - ((arcHeight > 0.0f) ? absArc : 0.0f);
            g.setOpacity(opacity);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            juce::Rectangle<float> destRect(rect.getX() - 1, drawY,
                                             (float)offW / S, (float)offH / S);
            g.drawImage(offscreen, destRect);
        };

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