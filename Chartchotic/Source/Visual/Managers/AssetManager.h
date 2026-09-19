/*
    ==============================================================================

        AssetManager.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <tuple>
#include <utility>
#include "../../Utils/ChartTypes.h"
#include "../Utils/LaneColours.h"
#include "../Utils/PartPalettes.h"

class AssetManager
{
public:
    AssetManager();
    ~AssetManager();

    // The three gem families that carry a lane colour. Each is one greyscale master, and a
    // colour is a bake off it rather than art of its own.
    enum class GemFamily { Note, Hopo, Cymbal };

    // A gem in a colour, baked on first ask and kept. Keyed by the colour itself, not by the
    // lane or the part, so two parts that agree on green share one bake and a part painted
    // entirely in one colour costs a single bake for its whole board.
    juce::Image* laneGem(GemFamily family, const LaneColours::Lane& lane);

    // The full-width bars that carry a colour: kick, 2x kick, and the open note. Same
    // on-demand, colour-keyed treatment as the gems, but baked and scaled board-wide.
    enum class BarVariant { Kick, Kick2x, Open, OpenHopo,
                            KickElite, Kick2xElite,
                            KickAccent, Kick2xAccent, KickGhost, Kick2xGhost };
    juce::Image* barGem(BarVariant variant, const LaneColours::Lane& lane);

    // Pre-scale all drawable assets to match the current viewport width.
    // Call on window resize. Skips work if width hasn't changed.
    void rescaleForWidth(int viewportWidth);

    // The unscaled original behind a live asset, or null for one that is generated rather than
    // loaded. Anything drawn OUTSIDE the highway wants this: rescaleForWidth swaps the live
    // images out from under their pointers as the viewport changes, so a control drawn from a
    // live one changes size whenever the chart is resized, which is not the control's business.
    const juce::Image* masterFor(const juce::Image* live) const;

    // Image picker methods. `part` decides which palette the colours come from, so two parts
    // asking for the same lane can get different gems.
    juce::Image* getGuitarGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive,
                                     Part part = Part::GUITAR);
    juce::Image* getDrumGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive,
                                   bool elite = false, Part part = Part::DRUMS);
    juce::Image* getGridlineImage(Gridline gridlineType);
    juce::Image* getOverlayImage(Gem gem, Part part);

    // What a lane is being drawn AS. There is no default on purpose: a drum lane holds a tom
    // and a cymbal at the same column in two different colours, and every caller that was
    // allowed to stay quiet about which one it meant got the tom. The gem flash, the
    // activation swell, the activation gem and the note map each learned that separately.
    enum class LaneKind { Pad, Cymbal };

    // The only thing that actually knows: elite reads its lane, 4-lane drums read the gem,
    // and a guitar lane is never a cymbal.
    static LaneKind kindOf(Part part, uint gemColumn, Gem gem)
    {
        if (part == Part::ELITE_DRUMS)
            return eliteLaneIsCymbal(gemColumn) ? LaneKind::Cymbal : LaneKind::Pad;
        if (!isDrumLike(part)) return LaneKind::Pad;
        return isCymbalGem(gem) ? LaneKind::Cymbal : LaneKind::Pad;
    }

    // Prefer this. Anything drawn FOR A NOTE (its gem, its flare, its swell, its mark on the
    // map) has a gem in hand, and the gem is the answer, so there is nothing to remember.
    juce::Colour getNoteColour(uint gemColumn, Part part, Gem gem, bool starPowerActive)
    {
        return getLaneColour(gemColumn, part, starPowerActive, kindOf(part, gemColumn, gem));
    }

    // For the few things drawn for a LANE rather than a note, where no gem exists to ask:
    // the strikeline pad under a column, or a map mark resolved from MIDI markers alone.
    juce::Colour getLaneColour(uint gemColumn, Part part, bool starPowerActive, LaneKind kind);

    // For marks too small to show a gradient, like the strip's note map: the colour the thing
    // reads as overall rather than the brightest point of it.
    juce::Colour getGlanceColour(uint gemColumn, Part part, LaneKind kind);

    // The colours this part is drawn with, from the process-wide store. Nothing has to be
    // told a palette changed: the cache is keyed by the colour itself, so an edited lane asks
    // for a colour that simply has no bake yet and the old entry is left alone rather than
    // freed under a pointer the renderer is still holding.
    const LaneColours::Palette& paletteFor(Part part) const
    {
        return PartPalettes::global().forPart(part);
    }

    // Which hue a lane wears, and whether that lane is a cymbal in its own right (elite only).
    // Shared with the strikeline so pads and gems cannot disagree about what a lane is.
    static LaneColours::Hue laneHue(uint gemColumn, Part part);
    static bool eliteLaneIsCymbal(uint gemColumn);

    // Any glyph, recoloured to any colour. neutralPull decides how far the gem's neutral pixels
    // -- its white highlights and framing -- follow: 0 leaves them, which is how ordinary gems
    // keep their highlights, and 1 takes the whole sprite. Built on first use and cached per
    // (source, colour, pull), because it is a full per-pixel pass and must never run per frame.
    juce::Image* getTintedGlyph(juce::Image* source, juce::Colour tint, float neutralPull);

    // Bar/Open notes. The coloured ones name the default palette's entry; a part's own kick
    // and open colours come through barGem from the glyph getters, which know the part.
    juce::Image* getBarKickImage()   { return barGem(BarVariant::Kick,   LaneColours::kick); }
    juce::Image* getBarKick2xImage() { return barGem(BarVariant::Kick2x, LaneColours::kick2x); }
    juce::Image* getBarOpenImage()   { return barGem(BarVariant::Open,   LaneColours::openBar); }
    juce::Image* getBarWhiteImage() { return &barWhiteImage; }
    juce::Image* getBarKickEliteImage()   { return barGem(BarVariant::KickElite,   LaneColours::kick); }
    juce::Image* getBarKick2xEliteImage() { return barGem(BarVariant::Kick2xElite, LaneColours::kick2x); }
    juce::Image* getBarWhiteEliteImage()  { return &barWhiteEliteImage; }
    juce::Image* getBarKickAccentImage()   { return barGem(BarVariant::KickAccent,   LaneColours::kick); }
    juce::Image* getBarKick2xAccentImage() { return barGem(BarVariant::Kick2xAccent, LaneColours::kick2x); }
    juce::Image* getBarKickGhostImage()    { return barGem(BarVariant::KickGhost,    LaneColours::kick); }
    juce::Image* getBarKick2xGhostImage()  { return barGem(BarVariant::Kick2xGhost,  LaneColours::kick2x); }
    juce::Image* getBarWhiteAccentImage()  { return &barWhiteAccentImage; }
    // Elite hi-hat pedal bars, drawn on cols 10 / 11.
    juce::Image* getBarStompImage() { return &barStompImage; }
    juce::Image* getBarSplashImage() { return &barSplashImage; }

    // Cymbal notes. The colours are bakes off the metallic master, so these name the default
    // palette's entry rather than an image of their own; white keeps its PNG.
    juce::Image* getCymBlueImage() { return laneGem(GemFamily::Cymbal, LaneColours::blue); }
    juce::Image* getCymGreenImage() { return laneGem(GemFamily::Cymbal, LaneColours::green); }
    juce::Image* getCymRedImage() { return laneGem(GemFamily::Cymbal, LaneColours::red); }
    juce::Image* getCymWhiteImage() { return &cymWhiteImage; }
    juce::Image* getCymYellowImage() { return laneGem(GemFamily::Cymbal, LaneColours::yellow); }
    juce::Image* getCymPurpleImage() { return laneGem(GemFamily::Cymbal, LaneColours::purple); }
    // Elite hi-hat: extracted gold art used as-is (not tinted), replaces the yellow cym on the Hi-Hat lane.
    juce::Image* getCymHiHatClosedImage() { return &cymHiHatClosedImage; }
    juce::Image* getCymHiHatOpenImage() { return &cymHiHatOpenImage; }

    // HOPO notes
    juce::Image* getHopoBarOpenImage() { return barGem(BarVariant::OpenHopo, LaneColours::openBar); }
    juce::Image* getHopoBarWhiteImage() { return &barWhiteImage; }  // TODO: create unique asset
    juce::Image* getHopoBlueImage() { return laneGem(GemFamily::Hopo, LaneColours::blue); }
    juce::Image* getHopoGreenImage() { return laneGem(GemFamily::Hopo, LaneColours::green); }
    juce::Image* getHopoOrangeImage() { return laneGem(GemFamily::Hopo, LaneColours::orange); }
    juce::Image* getHopoRedImage() { return laneGem(GemFamily::Hopo, LaneColours::red); }
    juce::Image* getHopoWhiteImage() { return &hopoWhiteImage; }
    juce::Image* getHopoYellowImage() { return laneGem(GemFamily::Hopo, LaneColours::yellow); }
    juce::Image* getHopoPurpleImage() { return laneGem(GemFamily::Hopo, LaneColours::purple); }

    // Lane graphics
    juce::Image* getLaneEndImage() { return &laneEndImage; }
    juce::Image* getLaneMidImage() { return &laneMidImage; }
    juce::Image* getLaneStartImage() { return &laneStartImage; }

    // Marker graphics
    juce::Image* getMarkerBeatImage() { return &markerBeatImage; }
    juce::Image* getMarkerHalfBeatImage() { return &markerHalfBeatImage; }
    juce::Image* getMarkerMeasureImage() { return &markerMeasureImage; }
    // Write-mode boosted variants — alpha amplified at load time so write
    // mode anchors render at full opacity through the same sprite path.
    juce::Image* getMarkerMeasureWriteImage() { return &markerMeasureWriteImage; }
    juce::Image* getMarkerBeatWriteImage()    { return &markerBeatWriteImage; }

    // Regular notes
    juce::Image* getNoteBlueImage() { return laneGem(GemFamily::Note, LaneColours::blue); }
    juce::Image* getNoteGreenImage() { return laneGem(GemFamily::Note, LaneColours::green); }
    juce::Image* getNoteOrangeImage() { return laneGem(GemFamily::Note, LaneColours::orange); }
    juce::Image* getNoteRedImage() { return laneGem(GemFamily::Note, LaneColours::red); }
    juce::Image* getNoteWhiteImage() { return &noteWhiteImage; }
    juce::Image* getNoteYellowImage() { return laneGem(GemFamily::Note, LaneColours::yellow); }
    juce::Image* getNotePurpleImage() { return laneGem(GemFamily::Note, LaneColours::purple); }

    // Ghost cursor blanks (write-mode hover preview)
    juce::Image* getNoteBlankImage() { return &noteBlankImage; }
    juce::Image* getHopoBlankImage() { return &hopoBlankImage; }
    juce::Image* getCymBlankImage()  { return &cymBlankImage; }
    juce::Image* getBarBlankImage()  { return &barBlankImage; }
    juce::Image* getGhostCursorImage(bool isDrums, int lane);

    // Overlay graphics
    juce::Image* getOverlayCymAccentImage() { return &overlayCymAccentImage; }
    juce::Image* getOverlayCymGhostImage() { return &overlayCymGhostImage; }
    juce::Image* getOverlayNoteAccentImage() { return &overlayNoteAccentImage; }
    juce::Image* getOverlayNoteGhostImage() { return &overlayNoteGhostImage; }
    juce::Image* getOverlayNoteTapImage() { return &overlayNoteTapImage; }

    // Sustain graphics (open only — colored sustains drawn flat by SustainRenderer)
    juce::Image* getSustainOpenWhiteImage() { return &sustainOpenWhiteImage; }
    juce::Image* getSustainOpenImage() { return &sustainOpenImage; }

    // Indicator graphics
    juce::Image* getDiscoBallImage() { return &discoBallImage; }

    // Hit animation graphics
    juce::Image* getHitAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 5) return &hitAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    // Star-power flare is the untinted greyscale "smoke" master itself.
    juce::Image* getHitFlareWhiteImage() { return &hitFlareWhite; }
    // Guitar tap flare — the shared purple.
    juce::Image* getHitFlarePurpleImage();
    // Per-lane flare: tint the one greyscale smoke master by the lane's colour
    // (getLaneColour is part- and lane-count-generic), so any highway lights up
    // in its own colour with no per-lane art. Cached per colour.
    juce::Image* getHitFlareImage(uint gemColumn, Part part, Gem gem);
    juce::Image* getKickAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &kickAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    juce::Image* getOpenAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &openAnimationFrames[frameNumber - 1];
        return nullptr;
    }

    // The bar hit flash in the part's own colour. Unlike the gems, this art is a colour PNG
    // rather than a greyscale master, so a recoloured bar gets the frame re-hued instead of
    // rebaked. A part still on the stock colour gets the untouched art back, so nothing that
    // has not been recoloured can drift.
    juce::Image* getBarHitFrame(int frameNumber, Part part, bool open, bool twoX)
    {
        auto* base = open ? getOpenAnimationFrame(frameNumber) : getKickAnimationFrame(frameNumber);
        if (base == nullptr) return nullptr;

        const auto& palette = paletteFor(part);
        const auto& live  = open ? palette.open : (twoX ? palette.kick2x : palette.kick);
        const auto& stock = open ? LaneColours::openBar
                                 : (twoX ? LaneColours::kick2x : LaneColours::kick);

        if (live.bright == stock.bright) return base;
        return getTintedGlyph(base, LaneColours::bright(live), 0.0f);
    }

private:
    void initAssets();

    // Downscale helper: returns src unchanged if already smaller than targetWidth
    static juce::Image downscale(const juce::Image& src, int targetWidth);

    // How wide an asset is ever actually drawn, as a fraction of the viewport. Scaling
    // everything to one size made every gem blit resample a source many times larger than
    // its destination, and JUCE's cost scales with the SOURCE, not the destination: on elite
    // that was ~230us per gem against ~36us from a lane-sized source. Bars really do span the
    // board, so they keep a full-width master and stay sharp.
    enum class AssetWidthClass
    {
        FullBoard,   // kick / open / star-power bars: drawn edge to edge
        Lane,        // gems, overlays, hit flashes: never wider than one lane's gem box
    };

    // Full-resolution originals (kept for re-scaling on window resize)
    struct ScalableAsset
    {
        juce::Image* target;   // pointer to the active (scaled) member
        juce::Image fullRes;   // original full-res copy
        AssetWidthClass widthClass = AssetWidthClass::Lane;
    };
    std::vector<ScalableAsset> scalableAssets;
    int lastScaledWidth = 0;

    // Bar/Open notes
    // The coloured bars are bakes in barGems, not members: see getBarKickImage above.
    juce::Image barWhiteImage;
    juce::Image barWhiteEliteImage;
    juce::Image barWhiteAccentImage;
    juce::Image barStompImage;           // Elite hi-hat pedal bar, solid yellow (col 10)
    juce::Image barSplashImage;          // Elite hi-hat pedal bar, white body + gold keyline (col 11)

    // Tinted glyph cuts, keyed by the glyph they came from, the colour applied, and the pull.
    std::map<std::tuple<const juce::Image*, juce::uint32, int>, juce::Image> tintedGlyphs;

    // The greyscale masters every lane colour is baked from, kept at full resolution: a bake
    // for a colour nobody has asked for yet has to come from somewhere.
    juce::Image noteMaster, hopoMaster, cymMaster;

    // Baked lane gems, keyed by { family, bright colour }. A node-based map, so a pointer
    // handed out stays good as later colours are added in the same frame. Each entry keeps
    // its own full-res bake beside the scaled copy that actually gets drawn, so the cache
    // takes the same width-class treatment as the fixed assets instead of side-stepping it
    // and blitting from an oversized source.
    const LaneColours::Lane& laneEntry(uint gemColumn, Part part, LaneKind kind) const;

    struct LaneGem { juce::Image fullRes, scaled; };
    std::map<std::pair<int, juce::uint32>, LaneGem> laneGems;
    std::map<std::pair<int, juce::uint32>, LaneGem> barGems;   // board-wide, so scaled differently

    // Colours nobody is drawing any more just sit there, so the cache is trimmed at the one
    // moment every pointer into it is being replaced anyway: a resize.
    static constexpr size_t laneGemCacheLimit = 64;

    // One answer to how wide a lane-class source should be, so a gem baked mid-session lands
    // at the same size as one rescaled on resize.
    static int laneWidthFor(int viewportWidth) { return std::max(200, viewportWidth / 3); }
    static int fullBoardWidthFor(int viewportWidth) { return std::max(200, viewportWidth / 2); }

    // Cymbal notes
    juce::Image cymWhiteImage;
    juce::Image cymHiHatClosedImage;     // Extracted gold hi-hat art (elite Hi-Hat lane), used as-is
    juce::Image cymHiHatOpenImage;       // Extracted open hi-hat; hooks in once open/closed parsing lands

    // HOPO notes. Only the star power glyph is a PNG; every colour is a bake off hopoMaster.
    juce::Image hopoWhiteImage;

    // Lane graphics
    juce::Image laneEndImage;
    juce::Image laneMidImage;
    juce::Image laneStartImage;

    // Marker graphics
    juce::Image markerBeatImage;
    juce::Image markerHalfBeatImage;
    juce::Image markerMeasureImage;
    juce::Image markerMeasureWriteImage;
    juce::Image markerBeatWriteImage;

    // Returns a copy of `src` with each pixel's alpha amplified to fully
    // saturate (any non-zero alpha → 255). Preserves RGB. Used to build the
    // write-mode marker variants without touching the source PNGs.
    static juce::Image makeAlphaBoostedCopy(const juce::Image& src);

    // Regular notes. Same as the HOPOs: white is the SP PNG, the colours are bakes.
    juce::Image noteWhiteImage;

    // Ghost cursor blanks
    juce::Image noteBlankImage;
    juce::Image hopoBlankImage;
    juce::Image cymBlankImage;
    juce::Image barBlankImage;

    // Overlay graphics
    juce::Image overlayCymAccentImage;
    juce::Image overlayCymGhostImage;
    juce::Image overlayNoteAccentImage;
    juce::Image overlayNoteGhostImage;
    juce::Image overlayNoteTapImage;

    // Sustain graphics (open only)
    juce::Image sustainOpenWhiteImage;
    juce::Image sustainOpenImage;

    // Indicator graphics
    juce::Image discoBallImage;

    // Hit animation graphics
    juce::Image hitAnimationFrames[5];   // hit_1.png through hit_5.png
    juce::Image hitFlareWhite;           // greyscale "smoke" master (was hit_flare_white)
    // Lane-colour -> tinted smoke flare, generated on first use and cleared on
    // rescale. Keyed by the lane colour's ARGB so any colour (preset or custom)
    // is handled with no per-lane asset.
    std::map<juce::uint32, juce::Image> flareTintCache;
    juce::Image* flareTinted(juce::Colour colour);
    juce::Image kickAnimationFrames[7];  // hit_kick_1.png through hit_kick_7.png
    juce::Image openAnimationFrames[7];  // hit_open_1.png through hit_open_7.png
};
