/*
    ==============================================================================

        AssetManager.cpp
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#include "AssetManager.h"
#include "../Geometry/PositionConstants.h"
#include "../Utils/LaneColours.h"
#include "../Utils/DrawingConstants.h"
#include "../Art/BarGemArt.h"

namespace
{
    // Recolour a COLOURED gem master (note / hopo / cymbal / bar) to a target lane colour by
    // re-hueing only the saturated part. A pixel's saturation is the mask: the neutral chrome
    // caps, white rims and specular highlights (saturation ~0) pass through untouched, while the
    // coloured body takes the target hue/saturation at its own brightness -- so the metallic
    // shading and the silver (or SP-gold, kept via the white PNGs) framing survive, and only the
    // colour changes. One master per family recolours to any lane; alpha is preserved.
    // valueScale < 1 darkens the recoloured body (leaves neutral framing alone). Green is the most
    // luminant hue, so the bright metallic cymbal master reads too neon in green even at the right
    // saturation; pull its value down without touching the other colours or the square notes.
    // neutralFloor drags the recolour weight up off the source's own saturation, so the neutral
    // framing and highlights a gem keeps by design also take the hue. 0 leaves them alone, which
    // is what every ordinary gem wants; the activation cut is the exception.
    juce::Image recolorGem(const juce::Image& master, juce::Colour target, float valueScale = 1.0f,
                           float neutralFloor = 0.0f)
    {
        const float th = target.getHue();
        const float ts = target.getSaturation();
        juce::Image out = master.createCopy();
        juce::Image::BitmapData bmp(out, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bmp.height; ++y)
            for (int x = 0; x < bmp.width; ++x)
            {
                auto px = bmp.getPixelColour(x, y);
                // Weight the recolour by how saturated the source pixel is: neutral framing
                // stays put, the colour body swaps hue. Ramp (not a hard cut) avoids a seam.
                float w = juce::jlimit(0.0f, 1.0f, (px.getSaturation() - 0.18f) / 0.30f);
                w = juce::jmax(w, neutralFloor);
                float v = juce::jlimit(0.0f, 1.0f, px.getBrightness() * valueScale);
                auto reh = juce::Colour::fromHSV(th, ts, v, px.getFloatAlpha());
                bmp.setPixelColour(x, y, px.interpolatedWith(reh, w));
            }
        return out;
    }

    // Greyscale "smoke" master -> lane-coloured flare. A flare is an additive glow,
    // not metal: recolour by a simple luminance * colour multiply (keeping the soft
    // alpha falloff), the same recipe the old purple tap flare used, generalized to
    // any lane colour. Nothing baked per lane.
    juce::Image tintFlare(const juce::Image& master, juce::Colour tint)
    {
        const float tr = tint.getFloatRed(), tg = tint.getFloatGreen(), tb = tint.getFloatBlue();
        juce::Image out = master.createCopy();
        juce::Image::BitmapData bmp(out, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bmp.height; ++y)
            for (int x = 0; x < bmp.width; ++x)
            {
                auto px = bmp.getPixelColour(x, y);
                float lum = 0.3f * px.getFloatRed() + 0.59f * px.getFloatGreen() + 0.11f * px.getFloatBlue();
                bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(lum * tr, lum * tg, lum * tb, px.getFloatAlpha()));
            }
        return out;
    }
}

AssetManager::AssetManager()
{
    initAssets();
}

AssetManager::~AssetManager()
{
}

void AssetManager::initAssets()
{
#ifndef CHARTCHOTIC_NO_BINARY_DATA
    // Bars: one greyscale tube master (bar_white) tinted per bar type. Kick = amber,
    // 2x kick = a distinct red-orange (gameplay cue), open = the shared open/purple.
    // White (star power) is the untinted master. No per-colour bar PNGs.
    // Bars are procedurally baked from the tube geometry measured off the original art
    // (BarGemArt): a glossy arc-bent tube + tilted end caps, with the vertical colour ramp
    // sampled per bar from the source PNGs. Pixel-faithful and resolution-independent -- the
    // 2432x152 bake is scaled to the highway width at draw time, so it renders at any length.
    {
        const auto canvas  = GemArt::barCanvas();
        const auto content = GemArt::barContentBounds();
        // Only the star power bars are fixed art. Every coloured bar (kick, 2x kick, open, and
        // the elite dynamics) is baked per colour through barGem, because which colours exist
        // is a question about the person's palettes rather than a list known here.
        const float eliteTh = GemArt::kEliteBarThickness;
        barWhiteImage       = GemArt::bakeBar(GemArt::barRampWhite(), canvas, content);
        barWhiteEliteImage  = GemArt::bakeBar(GemArt::barRampWhite(), canvas, content, eliteTh);
        // Star power flattens every colour to white, so a white rim would say nothing there:
        // the SP accent keeps the centre line as its cue.
        barWhiteAccentImage = GemArt::bakeBar(GemArt::barRampWhite(), canvas, content, eliteTh,
                                              GemArt::kAccentCentreLineAlpha);
    }
    // Elite Stomp/Splash pedal bar: procedurally baked FLAT (like the kick bars). The highway
    // curve is applied at render time (over the bar's off-centre span) so it matches the tilted
    // gridline exactly, rather than baking a symmetric bow that can't tilt.
    barStompImage  = GemArt::bakeStompBar(0.0f, PositionConstants::ELITE_PEDAL_THICKNESS,
                                          GemArt::kStompBarFace);
    barSplashImage = GemArt::bakeStompBar(0.0f, PositionConstants::ELITE_PEDAL_THICKNESS,
                                          GemArt::kStompBarBody, GemArt::kStompBarFace,
                                          GemArt::kStompBarEdgeWidth);

    // Cymbals: one greyscale metallic master (the blue cymbal collapsed to luminance) drives
    // every lane colour through a LaneColours tint ramp, presets AND any custom hue alike, so
    // a colour changes in one place (the constant) with no per-colour PNG. The white / star
    // power cymbal keeps its PNG (its gold rim is not a greyscale tint).
    cymWhiteImage = juce::ImageCache::getFromMemory(BinaryData::cym_white_png, BinaryData::cym_white_pngSize);
    cymMaster = juce::ImageCache::getFromMemory(BinaryData::cym_blue_png, BinaryData::cym_blue_pngSize);
    // Elite hi-hat: extracted gold/metallic art, drawn as-is (NOT tinted) on the Hi-Hat lane.
    // Closed is the default; the open variant hooks in once open/closed parsing (upper octave
    // 84-90) lands to pick a gem type.
    cymHiHatClosedImage = juce::ImageCache::getFromMemory(BinaryData::cym_hihat_closed_png, BinaryData::cym_hihat_closed_pngSize);
    cymHiHatOpenImage   = juce::ImageCache::getFromMemory(BinaryData::cym_hihat_open_png,   BinaryData::cym_hihat_open_pngSize);

    // Notes + HOPOs: same greyscale-master + LaneColours tint as cymbals. One master per
    // family (a mid-tone colour PNG collapsed to luminance) recolours to every lane, so a
    // new part/palette needs no per-colour note art and purple gets real art for free. The
    // white (star power) glyphs keep their PNGs.
    noteWhiteImage = juce::ImageCache::getFromMemory(BinaryData::note_white_png, BinaryData::note_white_pngSize);
    noteMaster     = juce::ImageCache::getFromMemory(BinaryData::note_blue_png, BinaryData::note_blue_pngSize);
    hopoWhiteImage = juce::ImageCache::getFromMemory(BinaryData::hopo_white_png, BinaryData::hopo_white_pngSize);
    hopoMaster     = juce::ImageCache::getFromMemory(BinaryData::hopo_blue_png, BinaryData::hopo_blue_pngSize);

    laneEndImage = juce::ImageCache::getFromMemory(BinaryData::lane_end_png, BinaryData::lane_end_pngSize);
    laneMidImage = juce::ImageCache::getFromMemory(BinaryData::lane_mid_png, BinaryData::lane_mid_pngSize);
    laneStartImage = juce::ImageCache::getFromMemory(BinaryData::lane_start_png, BinaryData::lane_start_pngSize);

    // Gridline markers: procedurally baked (was baked-curve PNGs). The arc derives from the SAME
    // curvature constant the notes / stomp bar use, so the gridline and the pedal (stomp/splash)
    // bar curve from ONE source and pixel-match over their shared span, rather than the gridline
    // baking a hand-guessed arc the stomp bar then chases. Thickness/alpha reproduce the old PNGs
    // (measure = brightest; beat / half-beat dimmer; half-beat thinner). STEP reuses half-beat.
    {
        const float gridArch = GemArt::gridlineArch();
        markerMeasureImage  = GemArt::bakeGridline(GemArt::kGridlineMeasureThickness,  TrackColours::gridlineMeasure,  gridArch);
        markerBeatImage     = GemArt::bakeGridline(GemArt::kGridlineMeasureThickness,  TrackColours::gridlineBeat,     gridArch);
        markerHalfBeatImage = GemArt::bakeGridline(GemArt::kGridlineHalfBeatThickness, TrackColours::gridlineHalfBeat, gridArch);
    }

    // Write-mode markers: copies of the same source PNGs with their alpha
    // amplified. The original PNGs bake alpha (~0.75 MEASURE / ~0.50 BEAT)
    // which caps gridline opacity even with the renderer's per-type
    // multiplier at 1.0. Boost at load time so write mode can render
    // structural anchors at full opacity through the same perspective sprite
    // path as everything else (no parallel render code).
    markerMeasureWriteImage = makeAlphaBoostedCopy(markerMeasureImage);
    markerBeatWriteImage    = makeAlphaBoostedCopy(markerBeatImage);

    // STEP gridlines reuse the half-beat marker image — same loading path,
    // same scaling, same visual treatment. They're differentiated from
    // half-beats by opacity in GridlineRenderer (STEP 0.25 vs HALF_BEAT 0.35),
    // not by a separate asset. Adding a dedicated thinner PNG is an asset-pack
    // task tracked in BACKLOG.md — do not invent parallel rendering paths.

    noteBlankImage = juce::ImageCache::getFromMemory(BinaryData::note_blank_png, BinaryData::note_blank_pngSize);
    hopoBlankImage = juce::ImageCache::getFromMemory(BinaryData::hopo_blank_png, BinaryData::hopo_blank_pngSize);
    cymBlankImage  = juce::ImageCache::getFromMemory(BinaryData::cym_blank_png, BinaryData::cym_blank_pngSize);
    barBlankImage  = juce::ImageCache::getFromMemory(BinaryData::bar_blank_png, BinaryData::bar_blank_pngSize);


    overlayCymAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_accent_png, BinaryData::overlay_cym_accent_pngSize);
    overlayCymGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_png, BinaryData::overlay_cym_ghost_pngSize);
    overlayNoteAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_accent_png, BinaryData::overlay_note_accent_pngSize);
    overlayNoteGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_ghost_png, BinaryData::overlay_note_ghost_pngSize);
    overlayNoteTapImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_tap_png, BinaryData::overlay_note_tap_pngSize);

    sustainOpenWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_white_png, BinaryData::sustain_open_white_pngSize);
    sustainOpenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_png, BinaryData::sustain_open_pngSize);

    discoBallImage = juce::ImageCache::getFromMemory(BinaryData::discoball_jpg, BinaryData::discoball_jpgSize);

    // Hit animation frames
    hitAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_1_png, BinaryData::hit_flash_1_pngSize);
    hitAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_2_png, BinaryData::hit_flash_2_pngSize);
    hitAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_3_png, BinaryData::hit_flash_3_pngSize);
    hitAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_4_png, BinaryData::hit_flash_4_pngSize);
    hitAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_5_png, BinaryData::hit_flash_5_pngSize);

    // One greyscale "smoke" flare master. Every lane's flare is this tinted by the
    // lane colour on demand (see getHitFlareImage), so there are no per-colour flare
    // PNGs and any lane count / palette just works. Star power draws it untinted.
    hitFlareWhite = juce::ImageCache::getFromMemory(BinaryData::hit_flare_white_png, BinaryData::hit_flare_white_pngSize);

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

    // Register all per-frame drawable assets for viewport-aware pre-scaling.
    // Full-res originals are copied now; rescaleForWidth() overwrites the active members.
    // Entries default to the Lane width class; FULL marks the ones drawn board-wide.
    constexpr auto FULL = AssetWidthClass::FullBoard;
    // The lane-coloured gems are NOT here: they are baked per colour on demand and rescaled
    // through laneGems in rescaleForWidth, which is the same width-class treatment by another
    // route. Only the star power glyphs and the untinted hi-hat art are fixed images.
    scalableAssets = {
        // Notes
        {&noteWhiteImage, noteWhiteImage},
        // Cymbals
        {&cymWhiteImage, cymWhiteImage},
        {&cymHiHatClosedImage, cymHiHatClosedImage}, {&cymHiHatOpenImage, cymHiHatOpenImage},
        // HOPOs
        {&hopoWhiteImage, hopoWhiteImage},
        // Bars — full board width, so they keep a full-width master
        {&barWhiteImage, barWhiteImage, FULL},
        {&barWhiteEliteImage, barWhiteEliteImage, FULL},
        {&barWhiteAccentImage, barWhiteAccentImage, FULL},
        {&barStompImage, barStompImage, FULL}, {&barSplashImage, barSplashImage, FULL},
        // Overlays
        {&overlayCymAccentImage, overlayCymAccentImage}, {&overlayCymGhostImage, overlayCymGhostImage},
        {&overlayNoteAccentImage, overlayNoteAccentImage}, {&overlayNoteGhostImage, overlayNoteGhostImage},
        {&overlayNoteTapImage, overlayNoteTapImage},
        // Hit flash frames
        {&hitAnimationFrames[0], hitAnimationFrames[0]}, {&hitAnimationFrames[1], hitAnimationFrames[1]},
        {&hitAnimationFrames[2], hitAnimationFrames[2]}, {&hitAnimationFrames[3], hitAnimationFrames[3]},
        {&hitAnimationFrames[4], hitAnimationFrames[4]},
        // Hit flare: only the greyscale master is scaled; lane tints derive from it.
        {&hitFlareWhite, hitFlareWhite},
        // Kick animation frames — the kick flash spans the board like its bar
        {&kickAnimationFrames[0], kickAnimationFrames[0], FULL}, {&kickAnimationFrames[1], kickAnimationFrames[1], FULL},
        {&kickAnimationFrames[2], kickAnimationFrames[2], FULL}, {&kickAnimationFrames[3], kickAnimationFrames[3], FULL},
        {&kickAnimationFrames[4], kickAnimationFrames[4], FULL}, {&kickAnimationFrames[5], kickAnimationFrames[5], FULL},
        {&kickAnimationFrames[6], kickAnimationFrames[6], FULL},
        // Open animation frames — same, they follow the open bar
        {&openAnimationFrames[0], openAnimationFrames[0], FULL}, {&openAnimationFrames[1], openAnimationFrames[1], FULL},
        {&openAnimationFrames[2], openAnimationFrames[2], FULL}, {&openAnimationFrames[3], openAnimationFrames[3], FULL},
        {&openAnimationFrames[4], openAnimationFrames[4], FULL}, {&openAnimationFrames[5], openAnimationFrames[5], FULL},
        {&openAnimationFrames[6], openAnimationFrames[6], FULL},
        // Gridline markers — drawn across the whole fretboard
        {&markerBeatImage, markerBeatImage, FULL}, {&markerHalfBeatImage, markerHalfBeatImage, FULL},
        {&markerMeasureImage, markerMeasureImage, FULL},
        {&markerMeasureWriteImage, markerMeasureWriteImage, FULL},
        {&markerBeatWriteImage,    markerBeatWriteImage, FULL},
        // Ghost cursor blanks (the bar blank follows the bars)
        {&noteBlankImage, noteBlankImage}, {&hopoBlankImage, hopoBlankImage},
        {&cymBlankImage, cymBlankImage}, {&barBlankImage, barBlankImage, FULL},
    };
#endif // CHARTCHOTIC_NO_BINARY_DATA
}

juce::Image AssetManager::makeAlphaBoostedCopy(const juce::Image& src)
{
    if (!src.isValid())
        return {};

    juce::Image out = src.createCopy();
    juce::Image::BitmapData bd(out, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
    {
        juce::uint8* line = bd.getLinePointer(y);
        for (int x = 0; x < bd.width; ++x)
        {
            juce::uint8* px = line + x * bd.pixelStride;
            // ARGB byte order on macOS/Win is BGRA in memory; alpha is the
            // 4th byte regardless. juce::PixelARGB stores it as the A
            // component — use the explicit getter for portability.
            juce::PixelARGB* pixel = reinterpret_cast<juce::PixelARGB*>(px);
            if (pixel->getAlpha() > 0)
                pixel->setAlpha(255);
        }
    }
    return out;
}

juce::Image AssetManager::downscale(const juce::Image& src, int targetWidth)
{
    if (!src.isValid() || targetWidth <= 0 || targetWidth >= src.getWidth())
        return src;

    float ratio = (float)targetWidth / (float)src.getWidth();
    int targetHeight = std::max(1, (int)(src.getHeight() * ratio));

    juce::Image scaled(juce::Image::ARGB, targetWidth, targetHeight, true);
    juce::Graphics g(scaled);
    g.drawImage(src, juce::Rectangle<float>(0, 0, (float)targetWidth, (float)targetHeight));
    return scaled;
}

void AssetManager::rescaleForWidth(int viewportWidth)
{
    if (viewportWidth == lastScaledWidth)
        return;

    // Two masters, because JUCE's image blit costs scale with the SOURCE size, not the
    // destination: resampling a half-viewport gem master down to a 9-lane elite gem cost
    // ~230us a sprite, against ~36us from a lane-sized master. Bars and gridline markers
    // really are drawn board-wide, so they keep the wide master and stay sharp; gems and
    // their overlays never exceed one lane's gem box, whose widest case is 4-lane drums at
    // roughly a third of the board once GEM_SIZE and the gem-scale slider are in.
    const int fullBoardWidth = fullBoardWidthFor(viewportWidth);
    const int laneWidth      = laneWidthFor(viewportWidth);

    for (auto& asset : scalableAssets)
        *asset.target = downscale(asset.fullRes,
            asset.widthClass == AssetWidthClass::FullBoard ? fullBoardWidth : laneWidth);

    // Baked lane gems take the same treatment. A resize is also the one safe place to drop
    // colours that have piled up from palette edits: every pointer into the cache is being
    // replaced on this pass regardless.
    if (laneGems.size() > laneGemCacheLimit) laneGems.clear();
    if (barGems.size()  > laneGemCacheLimit) barGems.clear();

    for (auto& [key, gem] : laneGems)
        gem.scaled = downscale(gem.fullRes, laneWidth);

    // Bars really are drawn board-wide, so they keep the wide master.
    for (auto& [key, bar] : barGems)
        bar.scaled = downscale(bar.fullRes, fullBoardWidth);

    // Flare tints derive from the (now rescaled) smoke master — drop them so they
    // regenerate at the new scale on next use.
    flareTintCache.clear();
    hitFrameTintCache.clear();

    lastScaledWidth = viewportWidth;
}

const juce::Image* AssetManager::masterFor(const juce::Image* live) const
{
    if (live == nullptr) return nullptr;
    for (const auto& asset : scalableAssets)
        if (asset.target == live) return &asset.fullRes;

    // Lane gems and bars keep their master beside them rather than in scalableAssets, so
    // anything drawn outside the highway (icons, swatches) can still get the unscaled bake.
    for (const auto& [key, gem] : laneGems)
        if (&gem.scaled == live) return &gem.fullRes;
    for (const auto& [key, bar] : barGems)
        if (&bar.scaled == live) return &bar.fullRes;

    return nullptr;
}

// The colour-carrying bars, baked from the stock tube profile re-hued to the palette. Same
// on-demand caching as the gems and for the same reason: which colours exist is now a
// question about the person's palettes, not a fixed list.
juce::Image* AssetManager::barGem(BarVariant variant, const LaneColours::Lane& lane)
{
    const auto key = std::make_pair((int)variant, lane.bright);

    auto it = barGems.find(key);
    if (it == barGems.end())
    {
        const auto canvas  = GemArt::barCanvas();
        const auto content = GemArt::barContentBounds();
        const auto target  = LaneColours::bright(lane);

        const float eliteTh  = GemArt::kEliteBarThickness;
        const float lift     = GemArt::kBarOutlineBrighten, sink = GemArt::kBarGhostDarken;
        const float rim      = GemArt::kBarOutlineAlpha,    ghostRim = GemArt::kBarGhostOutlineAlpha;
        const auto  ghostCol = GemArt::kBarGhostOutlineColour;

        auto base = [&](BarVariant v)
        {
            const bool twice = v == BarVariant::Kick2x || v == BarVariant::Kick2xElite
                            || v == BarVariant::Kick2xAccent || v == BarVariant::Kick2xGhost;
            const bool open  = v == BarVariant::Open || v == BarVariant::OpenHopo;

            return GemArt::recolouredRamp(open    ? GemArt::barRampOpen()
                                        : twice   ? GemArt::barRampKick2x()
                                                  : GemArt::barRampKick(), target);
        };

        auto shaded = [](GemArt::BarRamp ramp, float amount, bool up)
        {
            for (auto& stop : ramp)
                stop.second = up ? stop.second.brighter(amount) : stop.second.darker(amount);
            return ramp;
        };

        juce::Image baked;
        switch (variant)
        {
        case BarVariant::Kick:
        case BarVariant::Kick2x:
        case BarVariant::Open:
            baked = GemArt::bakeBar(base(variant), canvas, content);
            break;

        // An open HOPO keeps the open note's colour and takes the rim instead of being
        // replaced by the white SP bar, which read as a star power note that was not there.
        case BarVariant::OpenHopo:
            baked = GemArt::bakeBar(shaded(base(variant), lift, true), canvas, content,
                                    1.0f, 0.0f, rim);
            break;

        case BarVariant::KickElite:
        case BarVariant::Kick2xElite:
            baked = GemArt::bakeBar(base(variant), canvas, content, eliteTh);
            break;

        case BarVariant::KickAccent:
        case BarVariant::Kick2xAccent:
            baked = GemArt::bakeBar(shaded(base(variant), lift, true),
                                    canvas, content, eliteTh, 0.0f, rim);
            break;

        case BarVariant::KickGhost:
        case BarVariant::Kick2xGhost:
            baked = GemArt::bakeBar(shaded(base(variant), sink, false),
                                    canvas, content, eliteTh, 0.0f, ghostRim, ghostCol);
            break;
        }

        LaneGem bar;
        bar.fullRes = std::move(baked);
        bar.scaled  = lastScaledWidth > 0
                    ? downscale(bar.fullRes, fullBoardWidthFor(lastScaledWidth))
                    : bar.fullRes;

        it = barGems.emplace(key, std::move(bar)).first;
    }

    return &it->second.scaled;
}

// Baked on first ask, then kept. `dark` rides along in the key so a palette that only moves
// a lane's gradient still gets its own bake rather than silently reusing the old one.
juce::Image* AssetManager::laneGem(GemFamily family, const LaneColours::Lane& lane)
{
    const auto key = std::make_pair((int)family, lane.bright);

    auto it = laneGems.find(key);
    if (it == laneGems.end())
    {
        const juce::Image* master = family == GemFamily::Note   ? &noteMaster
                                  : family == GemFamily::Hopo   ? &hopoMaster
                                                                : &cymMaster;

        // The metallic master reads far brighter than the old green cymbal art did, so the
        // stock green keeps the darkening that was tuned to match it. A green someone picked
        // themselves is theirs and is left alone.
        const bool stockGreenCymbal = family == GemFamily::Cymbal
                                   && lane.bright == LaneColours::green.bright;

        LaneGem gem;
        gem.fullRes = recolorGem(*master, LaneColours::bright(lane), stockGreenCymbal ? 0.68f : 1.0f);
        // Scaled to the width already in force, so a colour first asked for mid-session is
        // not the one sprite on the board blitting from an oversized source.
        gem.scaled  = lastScaledWidth > 0 ? downscale(gem.fullRes, laneWidthFor(lastScaledWidth))
                                          : gem.fullRes;

        it = laneGems.emplace(key, std::move(gem)).first;
    }

    return &it->second.scaled;
}

// Tint the smoke master by an arbitrary lane colour, memoized per colour. Node-based
// map => returned pointers stay valid as new colours are added within a frame.
juce::Image* AssetManager::flareTinted(juce::Colour colour)
{
    auto key = colour.getARGB();
    auto it = flareTintCache.find(key);
    if (it == flareTintCache.end())
        it = flareTintCache.emplace(key, tintFlare(hitFlareWhite, colour)).first;
    return &it->second;
}

juce::Image* AssetManager::getHitAnimationFrameTinted(int frameNumber, juce::Colour tint)
{
    auto* master = getHitAnimationFrame(frameNumber);
    if (master == nullptr) return nullptr;

    const auto key = std::make_pair(frameNumber, tint.getARGB());
    auto it = hitFrameTintCache.find(key);
    if (it == hitFrameTintCache.end())
        it = hitFrameTintCache.emplace(key, tintFlare(*master, tint)).first;
    return &it->second;
}

juce::Image* AssetManager::getHitFlarePurpleImage()
{
    return flareTinted(LaneColours::bright(LaneColours::purple));
}

juce::Image* AssetManager::getHitFlareImage(uint gemColumn, Part part, Gem gem)
{
    // Colour comes from the same generic source the gems/pads use, so the flare
    // lights up in the lane's own colour for any part / lane count (elite included).
    return flareTinted(getNoteColour(gemColumn, part, gem, /*starPowerActive=*/false));
}

