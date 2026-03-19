#pragma once

#include <JuceHeader.h>
#include "Renderers/TrackRenderer.h"
#include "Utils/PositionMath.h"

class TrackImageCache
{
public:
    struct BakedTrack {
        juce::Image fadedTrack;
        juce::Image layers[TrackRenderer::NUM_LAYERS];
        bool valid = false;
    };

    BakedTrack& get(bool isDrums) { return isDrums ? drums : guitar; }
    const BakedTrack& get(bool isDrums) const { return isDrums ? drums : guitar; }

    /** Bake both guitar and drums track images using a scratch renderer.
        Lane coords are passed in so the renderer can project lane lines for each instrument. */
    void rebake(TrackRenderer& renderer, int width, int height, int overflow,
                float farFadeEnd, float farFadeLen, float farFadeCurve, float posEnd,
                const PositionConstants::NormalizedCoordinates* guitarLaneCoords, int guitarLaneCount,
                const PositionConstants::NormalizedCoordinates* drumLaneCoords, int drumLaneCount)
    {
        auto bakeOne = [&](bool isDrums, BakedTrack& out) {
            renderer.activePart = isDrums ? Part::DRUMS : Part::GUITAR;
            auto* coords = isDrums ? drumLaneCoords : guitarLaneCoords;
            int count = isDrums ? drumLaneCount : guitarLaneCount;
            renderer.setLaneCoords(coords, count);
            renderer.invalidate();
            renderer.rebuild(width, height, overflow, farFadeEnd, farFadeLen, farFadeCurve, posEnd);

            out.fadedTrack = renderer.getFadedTrackImage().createCopy();
            for (int i = 0; i < TrackRenderer::NUM_LAYERS; i++)
                out.layers[i] = renderer.getLayerImage((TrackRenderer::Layer)i).createCopy();
            out.valid = true;
        };

        bakeOne(false, guitar);
        bakeOne(true, drums);

        bakedWidth = width;
        bakedHeight = height;
        bakedOverflow = overflow;
        dirty = false;
    }

    void invalidate() { dirty = true; }
    bool isDirty() const { return dirty; }
    bool isValid() const { return !dirty && guitar.valid && drums.valid; }

    int getBakedWidth() const { return bakedWidth; }
    int getBakedHeight() const { return bakedHeight; }
    int getBakedOverflow() const { return bakedOverflow; }

private:
    BakedTrack guitar, drums;
    bool dirty = true;
    int bakedWidth = 0, bakedHeight = 0, bakedOverflow = 0;
};
