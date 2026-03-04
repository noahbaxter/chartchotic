/*
    ==============================================================================

        TrackRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TrackRenderer.h"

TrackRenderer::TrackRenderer(juce::ValueTree& state)
    : state(state)
{
    trackDrumImage = juce::ImageCache::getFromMemory(BinaryData::track_drum_png, BinaryData::track_drum_pngSize);
    trackGuitarImage = juce::ImageCache::getFromMemory(BinaryData::track_guitar_png, BinaryData::track_guitar_pngSize);
}

void TrackRenderer::paint(juce::Graphics& g)
{
    if (fadedTrackImage.isValid())
        g.drawImageAt(fadedTrackImage, 0, 0);
}

void TrackRenderer::rebuild(int width, int height,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               float wNear, float wMid, float wFar, float posEnd)
{
    if (width <= 0 || height <= 0) return;

    bool isDrums = isPart(state, Part::DRUMS);
    juce::Image& sourceTrack = isDrums ? trackDrumImage : trackGuitarImage;

    fadedTrackImage = createFadedTrackImage(sourceTrack, width, height, isDrums,
                                             farFadeEnd, farFadeLen, farFadeCurve,
                                             wNear, wMid, wFar, posEnd);
}
