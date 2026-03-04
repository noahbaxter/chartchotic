/*
    ==============================================================================

        AssetManager.cpp
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#include "AssetManager.h"

//==============================================================================
// Helper: load an SVG drawable from BinaryData, returning nullptr if data is null/empty.

static std::unique_ptr<juce::Drawable> loadSvg(const char* data, int size)
{
    if (data == nullptr || size <= 0)
        return nullptr;
    return juce::Drawable::createFromImageData(data, (size_t)size);
}

//==============================================================================

AssetManager::AssetManager()
{
    initRasterAssets();
    initVectorAssets();

    // Pre-parse gridline SVG path data (measure and beat share the same path)
    gridlineBeatPath = juce::Drawable::parseSVGPath(
        "M286.67,9.28c-91.44-9.2-182.59-9.2-274.03,0-.26.03-.48.22-.54.48-.55,2.27-.89,3.64-1.41,5.81-.1.41.24.79.65.75,92.31-9.43,184.31-9.43,276.62,0,.42.04.75-.34.65-.75-.52-2.17-.85-3.54-1.41-5.81-.06-.25-.28-.45-.54-.48Z");
    gridlineHalfBeatPath = juce::Drawable::parseSVGPath(
        "M287.39,10.82c-91.92-9.32-183.55-9.32-275.48,0-.26.03-.48.22-.54.48-.26,1.09-.44,1.82-.68,2.83-.1.41.23.8.65.75,92.31-9.43,184.31-9.43,276.62,0,.42.04.75-.35.65-.75-.24-1.01-.42-1.74-.68-2.83-.06-.25-.28-.45-.54-.48Z");
}

AssetManager::~AssetManager()
{
}

//==============================================================================
// Raster assets (PNG/JPG fallback)

void AssetManager::initRasterAssets()
{
    barKickImage = juce::ImageCache::getFromMemory(BinaryData::bar_kick_png, BinaryData::bar_kick_pngSize);
    barKick2xImage = juce::ImageCache::getFromMemory(BinaryData::bar_kick_2x_png, BinaryData::bar_kick_2x_pngSize);
    barOpenImage = juce::ImageCache::getFromMemory(BinaryData::bar_open_png, BinaryData::bar_open_pngSize);
    barWhiteImage = juce::ImageCache::getFromMemory(BinaryData::bar_white_png, BinaryData::bar_white_pngSize);

    cymBlueImage = juce::ImageCache::getFromMemory(BinaryData::cym_blue_png, BinaryData::cym_blue_pngSize);
    cymGreenImage = juce::ImageCache::getFromMemory(BinaryData::cym_green_png, BinaryData::cym_green_pngSize);
    cymRedImage = juce::ImageCache::getFromMemory(BinaryData::cym_red_png, BinaryData::cym_red_pngSize);
    cymWhiteImage = juce::ImageCache::getFromMemory(BinaryData::cym_white_png, BinaryData::cym_white_pngSize);
    cymYellowImage = juce::ImageCache::getFromMemory(BinaryData::cym_yellow_png, BinaryData::cym_yellow_pngSize);

    hopoBlueImage = juce::ImageCache::getFromMemory(BinaryData::hopo_blue_png, BinaryData::hopo_blue_pngSize);
    hopoGreenImage = juce::ImageCache::getFromMemory(BinaryData::hopo_green_png, BinaryData::hopo_green_pngSize);
    hopoOrangeImage = juce::ImageCache::getFromMemory(BinaryData::hopo_orange_png, BinaryData::hopo_orange_pngSize);
    hopoRedImage = juce::ImageCache::getFromMemory(BinaryData::hopo_red_png, BinaryData::hopo_red_pngSize);
    hopoWhiteImage = juce::ImageCache::getFromMemory(BinaryData::hopo_white_png, BinaryData::hopo_white_pngSize);
    hopoYellowImage = juce::ImageCache::getFromMemory(BinaryData::hopo_yellow_png, BinaryData::hopo_yellow_pngSize);

    laneEndImage = juce::ImageCache::getFromMemory(BinaryData::lane_end_png, BinaryData::lane_end_pngSize);
    laneMidImage = juce::ImageCache::getFromMemory(BinaryData::lane_mid_png, BinaryData::lane_mid_pngSize);
    laneStartImage = juce::ImageCache::getFromMemory(BinaryData::lane_start_png, BinaryData::lane_start_pngSize);

    markerBeatImage = juce::ImageCache::getFromMemory(BinaryData::marker_beat_png, BinaryData::marker_beat_pngSize);
    markerHalfBeatImage = juce::ImageCache::getFromMemory(BinaryData::marker_half_beat_png, BinaryData::marker_half_beat_pngSize);
    markerMeasureImage = juce::ImageCache::getFromMemory(BinaryData::marker_measure_png, BinaryData::marker_measure_pngSize);

    noteBlueImage = juce::ImageCache::getFromMemory(BinaryData::note_blue_png, BinaryData::note_blue_pngSize);
    noteGreenImage = juce::ImageCache::getFromMemory(BinaryData::note_green_png, BinaryData::note_green_pngSize);
    noteOrangeImage = juce::ImageCache::getFromMemory(BinaryData::note_orange_png, BinaryData::note_orange_pngSize);
    noteRedImage = juce::ImageCache::getFromMemory(BinaryData::note_red_png, BinaryData::note_red_pngSize);
    noteWhiteImage = juce::ImageCache::getFromMemory(BinaryData::note_white_png, BinaryData::note_white_pngSize);
    noteYellowImage = juce::ImageCache::getFromMemory(BinaryData::note_yellow_png, BinaryData::note_yellow_pngSize);

    overlayCymAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_accent_png, BinaryData::overlay_cym_accent_pngSize);
    overlayCymGhost80scaleImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_80scale_png, BinaryData::overlay_cym_ghost_80scale_pngSize);
    overlayCymGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_png, BinaryData::overlay_cym_ghost_pngSize);
    overlayNoteAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_accent_png, BinaryData::overlay_note_accent_pngSize);
    overlayNoteGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_ghost_png, BinaryData::overlay_note_ghost_pngSize);
    overlayNoteTapImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_tap_png, BinaryData::overlay_note_tap_pngSize);

    sustainBlueImage = juce::ImageCache::getFromMemory(BinaryData::sustain_blue_png, BinaryData::sustain_blue_pngSize);
    sustainGreenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_green_png, BinaryData::sustain_green_pngSize);
    sustainOpenWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_white_png, BinaryData::sustain_open_white_pngSize);
    sustainOpenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_png, BinaryData::sustain_open_pngSize);
    sustainOrangeImage = juce::ImageCache::getFromMemory(BinaryData::sustain_orange_png, BinaryData::sustain_orange_pngSize);
    sustainRedImage = juce::ImageCache::getFromMemory(BinaryData::sustain_red_png, BinaryData::sustain_red_pngSize);
    sustainWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_white_png, BinaryData::sustain_white_pngSize);
    sustainYellowImage = juce::ImageCache::getFromMemory(BinaryData::sustain_yellow_png, BinaryData::sustain_yellow_pngSize);

    // Hit animation frames
    hitAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_1_png, BinaryData::hit_1_pngSize);
    hitAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_2_png, BinaryData::hit_2_pngSize);
    hitAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_3_png, BinaryData::hit_3_pngSize);
    hitAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_4_png, BinaryData::hit_4_pngSize);
    hitAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_5_png, BinaryData::hit_5_pngSize);

    // Hit flare images (blue=4, green=1, orange=5, red=2, yellow=3)
    hitFlareImages[0] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_green_png, BinaryData::hit_flare_green_pngSize);
    hitFlareImages[1] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_red_png, BinaryData::hit_flare_red_pngSize);
    hitFlareImages[2] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_yellow_png, BinaryData::hit_flare_yellow_pngSize);
    hitFlareImages[3] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_blue_png, BinaryData::hit_flare_blue_pngSize);
    hitFlareImages[4] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_orange_png, BinaryData::hit_flare_orange_pngSize);

    // Kick animation frames
    kickAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_1_png, BinaryData::hit_kick_1_pngSize);
    kickAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_2_png, BinaryData::hit_kick_2_pngSize);
    kickAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_3_png, BinaryData::hit_kick_3_pngSize);
    kickAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_4_png, BinaryData::hit_kick_4_pngSize);
    kickAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_5_png, BinaryData::hit_kick_5_pngSize);
    kickAnimationFrames[5] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_6_png, BinaryData::hit_kick_6_pngSize);
    kickAnimationFrames[6] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_7_png, BinaryData::hit_kick_7_pngSize);

    // Open animation frames (guitar open notes)
    openAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_open_1_png, BinaryData::hit_open_1_pngSize);
    openAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_open_2_png, BinaryData::hit_open_2_pngSize);
    openAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_open_3_png, BinaryData::hit_open_3_pngSize);
    openAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_open_4_png, BinaryData::hit_open_4_pngSize);
    openAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_open_5_png, BinaryData::hit_open_5_pngSize);
    openAnimationFrames[5] = juce::ImageCache::getFromMemory(BinaryData::hit_open_6_png, BinaryData::hit_open_6_pngSize);
    openAnimationFrames[6] = juce::ImageCache::getFromMemory(BinaryData::hit_open_7_png, BinaryData::hit_open_7_pngSize);
}

//==============================================================================
// Vector assets (SVG)

void AssetManager::initVectorAssets()
{
    // Gridlines (existing)
    gridlineBeatSvg = loadSvg(BinaryData::gridline_beat_svg, BinaryData::gridline_beat_svgSize);
    gridlineHalfBeatSvg = loadSvg(BinaryData::gridline_half_beat_svg, BinaryData::gridline_half_beat_svgSize);
    gridlineMeasureSvg = loadSvg(BinaryData::gridline_measure_svg, BinaryData::gridline_measure_svgSize);

    // Note glyphs
    noteBlueSvg = loadSvg(BinaryData::note_blue_svg, BinaryData::note_blue_svgSize);
    noteGreenSvg = loadSvg(BinaryData::note_green_svg, BinaryData::note_green_svgSize);
    noteOrangeSvg = loadSvg(BinaryData::note_orange_svg, BinaryData::note_orange_svgSize);
    noteRedSvg = loadSvg(BinaryData::note_red_svg, BinaryData::note_red_svgSize);
    noteWhiteSvg = loadSvg(BinaryData::note_white_svg, BinaryData::note_white_svgSize);
    noteYellowSvg = loadSvg(BinaryData::note_yellow_svg, BinaryData::note_yellow_svgSize);

    // HOPO glyphs
    hopoBlueSvg = loadSvg(BinaryData::hopo_blue_svg, BinaryData::hopo_blue_svgSize);
    hopoGreenSvg = loadSvg(BinaryData::hopo_green_svg, BinaryData::hopo_green_svgSize);
    hopoOrangeSvg = loadSvg(BinaryData::hopo_orange_svg, BinaryData::hopo_orange_svgSize);
    hopoRedSvg = loadSvg(BinaryData::hopo_red_svg, BinaryData::hopo_red_svgSize);
    hopoWhiteSvg = loadSvg(BinaryData::hopo_white_svg, BinaryData::hopo_white_svgSize);
    hopoYellowSvg = loadSvg(BinaryData::hopo_yellow_svg, BinaryData::hopo_yellow_svgSize);

    // Cymbal glyphs
    cymBlueSvg = loadSvg(BinaryData::cym_blue_svg, BinaryData::cym_blue_svgSize);
    cymGreenSvg = loadSvg(BinaryData::cym_green_svg, BinaryData::cym_green_svgSize);
    cymRedSvg = loadSvg(BinaryData::cym_red_svg, BinaryData::cym_red_svgSize);
    cymWhiteSvg = loadSvg(BinaryData::cym_white_svg, BinaryData::cym_white_svgSize);
    cymYellowSvg = loadSvg(BinaryData::cym_yellow_svg, BinaryData::cym_yellow_svgSize);

    // Bar/kick/open notes
    barKickSvg = loadSvg(BinaryData::bar_kick_svg, BinaryData::bar_kick_svgSize);
    barKick2xSvg = loadSvg(BinaryData::bar_kick_2x_svg, BinaryData::bar_kick_2x_svgSize);
    barOpenSvg = loadSvg(BinaryData::bar_open_svg, BinaryData::bar_open_svgSize);
    barWhiteSvg = loadSvg(BinaryData::bar_white_svg, BinaryData::bar_white_svgSize);

    // Overlays
    overlayNoteGhostSvg = loadSvg(BinaryData::overlay_note_ghost_svg, BinaryData::overlay_note_ghost_svgSize);
    overlayCymGhostSvg = loadSvg(BinaryData::overlay_cym_ghost_svg, BinaryData::overlay_cym_ghost_svgSize);
    overlayNoteTapSvg = loadSvg(BinaryData::overlay_note_tap_svg, BinaryData::overlay_note_tap_svgSize);

    // Sustains
    sustainOpenSvg = loadSvg(BinaryData::sustain_open_svg, BinaryData::sustain_open_svgSize);
    sustainOpenWhiteSvg = loadSvg(BinaryData::sustain_open_white_svg, BinaryData::sustain_open_white_svgSize);

    // Hit flares (green=0, red=1, yellow=2, blue=3, orange=4)
    hitFlareSvgs[0] = loadSvg(BinaryData::hit_flare_green_svg, BinaryData::hit_flare_green_svgSize);
    hitFlareSvgs[1] = loadSvg(BinaryData::hit_flare_red_svg, BinaryData::hit_flare_red_svgSize);
    hitFlareSvgs[2] = loadSvg(BinaryData::hit_flare_yellow_svg, BinaryData::hit_flare_yellow_svgSize);
    hitFlareSvgs[3] = loadSvg(BinaryData::hit_flare_blue_svg, BinaryData::hit_flare_blue_svgSize);
    hitFlareSvgs[4] = loadSvg(BinaryData::hit_flare_orange_svg, BinaryData::hit_flare_orange_svgSize);

    // Hit animation frames
    hitAnimationSvgs[0] = loadSvg(BinaryData::hit_1_svg, BinaryData::hit_1_svgSize);
    hitAnimationSvgs[1] = loadSvg(BinaryData::hit_2_svg, BinaryData::hit_2_svgSize);
    hitAnimationSvgs[2] = loadSvg(BinaryData::hit_3_svg, BinaryData::hit_3_svgSize);
    hitAnimationSvgs[3] = loadSvg(BinaryData::hit_4_svg, BinaryData::hit_4_svgSize);
    hitAnimationSvgs[4] = loadSvg(BinaryData::hit_5_svg, BinaryData::hit_5_svgSize);
}

//==============================================================================
// SVG → cached Image rendering

juce::Image AssetManager::renderDrawableToImage(juce::Drawable* drawable, int targetWidth)
{
    if (drawable == nullptr || targetWidth <= 0)
        return {};

    auto bounds = drawable->getDrawableBounds();
    if (bounds.isEmpty())
        return {};

    float aspect = bounds.getWidth() / bounds.getHeight();
    int w = targetWidth;
    int h = std::max(1, (int)(w / aspect));

    juce::Image img(juce::Image::ARGB, w, h, true);
    juce::Graphics g(img);
    drawable->drawWithin(g, juce::Rectangle<float>(0.0f, 0.0f, (float)w, (float)h),
                         juce::RectanglePlacement::centred, 1.0f);
    return img;
}

void AssetManager::renderCachedImages(int viewportWidth, int viewportHeight)
{
    if (viewportWidth == cachedWidth && viewportHeight == cachedHeight)
        return;

    cachedWidth = viewportWidth;
    cachedHeight = viewportHeight;

    // Render SVG drawables to cached images at sizes appropriate for the viewport.
    // 2x scale factor provides quality headroom for perspective transforms.
    constexpr float scale = 2.0f;

    // Regular glyphs: max ~15% of viewport width at strikeline
    int glyphW = (int)(viewportWidth * 0.15f * scale);

    // Bar notes (kick/open): max ~70% of viewport width at strikeline
    int barW = (int)(viewportWidth * 0.70f * scale);

    // Overlays: same size as the glyphs they sit on
    int overlayW = glyphW;

    // Sustains: narrower than glyphs
    int sustainW = (int)(viewportWidth * 0.10f * scale);

    // Hit effects: medium
    int hitW = (int)(viewportWidth * 0.20f * scale);

    // Helper macro: render SVG to Image if the Drawable exists
    #define RENDER_SVG(drawable, image, width) \
        if (drawable) image = renderDrawableToImage(drawable.get(), width)

    // Note glyphs
    RENDER_SVG(noteBlueSvg, noteBlueImage, glyphW);
    RENDER_SVG(noteGreenSvg, noteGreenImage, glyphW);
    RENDER_SVG(noteOrangeSvg, noteOrangeImage, glyphW);
    RENDER_SVG(noteRedSvg, noteRedImage, glyphW);
    RENDER_SVG(noteWhiteSvg, noteWhiteImage, glyphW);
    RENDER_SVG(noteYellowSvg, noteYellowImage, glyphW);

    // HOPO glyphs
    RENDER_SVG(hopoBlueSvg, hopoBlueImage, glyphW);
    RENDER_SVG(hopoGreenSvg, hopoGreenImage, glyphW);
    RENDER_SVG(hopoOrangeSvg, hopoOrangeImage, glyphW);
    RENDER_SVG(hopoRedSvg, hopoRedImage, glyphW);
    RENDER_SVG(hopoWhiteSvg, hopoWhiteImage, glyphW);
    RENDER_SVG(hopoYellowSvg, hopoYellowImage, glyphW);

    // Cymbal glyphs
    RENDER_SVG(cymBlueSvg, cymBlueImage, glyphW);
    RENDER_SVG(cymGreenSvg, cymGreenImage, glyphW);
    RENDER_SVG(cymRedSvg, cymRedImage, glyphW);
    RENDER_SVG(cymWhiteSvg, cymWhiteImage, glyphW);
    RENDER_SVG(cymYellowSvg, cymYellowImage, glyphW);

    // Bar/kick/open notes
    RENDER_SVG(barKickSvg, barKickImage, barW);
    RENDER_SVG(barKick2xSvg, barKick2xImage, barW);
    RENDER_SVG(barOpenSvg, barOpenImage, barW);
    RENDER_SVG(barWhiteSvg, barWhiteImage, barW);

    // Overlays
    RENDER_SVG(overlayNoteGhostSvg, overlayNoteGhostImage, overlayW);
    RENDER_SVG(overlayCymGhostSvg, overlayCymGhostImage, overlayW);
    RENDER_SVG(overlayNoteTapSvg, overlayNoteTapImage, overlayW);

    // Sustains
    RENDER_SVG(sustainOpenSvg, sustainOpenImage, sustainW);
    RENDER_SVG(sustainOpenWhiteSvg, sustainOpenWhiteImage, sustainW);

    // Hit flares
    for (int i = 0; i < 5; ++i)
        RENDER_SVG(hitFlareSvgs[i], hitFlareImages[i], hitW);

    // Hit animation frames
    for (int i = 0; i < 5; ++i)
        RENDER_SVG(hitAnimationSvgs[i], hitAnimationFrames[i], hitW);

    #undef RENDER_SVG
}

//==============================================================================
// Picker methods

juce::Image* AssetManager::getGridlineImage(Gridline gridlineType)
{
    switch (gridlineType)
    {
    case Gridline::MEASURE: return getMarkerMeasureImage();
    case Gridline::BEAT: return getMarkerBeatImage();
    case Gridline::HALF_BEAT: return getMarkerHalfBeatImage();
    }

    return nullptr;
}

juce::Drawable* AssetManager::getGridlineDrawable(Gridline gridlineType)
{
    switch (gridlineType)
    {
    case Gridline::MEASURE: return gridlineMeasureSvg.get();
    case Gridline::BEAT: return gridlineBeatSvg.get();
    case Gridline::HALF_BEAT: return gridlineHalfBeatSvg.get();
    }

    return nullptr;
}

const juce::Path& AssetManager::getGridlinePath(Gridline gridlineType)
{
    if (gridlineType == Gridline::HALF_BEAT)
        return gridlineHalfBeatPath;
    return gridlineBeatPath;  // MEASURE and BEAT share the same shape
}

juce::Image* AssetManager::getGuitarGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive)
{
    // Use the gem's star power flag to determine if it should be white
    bool shouldBeWhite = starPowerActive && gemWrapper.starPower;

    if (shouldBeWhite)
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getHopoBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getHopoWhiteImage();
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getNoteWhiteImage();
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getHopoBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getHopoWhiteImage();
            } break;
        default: break;
        }
    }
    else
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getHopoBarOpenImage();
            case 1: return getHopoGreenImage();
            case 2: return getHopoRedImage();
            case 3: return getHopoYellowImage();
            case 4: return getHopoBlueImage();
            case 5: return getHopoOrangeImage();
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return getBarOpenImage();
            case 1: return getNoteGreenImage();
            case 2: return getNoteRedImage();
            case 3: return getNoteYellowImage();
            case 4: return getNoteBlueImage();
            case 5: return getNoteOrangeImage();
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getBarOpenImage();
            case 1: return getHopoGreenImage();
            case 2: return getHopoRedImage();
            case 3: return getHopoYellowImage();
            case 4: return getHopoBlueImage();
            case 5: return getHopoOrangeImage();
            } break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getDrumGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive)
{
    // Use the gem's star power flag to determine if it should be white
    bool shouldBeWhite = starPowerActive && gemWrapper.starPower;

    if (shouldBeWhite)
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0:
            case 6: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4: return getHopoWhiteImage();
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0:
            case 6: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4: return getNoteWhiteImage();
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2:
            case 3:
            case 4: return getCymWhiteImage();
            } break;
        default: break;
        }
    }
    else
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 1: return getHopoRedImage();
            case 2: return getHopoYellowImage();
            case 3: return getHopoBlueImage();
            case 4: return getHopoGreenImage();
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getBarKickImage();
            case 6: return getBarKick2xImage();
            case 1: return getNoteRedImage();
            case 2: return getNoteYellowImage();
            case 3: return getNoteBlueImage();
            case 4: return getNoteGreenImage();
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2: return getCymYellowImage();
            case 3: return getCymBlueImage();
            case 4: return getCymGreenImage();
            } break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getOverlayImage(Gem gem, Part part)
{
    if (part == Part::GUITAR)
    {
        switch (gem)
        {
        case Gem::TAP_ACCENT: return getOverlayNoteTapImage();
        default: break;
        }
    }
    else // if (part == Part::DRUMS)
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST: return getOverlayNoteGhostImage();
        case Gem::TAP_ACCENT: return getOverlayNoteAccentImage();
        case Gem::CYM_GHOST: return getOverlayCymGhostImage();
        case Gem::CYM_ACCENT: return getOverlayCymAccentImage();
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getSustainImage(uint gemColumn, bool starPowerActive)
{
    // Note: sustain color is determined by lane, not by gem type
    // For star power sustains, return white; otherwise return the lane color
    if (starPowerActive)
    {
        if (gemColumn == 0)
        {
            return getSustainOpenWhiteImage();
        }
        else
        {
            return getSustainWhiteImage();
        }
    }
    else
    {
        switch (gemColumn)
        {
        case 0: return getSustainOpenImage();
        case 1: return getSustainGreenImage();
        case 2: return getSustainRedImage();
        case 3: return getSustainYellowImage();
        case 4: return getSustainBlueImage();
        case 5: return getSustainOrangeImage();
        }
    }

    return nullptr;
}

juce::Colour AssetManager::getLaneColour(uint gemColumn, Part part, bool starPowerActive)
{
    if (starPowerActive)
    {
        return juce::Colours::white;
    }

    if (part == Part::GUITAR)
    {
        juce::Colour guitarColors[] = {
            juce::Colours::purple,
            juce::Colours::green,
            juce::Colours::red,
            juce::Colours::yellow,
            juce::Colours::blue,
            juce::Colours::orange
        };
        return guitarColors[std::min(gemColumn, 5u)];
    }
    else // if (part == Part::DRUMS)
    {
        juce::Colour drumColors[] = {
            juce::Colours::orange,
            juce::Colours::red,
            juce::Colours::yellow,
            juce::Colours::blue,
            juce::Colours::green
        };
        return drumColors[std::min(gemColumn, 4u)];
    }
}
