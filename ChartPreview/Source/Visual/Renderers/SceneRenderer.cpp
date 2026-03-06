/*
  ==============================================================================

    SceneRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "SceneRenderer.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/PositionConstants.h"

using namespace PositionConstants;

SceneRenderer::SceneRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter)
	: state(state),
	  midiInterpreter(midiInterpreter),
	  assetManager(),
	  noteRenderer(state, assetManager),
	  sustainRenderer(state, assetManager),
	  gridlineRenderer(state, assetManager),
	  animationRenderer(state, midiInterpreter, assetManager)
{
}

SceneRenderer::~SceneRenderer()
{
}

void SceneRenderer::paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying)
{
    ScopedPhaseMeasure totalMeasure(lastPhaseTiming.total_us, collectPhaseTiming);

    // Set the drawing area dimensions from the graphics context
    auto clipBounds = g.getClipBounds();
    width = clipBounds.getWidth();
    height = clipBounds.getHeight();

    // Resolve instrument-specific bezier params
    bool isDrums = isPart(state, Part::DRUMS);
    float wNear = isDrums ? fretboardWidthScaleNearDrums : fretboardWidthScaleNearGuitar;
    float wMid  = isDrums ? fretboardWidthScaleMidDrums  : fretboardWidthScaleMidGuitar;
    float wFar  = isDrums ? fretboardWidthScaleFarDrums  : fretboardWidthScaleFarGuitar;

    // Update sustain states for active animations (force-trigger if playing into active sustain)
    animationRenderer.updateSustainStates(sustainWindow, isPlaying);

    // Clear animations when paused to indicate gem is not being played
    if (!isPlaying)
    {
        animationRenderer.reset();
    }

    // Repopulate drawCallMap
    for (auto& row : drawCallMap)
        for (auto& bucket : row)
            bucket.clear();

    // Resolution scale: all pixel-based Z offsets are tuned at REFERENCE_HEIGHT
    float resScale = (float)height / PositionConstants::REFERENCE_HEIGHT;

    noteRenderer.noteCurvatureGuitar = noteCurvatureGuitar;
    noteRenderer.noteCurvatureDrums = noteCurvatureDrums;
    noteRenderer.gemWidthScale = gemWidthScale;
    noteRenderer.gemHeightScale = gemHeightScale;
    noteRenderer.barWidthScale = barWidthScale;
    noteRenderer.barHeightScale = barHeightScale;
    float strikePosGem = isDrums ? strikePosGemDrums : strikePosGemGuitar;
    float strikePosBar = isDrums ? strikePosBarDrums : strikePosBarGuitar;

    noteRenderer.showGems = showGems;
    noteRenderer.showBars = showBars;
    noteRenderer.gemZOffset = (isDrums ? gemZOffsetDrums : gemZOffsetGuitar) * resScale;
    noteRenderer.barZOffset = (isDrums ? barZOffsetDrums : barZOffsetGuitar) * resScale;
    noteRenderer.strikePosGem = strikePosGem;
    noteRenderer.strikePosBar = strikePosBar;
    noteRenderer.gemNoteScale = gemNoteScale;
    noteRenderer.gemHopoScale = gemHopoScale;
    noteRenderer.gemHopoBaseScale = gemHopoBaseScale;
    noteRenderer.gemTapOverlayScale = gemTapOverlayScale;
    noteRenderer.gemGhostOverlayScale = gemGhostOverlayScale;
    noteRenderer.gemAccentOverlayScale = gemAccentOverlayScale;
    noteRenderer.gemNoteBaseScale = gemNoteBaseScale;
    noteRenderer.gemCymScale = gemCymScale;
    noteRenderer.gemCymBaseScale = gemCymBaseScale;
    noteRenderer.gemSpScale = gemSpScale;
    for (int i = 0; i < 5; i++)
        noteRenderer.drumColZOffsets[i] = drumColZOffsets[i] * resScale;
    for (int i = 0; i < 6; i++) {
        noteRenderer.guitarColXOffsets[i] = guitarGemXOffsets[i] * resScale;
        noteRenderer.guitarColXOffsets2[i] = guitarGemXOffsets2[i] * resScale;
    }
    for (int i = 0; i < 5; i++) {
        noteRenderer.drumColXOffsets[i] = drumGemXOffsets[i] * resScale;
        noteRenderer.drumColXOffsets2[i] = drumGemXOffsets2[i] * resScale;
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.notes_us, collectPhaseTiming);
        if (showGems || showBars)
            noteRenderer.populate(drawCallMap, trackWindow, windowStartTime, windowEndTime,
                                  width, height, wNear, wMid, wFar, highwayPosEnd,
                                  farFadeEnd, farFadeLen, farFadeCurve);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.sustains_us, collectPhaseTiming);
        sustainRenderer.populate(drawCallMap, sustainWindow, windowStartTime, windowEndTime,
                                 width, height, showLanes, showSustains,
                                 wNear, wMid, wFar, highwayPosEnd,
                                 farFadeEnd, farFadeLen, farFadeCurve,
                                 guitarLaneCoordsLocal, drumLaneCoordsLocal);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.gridlines_us, collectPhaseTiming);
        if (showGridlines)
            gridlineRenderer.populate(drawCallMap, gridlines, windowStartTime, windowEndTime,
                                      width, height, wNear, wMid, wFar, highwayPosEnd,
                                      gridlinePosOffset,
                                      (isDrums ? gridZOffsetDrums : gridZOffsetGuitar) * resScale,
                                      farFadeEnd, farFadeLen, farFadeCurve);
    }

    // Detect and add animations to drawCallMap (if enabled)
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_us, collectPhaseTiming);
        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            double windowTimeSpan = windowEndTime - windowStartTime;
            double strikeTimeOffset = strikePosGem * windowTimeSpan;
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow, strikeTimeOffset); }

            animationRenderer.hitGemZOffset = (isDrums ? hitGemZOffsetDrums : hitGemZOffsetGuitar) * resScale;
            animationRenderer.hitBarZOffset = (isDrums ? hitBarZOffsetDrums : hitBarZOffsetGuitar) * resScale;
            animationRenderer.noteCurvature = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
            animationRenderer.hitGemScale = hitGemScale;
            animationRenderer.hitBarScale = hitBarScale;
            animationRenderer.hitGemWidthScale = hitGemWidthScale;
            animationRenderer.hitGemHeightScale = hitGemHeightScale;
            animationRenderer.hitBarWidthScale = hitBarWidthScale;
            animationRenderer.hitBarHeightScale = hitBarHeightScale;
            animationRenderer.ghostScale = hitGhostScale;
            animationRenderer.accentScale = hitAccentScale;
            animationRenderer.hopoScale = hitHopoScale;
            animationRenderer.tapScale = hitTapScale;
            animationRenderer.spScale = hitSpScale;
            animationRenderer.spWhiteFlare = spWhiteFlare;
            animationRenderer.tapPurpleFlare = tapPurpleFlare;
            for (int i = 0; i < 5; i++)
                animationRenderer.drumColZOffsets[i] = drumColZOffsets[i] * resScale;

            animationRenderer.renderToDrawCallMap(drawCallMap, width, height, wNear, wMid, wFar, highwayPosEnd,
                                                strikePosGem);
        }
    }

    // Inject registered image overlays into drawCallMap
    for (auto& kv : overlays)
    {
        auto* img = kv.second;
        if (img && img->isValid())
            drawCallMap[static_cast<int>(kv.first)][0].push_back([img](juce::Graphics& g) { g.setOpacity(1.0f); g.drawImageAt(*img, 0, 0); });
    }

    // Draw layer by layer, then column by column within each layer
    {
        ScopedPhaseMeasure m(lastPhaseTiming.execute_us, collectPhaseTiming);
        if (collectPhaseTiming)
            std::fill(std::begin(lastPhaseTiming.layer_us), std::end(lastPhaseTiming.layer_us), 0.0);

        for (int d = 0; d < DRAW_ORDER_COUNT; d++)
        {
            ScopedPhaseMeasure lm(lastPhaseTiming.layer_us[d], collectPhaseTiming);
            for (int c = 0; c < MAX_DRAW_COLUMNS; c++)
            {
                auto& bucket = drawCallMap[d][c];
                // Draw each layer from back to front
                for (auto it = bucket.rbegin(); it != bucket.rend(); ++it)
                {
                    (*it)(g);
                }
            }
        }
    }

    // Advance animation frames after rendering (time-based)
    {
        double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double delta = (lastFrameTimeSeconds > 0.0) ? (now - lastFrameTimeSeconds) : (1.0 / 60.0);
        delta = juce::jlimit(0.001, 0.1, delta);  // Clamp to avoid huge jumps
        lastFrameTimeSeconds = now;

        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            animationRenderer.advanceFrames(delta);
        }
    }
}
