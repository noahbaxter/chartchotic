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
    drawCallMap.clear();

    {
        ScopedPhaseMeasure m(lastPhaseTiming.notes_us, collectPhaseTiming);
        if (showNotes)
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
                                      gridlinePosOffset, farFadeEnd, farFadeLen, farFadeCurve);
    }

    // Detect and add animations to drawCallMap (if enabled)
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_us, collectPhaseTiming);
        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow); }
            animationRenderer.renderToDrawCallMap(drawCallMap, width, height, wNear, wMid, wFar, highwayPosEnd);
        }
    }

    // Inject registered image overlays into drawCallMap
    for (auto& kv : overlays)
    {
        auto* img = kv.second;
        if (img && img->isValid())
            drawCallMap[kv.first][0].push_back([img](juce::Graphics& g) { g.setOpacity(1.0f); g.drawImageAt(*img, 0, 0); });
    }

    // Draw layer by layer, then column by column within each layer
    {
        ScopedPhaseMeasure m(lastPhaseTiming.execute_us, collectPhaseTiming);
        if (collectPhaseTiming)
            lastPhaseTiming.layer_us.clear();

        for (const auto& drawOrder : drawCallMap)
        {
            ScopedPhaseMeasure lm(lastPhaseTiming.layer_us[drawOrder.first], collectPhaseTiming);
            for (const auto& column : drawOrder.second)
            {
                // Draw each layer from back to front
                for (auto it = column.second.rbegin(); it != column.second.rend(); ++it)
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
