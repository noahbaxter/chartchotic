/*
    ==============================================================================

        TrackRenderer.h
        Author:  Noah Baxter

        Renders the track background texture with far-end fade.
        Will be expanded with additional track visuals (e.g., overlays, effects).

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"
#include "TrackFade.h"

class TrackRenderer
{
public:
    TrackRenderer(juce::ValueTree& state);

    void paint(juce::Graphics& g);

    /** Rebuild the cached faded track image. Call on resize, instrument change, or highway length change. */
    void rebuild(int width, int height,
                 float farFadeEnd, float farFadeLen, float farFadeCurve,
                 float wNear, float wMid, float wFar, float posEnd);

private:
    juce::ValueTree& state;

    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;
    juce::Image fadedTrackImage;
};