juce::Image* AssetManager::getGhostCursorImage(bool isDrums, int lane)
{
    if (lane == DRUM_KICK_COLUMN) return &barBlankImage;
    if (isDrums && lane == DRUM_KICK_2X_COLUMN) return &barBlankImage;
    return &noteBlankImage;
}

juce::Image* AssetManager::getGuitarGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive, Part part)
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
        // The lane's colour comes from this part's palette, so bass and guitar can hold the
        // same fret in different colours. Column 0 is the open bar, which is art of its own.
        const auto& palette = paletteFor(part);
        auto gem = [&](GemFamily family) { return laneGem(family, palette.padFor(laneHue(gemColumn, part))); };

        // The open bar has its own palette entry, not the purple gem's: its art is a far
        // brighter orchid, and sharing one entry tied it to elite's left crash.
        const auto& open = palette.open;

        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            if (gemColumn == 0) return barGem(BarVariant::OpenHopo, open);
            if (gemColumn <= 5) return gem(GemFamily::Hopo);
            break;
        case Gem::NOTE:
            if (gemColumn == 0) return barGem(BarVariant::Open, open);
            if (gemColumn <= 5) return gem(GemFamily::Note);
            break;
        case Gem::TAP_ACCENT:
            if (gemColumn == 0) return barGem(BarVariant::Open, open);
            if (gemColumn <= 5) return gem(GemFamily::Hopo);
            break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getDrumGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive, bool elite, Part part)
{
    // Use the gem's star power flag to determine if it should be white
    bool shouldBeWhite = starPowerActive && gemWrapper.starPower;

    if (elite)
    {
        // Hi-hat pedal bars: the raised mini kick-bar art in the hi-hat yellow, solid for the
        // Stomp foot click and hollow for the Splash. Identified by gem type, not column; the
        // renderer centres it on the hi-hat lane.
        if (gemWrapper.gem == Gem::STOMP)  return getBarStompImage();
        if (gemWrapper.gem == Gem::SPLASH) return getBarSplashImage();

        // Elite: each hand lane is drum XOR cymbal, and its colour comes from the shared
        // ELITE_LANE_STYLES table (same source the strikeline pads use), so the gem and
        // its pad always match. Kick / 2x-Kick are full-width bars. Purple (Left Crash)
        // has no gem art yet, so it borrows the tap-note glyph as a placeholder until the
        // procedural note glyph lands.
        using T = PositionConstants::DrumLaneTint;
        const auto& palette = paletteFor(part);
        const auto hue = laneHue(gemColumn, part);

        if (gemColumn >= 1 && gemColumn <= 8)
        {
            const auto& style = PositionConstants::ELITE_LANE_STYLES[gemColumn];
            if (style.cymbal)
            {
                if (shouldBeWhite) return getCymWhiteImage();
                switch (style.tint)
                {
                // Hi-Hat (col 2, the only Yellow cymbal lane): the gem art follows the pedal
                // state (spec: Yellow defaults to Open, Closed via coincident pedal-down,
                // Indifferent looks like an ordinary cymbal). Open is the default when no
                // state is set (e.g. before MIDI parsing populates it).
                case T::Yellow:
                    switch (gemWrapper.hihat)
                    {
                    case HiHatState::Closed:      return getCymHiHatClosedImage();
                    case HiHatState::Indifferent: return laneGem(GemFamily::Cymbal, palette.cymbalFor(hue));
                    default:                      return isOpenHiHat(gemWrapper.hihat)
                                                         ? getCymHiHatOpenImage()
                                                         : getCymHiHatClosedImage();
                    }
                case T::Blue:
                case T::Green:
                case T::Red:
                case T::Purple: return laneGem(GemFamily::Cymbal, palette.cymbalFor(hue));
                default:        return getCymWhiteImage();
                }
            }
            // Note lanes: a ghost uses the short HOPO glyph (same cue as 4-lane drums);
            // normal and accent use the full note glyph (accent adds its chevron overlay).
            const bool ghost = (gemWrapper.gem == Gem::HOPO_GHOST);
            if (shouldBeWhite) return ghost ? getHopoWhiteImage() : getNoteWhiteImage();
            if (style.tint == T::None) return ghost ? getHopoWhiteImage() : getNoteWhiteImage();

            // A shell lane: the pad colour, which is also what the strikeline under it reads.
            return laneGem(ghost ? GemFamily::Hopo : GemFamily::Note, palette.padFor(hue));
        }
        // Kick / 2x Kick. Elite kicks carry dynamics (spec), so each dynamic has its own bake:
        // an accent is thicker with a light rim, a ghost is darkened with a near-black one.
        // Star power flattens both to the white bar, which is the point of star power.
        const bool accent = gemWrapper.gem == Gem::TAP_ACCENT || gemWrapper.gem == Gem::CYM_ACCENT;
        const bool ghost  = gemWrapper.gem == Gem::HOPO_GHOST || gemWrapper.gem == Gem::CYM_GHOST;
        if (shouldBeWhite)
            return accent ? getBarWhiteAccentImage() : getBarWhiteEliteImage();
        if (gemColumn == ELITE_KICK_2X_COLUMN)
            return barGem(accent ? BarVariant::Kick2xAccent
                        : ghost  ? BarVariant::Kick2xGhost
                                 : BarVariant::Kick2xElite, palette.kick2x);
        return barGem(accent ? BarVariant::KickAccent
                    : ghost  ? BarVariant::KickGhost
                             : BarVariant::KickElite, palette.kick);
    }

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
        // 4-lane drums, where the lane does not say pad or cymbal: the same yellow lane holds
        // a tom and a hi-hat, so the gem picks which of the two colours the lane has.
        const auto& palette = paletteFor(part);
        const auto hue = laneHue(gemColumn, part);

        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            if (gemColumn >= 1 && gemColumn <= 4) return laneGem(GemFamily::Hopo, palette.padFor(hue));
            break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            if (gemColumn == 0) return barGem(BarVariant::Kick,   palette.kick);
            if (gemColumn == 6) return barGem(BarVariant::Kick2x, palette.kick2x);
            if (gemColumn >= 1 && gemColumn <= 4) return laneGem(GemFamily::Note, palette.padFor(hue));
            break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            if (gemColumn >= 2 && gemColumn <= 4) return laneGem(GemFamily::Cymbal, palette.cymbalFor(hue));
            break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getGridlineImage(Gridline gridlineType)
{
    switch (gridlineType)
    {
    case Gridline::MEASURE: return getMarkerMeasureImage();
    case Gridline::BEAT: return getMarkerBeatImage();
    case Gridline::HALF_BEAT: return getMarkerHalfBeatImage();
    case Gridline::STEP: return getMarkerHalfBeatImage(); // alias — opacity differentiates
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

juce::Image* AssetManager::getTintedGlyph(juce::Image* source, juce::Colour tint, float neutralPull)
{
    if (source == nullptr || !source->isValid()) return source;

    // Pull quantised into the key: a float key would miss on rounding and rebake every frame.
    const auto key = std::make_tuple((const juce::Image*)source, tint.getARGB(),
                                     (int)std::lround(juce::jlimit(0.0f, 1.0f, neutralPull) * 100.0f));
    auto it = tintedGlyphs.find(key);
    if (it == tintedGlyphs.end())
        it = tintedGlyphs.emplace(key, recolorGem(*source, tint, 1.0f, neutralPull)).first;

    return &it->second;
}

// Which hue a lane wears. This is the lane's identity, not its colour: what that hue is
// actually painted comes from the part's palette, which is why nothing here reads a colour.
LaneColours::Hue AssetManager::laneHue(uint gemColumn, Part part)
{
    using T = PositionConstants::DrumLaneTint;
    using H = LaneColours::Hue;

    auto fromTint = [](T t)
    {
        switch (t)
        {
        case T::Red:    return H::Red;
        case T::Yellow: return H::Yellow;
        case T::Blue:   return H::Blue;
        case T::Green:  return H::Green;
        case T::Orange: return H::Orange;
        case T::Purple: return H::Purple;
        default:        return H::White;
        }
    };

    if (part == Part::ELITE_DRUMS)
    {
        if (gemColumn >= 1 && gemColumn <= 8)
            return fromTint(PositionConstants::ELITE_LANE_STYLES[gemColumn].tint);
        return H::Orange;   // kick / 2x-kick lanes read as orange
    }
    if (isGuitarLike(part))
    {
        // 5-fret: col 0 = open (purple), 1-5 = GRYBO. Every guitar-like part uses this
        // layout, and each now answers for its own colours rather than borrowing guitar's.
        static const H g[6] = { H::Purple, H::Green, H::Red, H::Yellow, H::Blue, H::Orange };
        return g[std::min(gemColumn, 5u)];
    }
    // 4-lane drums: kick(orange), red, yellow, blue, green.
    static const H d[5] = { H::Orange, H::Red, H::Yellow, H::Blue, H::Green };
    return d[std::min(drumColumnIndex(gemColumn, part), 4u)];
}

bool AssetManager::eliteLaneIsCymbal(uint gemColumn)
{
    return gemColumn >= 1 && gemColumn <= 8
        && PositionConstants::ELITE_LANE_STYLES[gemColumn].cymbal;
}

// The palette entry behind a column: the dark/bright pair, before anything decides which end
// of it to read. Both colour accessors come through here so they cannot disagree about which
// entry a column belongs to.
const LaneColours::Lane& AssetManager::laneEntry(uint gemColumn, Part part, LaneKind kind) const
{
    const auto& palette = paletteFor(part);

    // A bar is not a lane. The kick, the 2x kick and the open note have palette entries of
    // their own, and reading them off the lane table instead gave the kick whatever colour
    // the ORANGE pad happened to be: paint the toms red and the kick turned red with them,
    // in the note map and anywhere else that asked by column.
    if (isGuitarLike(part))
    {
        if (gemColumn == 0) return palette.open;
    }
    else if (isDrumLike(part))
    {
        const uint twoXColumn = part == Part::ELITE_DRUMS ? (uint)ELITE_KICK_2X_COLUMN
                                                          : (uint)DRUM_KICK_2X_COLUMN;
        if (gemColumn == twoXColumn)             return palette.kick2x;
        if (gemColumn == (uint)DRUM_KICK_COLUMN) return palette.kick;
    }

    // Gems, roll lanes, strikeline pads and effects all come through here, so a lane has one
    // colour whoever is asking. Elite overrules the caller: there the lane itself is a cymbal
    // or a drum, and a caller guessing otherwise would be wrong rather than unaware.
    const auto hue = laneHue(gemColumn, part);
    const bool cymbal = part == Part::ELITE_DRUMS ? eliteLaneIsCymbal(gemColumn)
                                                  : kind == LaneKind::Cymbal;

    return cymbal ? palette.cymbalFor(hue) : palette.padFor(hue);
}

juce::Colour AssetManager::getLaneColour(uint gemColumn, Part part, bool starPowerActive, LaneKind kind)
{
    if (starPowerActive)
    {
        return juce::Colours::white;
    }

    return LaneColours::bright(laneEntry(gemColumn, part, kind));
}

// What something READS as where its gradient cannot be seen. A gem is mostly its bright
// colour, but a bar is a lit tube running from a deep rim to a gloss line, and taking the
// gloss alone made a strip mark that stood for an orange kick come out pale and yellow next
// to the bar itself. The midpoint is what the eye averages the tube to.
juce::Colour AssetManager::getGlanceColour(uint gemColumn, Part part, LaneKind kind)
{
    const auto& lane = laneEntry(gemColumn, part, kind);
    return LaneColours::bright(lane).interpolatedWith(LaneColours::dark(lane), 0.5f);
}
