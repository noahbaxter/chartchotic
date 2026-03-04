/*
    ==============================================================================

        AssetManager.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"

// Forward declarations
class MidiInterpreter;

class AssetManager
{
public:
    AssetManager();
    ~AssetManager();

    //==========================================================================
    // Cache management

    // Re-render all SVG drawables to cached images at the given viewport size.
    // Call when the viewport dimensions change (e.g. on resize).
    // No-ops if dimensions match the last render.
    void renderCachedImages(int viewportWidth, int viewportHeight);

    //==========================================================================
    // Picker methods (logic for selecting the right asset by game state)

    juce::Image* getGuitarGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive);
    juce::Image* getDrumGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive);
    juce::Image* getGridlineImage(Gridline gridlineType);
    juce::Image* getOverlayImage(Gem gem, Part part);
    juce::Image* getSustainImage(uint gemColumn, bool starPowerActive);
    juce::Colour getLaneColour(uint gemColumn, Part part, bool starPowerActive);

    //==========================================================================
    // Raster assets (PNG/JPG — scaled bitmaps, or SVG-rendered cached images)

    // Bar/Open notes
    juce::Image* getBarKickImage() { return &barKickImage; }
    juce::Image* getBarKick2xImage() { return &barKick2xImage; }
    juce::Image* getBarOpenImage() { return &barOpenImage; }
    juce::Image* getBarWhiteImage() { return &barWhiteImage; }

    // Cymbal notes
    juce::Image* getCymBlueImage() { return &cymBlueImage; }
    juce::Image* getCymGreenImage() { return &cymGreenImage; }
    juce::Image* getCymRedImage() { return &cymRedImage; }
    juce::Image* getCymWhiteImage() { return &cymWhiteImage; }
    juce::Image* getCymYellowImage() { return &cymYellowImage; }

    // HOPO notes
    juce::Image* getHopoBarOpenImage() { return &barWhiteImage; }   // TODO: create unique asset
    juce::Image* getHopoBarWhiteImage() { return &barWhiteImage; }  // TODO: create unique asset
    juce::Image* getHopoBlueImage() { return &hopoBlueImage; }
    juce::Image* getHopoGreenImage() { return &hopoGreenImage; }
    juce::Image* getHopoOrangeImage() { return &hopoOrangeImage; }
    juce::Image* getHopoRedImage() { return &hopoRedImage; }
    juce::Image* getHopoWhiteImage() { return &hopoWhiteImage; }
    juce::Image* getHopoYellowImage() { return &hopoYellowImage; }

    // Lane graphics
    juce::Image* getLaneEndImage() { return &laneEndImage; }
    juce::Image* getLaneMidImage() { return &laneMidImage; }
    juce::Image* getLaneStartImage() { return &laneStartImage; }

    // Marker graphics (PNG fallback)
    juce::Image* getMarkerBeatImage() { return &markerBeatImage; }
    juce::Image* getMarkerHalfBeatImage() { return &markerHalfBeatImage; }
    juce::Image* getMarkerMeasureImage() { return &markerMeasureImage; }

    // Regular notes
    juce::Image* getNoteBlueImage() { return &noteBlueImage; }
    juce::Image* getNoteGreenImage() { return &noteGreenImage; }
    juce::Image* getNoteOrangeImage() { return &noteOrangeImage; }
    juce::Image* getNoteRedImage() { return &noteRedImage; }
    juce::Image* getNoteWhiteImage() { return &noteWhiteImage; }
    juce::Image* getNoteYellowImage() { return &noteYellowImage; }

    // Overlay graphics
    juce::Image* getOverlayCymAccentImage() { return &overlayCymAccentImage; }
    juce::Image* getOverlayCymGhost80scaleImage() { return &overlayCymGhost80scaleImage; }
    juce::Image* getOverlayCymGhostImage() { return &overlayCymGhostImage; }
    juce::Image* getOverlayNoteAccentImage() { return &overlayNoteAccentImage; }
    juce::Image* getOverlayNoteGhostImage() { return &overlayNoteGhostImage; }
    juce::Image* getOverlayNoteTapImage() { return &overlayNoteTapImage; }

    // Sustain graphics
    juce::Image* getSustainBlueImage() { return &sustainBlueImage; }
    juce::Image* getSustainGreenImage() { return &sustainGreenImage; }
    juce::Image* getSustainOpenWhiteImage() { return &sustainOpenWhiteImage; }
    juce::Image* getSustainOpenImage() { return &sustainOpenImage; }
    juce::Image* getSustainOrangeImage() { return &sustainOrangeImage; }
    juce::Image* getSustainRedImage() { return &sustainRedImage; }
    juce::Image* getSustainWhiteImage() { return &sustainWhiteImage; }
    juce::Image* getSustainYellowImage() { return &sustainYellowImage; }

    // Hit animation graphics
    juce::Image* getHitAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 5) return &hitAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    juce::Image* getHitFlareImage(uint gemColumn, Part part) {
        if (part == Part::GUITAR && gemColumn >= 1 && gemColumn <= 5) {
            return &hitFlareImages[gemColumn - 1];
        } else if (part == Part::DRUMS && gemColumn >= 1 && gemColumn <= 4) {
            uint flareIndex = (gemColumn == 4) ? 0 : gemColumn;
            return &hitFlareImages[flareIndex];
        }
        return nullptr;
    }
    juce::Image* getKickAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &kickAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    juce::Image* getOpenAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &openAnimationFrames[frameNumber - 1];
        return nullptr;
    }

    //==========================================================================
    // Vector assets (SVG — resolution-independent drawables)

    juce::Drawable* getGridlineDrawable(Gridline gridlineType);

    // Pre-parsed gridline paths for direct fillPath rendering (no Drawable overhead)
    const juce::Path& getGridlinePath(Gridline gridlineType);

private:
    void initRasterAssets();
    void initVectorAssets();

    // Render a Drawable to a juce::Image at the given width, preserving aspect ratio.
    static juce::Image renderDrawableToImage(juce::Drawable* drawable, int targetWidth);

    //==========================================================================
    // Cached render state

    int cachedWidth = 0;
    int cachedHeight = 0;

    //==========================================================================
    // Raster data (juce::Image — loaded from PNG/JPG, overwritten by SVG cache)

    juce::Image barKickImage, barKick2xImage, barOpenImage, barWhiteImage;
    juce::Image cymBlueImage, cymGreenImage, cymRedImage, cymWhiteImage, cymYellowImage;
    juce::Image hopoBlueImage, hopoGreenImage, hopoOrangeImage, hopoRedImage, hopoWhiteImage, hopoYellowImage;
    juce::Image laneEndImage, laneMidImage, laneStartImage;
    juce::Image markerBeatImage, markerHalfBeatImage, markerMeasureImage;
    juce::Image noteBlueImage, noteGreenImage, noteOrangeImage, noteRedImage, noteWhiteImage, noteYellowImage;
    juce::Image overlayCymAccentImage, overlayCymGhost80scaleImage, overlayCymGhostImage;
    juce::Image overlayNoteAccentImage, overlayNoteGhostImage, overlayNoteTapImage;
    juce::Image sustainBlueImage, sustainGreenImage, sustainOpenWhiteImage, sustainOpenImage;
    juce::Image sustainOrangeImage, sustainRedImage, sustainWhiteImage, sustainYellowImage;
    juce::Image hitAnimationFrames[5];
    juce::Image hitFlareImages[5];
    juce::Image kickAnimationFrames[7];
    juce::Image openAnimationFrames[7];

    //==========================================================================
    // Pre-parsed gridline paths (direct fillPath, no Drawable overhead)

    juce::Path gridlineBeatPath;
    juce::Path gridlineHalfBeatPath;

    //==========================================================================
    // Vector data (juce::Drawable — loaded from SVG binary data)

    // Gridlines (kept for PNG fallback cache rendering)
    std::unique_ptr<juce::Drawable> gridlineBeatSvg;
    std::unique_ptr<juce::Drawable> gridlineHalfBeatSvg;
    std::unique_ptr<juce::Drawable> gridlineMeasureSvg;

    // Note glyphs
    std::unique_ptr<juce::Drawable> noteBlueSvg, noteGreenSvg, noteOrangeSvg;
    std::unique_ptr<juce::Drawable> noteRedSvg, noteWhiteSvg, noteYellowSvg;

    // HOPO glyphs
    std::unique_ptr<juce::Drawable> hopoBlueSvg, hopoGreenSvg, hopoOrangeSvg;
    std::unique_ptr<juce::Drawable> hopoRedSvg, hopoWhiteSvg, hopoYellowSvg;

    // Cymbal glyphs
    std::unique_ptr<juce::Drawable> cymBlueSvg, cymGreenSvg, cymRedSvg;
    std::unique_ptr<juce::Drawable> cymWhiteSvg, cymYellowSvg;

    // Bar/kick/open notes
    std::unique_ptr<juce::Drawable> barKickSvg, barKick2xSvg;
    std::unique_ptr<juce::Drawable> barOpenSvg, barWhiteSvg;

    // Overlays
    std::unique_ptr<juce::Drawable> overlayNoteGhostSvg, overlayCymGhostSvg;
    std::unique_ptr<juce::Drawable> overlayNoteTapSvg;

    // Sustains
    std::unique_ptr<juce::Drawable> sustainOpenSvg, sustainOpenWhiteSvg;

    // Hit effects
    std::unique_ptr<juce::Drawable> hitFlareSvgs[5];
    std::unique_ptr<juce::Drawable> hitAnimationSvgs[5];
};
