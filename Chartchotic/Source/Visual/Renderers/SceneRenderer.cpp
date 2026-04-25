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

SceneRenderer::SceneRenderer(juce::ValueTree &state, AssetManager &assetManager)
	: state(state),
	  assetManager(assetManager),
	  noteRenderer(state, assetManager),
	  sustainRenderer(state, assetManager),
	  gridlineRenderer(state, assetManager),
	  animationRenderer(state, assetManager)
{
    std::copy_n(PositionConstants::OVERLAY_DEFAULTS, PositionConstants::NUM_OVERLAY_TYPES, overlayAdjusts);
}

SceneRenderer::~SceneRenderer()
{
}

void SceneRenderer::paint(juce::Graphics &g, int viewportWidth, int viewportHeight, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, const TimeBasedFlipRegions& flipRegions, const TimeBasedEventMarkers& eventMarkers, double windowStartTime, double windowEndTime, bool isPlaying)
{
    ScopedPhaseMeasure totalMeasure(lastPhaseTiming.total_us, collectPhaseTiming);

    // Use the full viewport size, not clip bounds — partial repaints must not
    // change the perspective math or the scene will render incorrectly.
    width = viewportWidth;
    height = viewportHeight;

    bool isDrums = isDrumLike(activePart);

    // Propagate activePart to sub-renderers
    noteRenderer.activePart = activePart;
    sustainRenderer.activePart = activePart;
    gridlineRenderer.activePart = activePart;
    animationRenderer.activePart = activePart;

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

    const auto& offsets = isDrums ? drumOffsets : guitarOffsets;

    noteRenderer.noteCurvatureGuitar = noteCurvatureGuitar;
    noteRenderer.noteCurvatureDrums = noteCurvatureDrums;
    // Per-instrument base scales — guitar gems are smaller/narrower than drum gems,
    // guitar bars are wider than drum kicks. The DebugTuningPanel still writes to
    // sceneRenderer.{gemScale,barScale}; if the user has tuned them off-default we
    // honor that, otherwise we use the per-instrument constant for the active part.
    bool gemScaleAtGuitarDefault = std::abs(gemScale.width  - PositionConstants::GUITAR_GEM_SCALE.width)  < 0.001f
                                && std::abs(gemScale.height - PositionConstants::GUITAR_GEM_SCALE.height) < 0.001f;
    bool barScaleAtGuitarDefault = std::abs(barScale.width  - PositionConstants::GUITAR_BAR_SCALE.width)  < 0.001f
                                && std::abs(barScale.height - PositionConstants::GUITAR_BAR_SCALE.height) < 0.001f;
    if (isDrums)
    {
        noteRenderer.gemScale = gemScaleAtGuitarDefault ? PositionConstants::DRUM_GEM_SCALE : gemScale;
        noteRenderer.barScale = barScaleAtGuitarDefault ? PositionConstants::DRUM_BAR_SCALE : barScale;
    }
    else
    {
        noteRenderer.gemScale = gemScale;
        noteRenderer.barScale = barScale;
    }
    float strikePosGem = offsets.strikePosGem;
    float strikePosBar = offsets.strikePosBar;

    noteRenderer.showGems = showGems;
    noteRenderer.showBars = showBars;
    noteRenderer.gemZOffset = offsets.gemZ * resScale;
    noteRenderer.cymZOffset = offsets.cymZ * resScale;
    noteRenderer.barZOffset = offsets.barZ * resScale;
    noteRenderer.strikePosGem = strikePosGem;
    noteRenderer.strikePosBar = strikePosBar;
    noteRenderer.gemTypeScales = gemTypeScales;
    std::copy_n(overlayAdjusts, PositionConstants::NUM_OVERLAY_TYPES, noteRenderer.overlayAdjusts);
    for (int i = 0; i < 6; i++) {
        const auto& ca = guitarColAdjust[i];
        noteRenderer.guitarColAdjust[i] = {
            ca.z * resScale,
            ca.sNear, ca.sFar, ca.w, ca.h };
    }
    for (int i = 0; i < 5; i++) {
        const auto& ca = drumColAdjust[i];
        noteRenderer.drumColAdjust[i] = {
            ca.z * resScale,
            ca.sNear, ca.sFar, ca.w, ca.h };
    }
    noteRenderer.laneCoordsGuitar = guitarLaneCoordsLocal;
    noteRenderer.laneCoordsDrums = drumLaneCoordsLocal;

    {
        ScopedPhaseMeasure m(lastPhaseTiming.notes_us, collectPhaseTiming);
        if (showGems || showBars)
            noteRenderer.populate(drawCallMap, trackWindow, windowStartTime, windowEndTime,
                                  width, height, highwayPosEnd,
                                  farFadeEnd, farFadeLen, farFadeCurve);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.sustains_us, collectPhaseTiming);
        sustainRenderer.laneShape = laneShape;
        sustainRenderer.populate(drawCallMap, sustainWindow, windowStartTime, windowEndTime,
                                 width, height, showLanes, showSustains,
                                 highwayPosEnd,
                                 farFadeEnd, farFadeLen, farFadeCurve,
                                 guitarLaneCoordsLocal, drumLaneCoordsLocal);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.gridlines_us, collectPhaseTiming);
        if (showGridlines)
            gridlineRenderer.populate(drawCallMap, gridlines, windowStartTime, windowEndTime,
                                      width, height, highwayPosEnd,
                                      gridlinePosOffset,
                                      offsets.gridZ * resScale,
                                      farFadeEnd, farFadeLen, farFadeCurve);
    }

    // Text event markers (disco flip start/end)
    if (!flipRegions.empty())
    {
        textEventRenderer.activePart = activePart;
        textEventRenderer.populate(drawCallMap, flipRegions, windowStartTime, windowEndTime,
                                   width, height, highwayPosEnd,
                                   farFadeEnd, farFadeLen, farFadeCurve);
    }

    // TODO: enable event marker rendering (tempo/timesig changes, sections, lyrics, etc.)
    // textEventRenderer.activePart = activePart;
    // textEventRenderer.populateEventMarkers(drawCallMap, eventMarkers, windowStartTime, windowEndTime,
    //                                        width, height, highwayPosEnd,
    //                                        farFadeEnd, farFadeLen, farFadeCurve);

    // Detect and add animations to drawCallMap (if enabled)
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_us, collectPhaseTiming);
        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            double windowTimeSpan = windowEndTime - windowStartTime;
            double strikeTimeOffset = strikePosGem * windowTimeSpan;
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow, strikeTimeOffset); }

            animationRenderer.laneCoordsGuitar = guitarLaneCoordsLocal;
            animationRenderer.laneCoordsDrums = drumLaneCoordsLocal;
            animationRenderer.hitGemZOffset = offsets.hitGemZ * resScale;
            animationRenderer.hitBarZOffset = offsets.hitBarZ * resScale;
            animationRenderer.noteCurvature = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
            animationRenderer.hitGemScale = hitGemScale;
            animationRenderer.hitBarScale = hitBarScale;
            animationRenderer.hitTypeConfig = hitTypeConfig;
            for (int i = 0; i < 5; i++)
                animationRenderer.drumColZAdjust[i] = drumColAdjust[i].z * resScale;

            animationRenderer.renderToDrawCallMap(drawCallMap, width, height, highwayPosEnd,
                                                strikePosGem);
        }
    }

    // Inject registered image overlays into drawCallMap
    for (auto& kv : overlays)
    {
        auto order = kv.first;
        auto* img = kv.second;
        if (!img || !img->isValid()) continue;

        // Skip track/strikeline overlays when toggled off
        if (!showTrack && (order == DrawOrder::TRACK_SIDEBARS || order == DrawOrder::TRACK_CONNECTORS))
            continue;
        if (!showLaneSeparators && order == DrawOrder::TRACK_LANE_LINES)
            continue;
        if (!showStrikeline && order == DrawOrder::TRACK_STRIKELINE)
            continue;

        int vw = width, vh = height;
        int ov = overlayYOffset;
        int totalH = vh + ov;
        drawCallMap[static_cast<int>(order)][0].push_back([img, vw, totalH, ov](juce::Graphics& g) {
            g.setOpacity(1.0f);
            int imgW = img->getWidth(), imgH = img->getHeight();
            if (imgW == vw && imgH == totalH)
                g.drawImageAt(*img, 0, -ov);
            else
                g.drawImage(*img, 0, -ov, vw, totalH, 0, 0, imgW, imgH);
        });
    }

    // Inject custom draw calls (e.g. bemani sidebar rails)
    for (auto& entry : customDrawCalls)
    {
        if (entry.second)
        {
            auto& drawFn = entry.second;
            drawCallMap[static_cast<int>(entry.first)][0].push_back([&drawFn](juce::Graphics& g) { drawFn(g); });
        }
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
