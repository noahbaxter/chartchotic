/*
    ==============================================================================

        AnimationRenderer.h
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.
        Combines detection (note crossing), state updates (sustains), and rendering.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../Midi/Processing/MidiInterpreter.h"
#include "../../Utils/Utils.h"
#include "../../Utils/TimeConverter.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/AssetManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

class AnimationRenderer
{
public:
    AnimationRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter, AssetManager &assetManager);
    ~AnimationRenderer();

    /**
     * Detect notes that have crossed the strikeline and trigger animations.
     * Should be called once per frame when playback is active.
     */
    void detectAndTriggerAnimations(const TimeBasedTrackWindow& trackWindow);

    /**
     * Update sustain states based on current sustain window.
     * Animations hold at frame 1 during sustains.
     * If isPlaying and sustain is active but no animation exists, force-trigger it.
     */
    void updateSustainStates(const TimeBasedSustainWindow& sustainWindow, bool isPlaying);

    /**
     * Populate drawCallMap with animation render calls.
     * Animations are added to BAR_ANIMATION and NOTE_ANIMATION layers for proper Z-ordering.
     * Bezier params (wNear/wMid/wFar/posEnd) come from SceneRenderer so debug sliders apply.
     */
    void renderToDrawCallMap(DrawCallMap& drawCallMap, uint width, uint height,
                             float wNear, float wMid, float wFar, float posEnd);

    /**
     * Advance all active animations by delta time.
     * Call this once per render frame after rendering.
     */
    void advanceFrames(double deltaSeconds);

    /**
     * Clear all animations (e.g., when transport stops).
     */
    void reset();

private:
    juce::ValueTree &state;
    MidiInterpreter &midiInterpreter;
    AnimationManager animationManager;
    AssetManager& assetManager;

    // Track the last note time per column to ensure every note triggers an animation
    std::array<double, 7> lastNoteTimePerColumn = {-999.0, -999.0, -999.0, -999.0, -999.0, -999.0, -999.0};

    // Helper: Trigger animation for a specific gem column
    void triggerAnimationForColumn(uint gemColumn);

    // Bezier column edge helper (mirrors SceneRenderer::getColumnEdge)
    PositionConstants::LaneCorners getColumnEdge(float position, const PositionConstants::NormalizedCoordinates& colCoords,
                                                  float sizeScale, float wNear, float wMid, float wFar, float posEnd,
                                                  float fretboardScale = 1.0f)
    {
        bool isDrums = isPart(state, Part::DRUMS);
        return PositionMath::getColumnPosition(isDrums, position, cachedWidth, cachedHeight,
                                               wNear, wMid, wFar,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale);
    }

    uint cachedWidth = 0, cachedHeight = 0;

    // Rendering helpers
    void renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                             float wNear, float wMid, float wFar, float posEnd);
    void renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                             float wNear, float wMid, float wFar, float posEnd);
};
