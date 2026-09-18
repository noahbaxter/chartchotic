/*
    ==============================================================================

        BarGemArt.h
        Author: Noah Baxter

        Procedural bar notes (kick / 2x kick / open / star power). The source
        PDFs use mesh gradients nothing can draw natively, so the tube is
        modeled instead: a constant-height glossy tube whose vertical colour
        ramp was sampled from the original PNGs, bent along a parabolic arc,
        with tilted dark end caps. Verified against assets/generated/bars/.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/LaneColours.h"
#include "../Geometry/PositionConstants.h"

namespace GemArt
{
    // Vertical colour ramp through the tube, top (0) to bottom (1).
    using BarRamp = std::vector<std::pair<float, juce::Colour>>;

    BarRamp barRampWhite();     // star power / SP bar (near-greyscale)
    BarRamp barRampKick();      // orange
    BarRamp barRampKick2x();    // red-orange
    BarRamp barRampOpen();      // purple

    // `thickness` squeezes the tube and its caps about the bar's centreline. The arc, the
    // canvas and the centreline are untouched, so a thin bar curves exactly like a fat one
    // and the gems that sit on it don't move. Elite bakes thin (kEliteBarThickness) because
    // its 9 lanes shrink the gems while a full-width bar keeps 4-lane proportions.
    constexpr float kEliteBarThickness = 0.5f;

    // Elite kick dynamics are the same bar at the same size, told apart the way an open note is
    // told from an open HOPO: a ghost dark-rimmed on a darkened tube, a normal bare, an accent
    // light-rimmed and lifted. Sizing them apart made the set read as three different notes
    // rather than one note played three ways.

    // `centreLineAlpha` > 0 draws that bright line, as a fraction of the tube height.
    constexpr float kAccentLineHeight = 0.16f;
    // How hard that line reads where it is still the accent's only cue (the star power bake).
    constexpr float kAccentCentreLineAlpha = 0.85f;

    // A HOPO bar is the same tube with a bright rim, not a bar in a different colour: an open
    // HOPO has to still read as an open note. The rim is a fraction of the tube's height, so it
    // thins with a thin bake instead of eating the tube.
    constexpr float kBarOutlineWidth = 0.24f;
    // Short of opaque, so the tube's own colour still comes through the rim and it reads as the
    // same bar lit at the edge rather than a white line laid over one.
    constexpr float kBarOutlineAlpha = 0.7f;
    // The rimmed bar also lifts a little overall, so it separates from the plain one at a glance
    // without turning into a different colour.
    constexpr float kBarOutlineBrighten = 0.12f;

    // A ghost rims the other way: a near-black edge on a tube pulled down, so it reads quieter
    // than a plain kick where the accent reads louder than one.
    // Dark against the tube but clearly lighter than the board. At the board's own colour the rim
    // read as a hole rather than an edge: the bar measured full height and looked a third of it,
    // because the eye only counts the coloured core.
    inline const juce::Colour kBarGhostOutlineColour { 0xff4a4a54 };
    constexpr float kBarGhostOutlineAlpha = 0.8f;
    // Enough to read quieter than a plain kick, not enough to lose it against the board. The rim
    // is a fraction of tube height, so on the thin ghost bake it already takes a good share of
    // the bar; darkening what is left as hard as a hand gem made the whole gem disappear.
    constexpr float kBarGhostDarken = 0.22f;
    // Alpha at or above which a baked pixel counts as part of the bar when the rim is traced.
    // Below it is antialiasing, which would otherwise read as its own ragged edge.
    constexpr int kBarOutlineSolidAlpha = 128;

    juce::Image bakeBar(const BarRamp& ramp,
                        juce::Rectangle<int> canvas,
                        juce::Rectangle<int> contentBounds,
                        float thickness = 1.0f,
                        float centreLineAlpha = 0.0f,
                        float outlineAlpha = 0.0f,
                        juce::Colour outlineColour = juce::Colours::white);

    inline juce::Rectangle<int> barCanvas()        { return { 0, 0, 2432, 152 }; }
    inline juce::Rectangle<int> barContentBounds() { return { 86, 0, 2260, 152 }; }

    // Elite Stomp/Splash pedal bar: a flat plane with a raised pseudo-3D box, flat shades (no
    // gradients). Symmetric and parabolically arced so it sits along the highway's curved
    // gridlines. Self-contained (defines its own canvas). `arch` sets the arc depth in content px
    // (caller derives it from the shared curvature constant); `thickness` scales the vertical
    // extent. The content canvas is kStompBarWidth wide, so arch = |curvature| * kStompBarWidth *
    // spanFraction matches the gridline curvature over the bar's span.
    // `face` tints the whole bar: it IS the top-face shade, and the front / end-cap shades are
    // scaled off it. `edgeWidth` > 0 adds a keyline of `edge` around the outer silhouette,
    // leaving the body filled.
    constexpr int kStompBarWidth = 2432;
    juce::Image bakeStompBar(float arch = 0.0f, float thickness = 1.0f,
                             juce::Colour face = juce::Colour(0xffffffff),
                             juce::Colour edge = juce::Colour(0x00000000),
                             float edgeWidth = 0.0f);

    // The hi-hat lane's own yellow. Stomp (the foot click) fills with it flat; Splash keeps the
    // white body and takes it as a gold keyline.
    inline const juce::Colour kStompBarFace = LaneColours::bright(LaneColours::yellow);
    inline const juce::Colour kStompBarBody = juce::Colour(0xffffffff);
    constexpr float kStompBarEdgeWidth = 20.0f;

    // Procedural gridline marker: a thin flat-grey parallelogram (diagonal-cut ends) bowed by
    // the SAME parabola the notes / stomp bar use. The caller derives `arch` from
    // NOTE_CURVATURE_DRUMS over the full board width, so gridlines and the stomp bar curve from
    // ONE source and pixel-match instead of the gridline baking a hand-guessed arc. `thickness`
    // is the bar height in content px; `colour` is the flat fill (grey + per-subdivision alpha);
    // `arch` bows the ends down (centre rises) by that many px. Canvas is kGridlineWidth wide.
    constexpr int kGridlineWidth = 2496;
    constexpr float kGridlineMargin = 89.0f;    // canvas px either side of the bar, matching the old PNGs
    constexpr float kGridlineTopPad = 12.0f;    // headroom above the centre so the bar never clips row 0
    constexpr float kGridlineBottomPad = 12.0f;
    constexpr float kGridlineMeasureThickness = 57.0f;
    constexpr float kGridlineHalfBeatThickness = 33.0f;
    juce::Image bakeGridline(float thickness, juce::Colour colour, float arch);

    // Full-board parabola amplitude (peak-to-edge) in bake px: width * |curvature|, matching
    // getCurvedImage's arcHeight = srcW * curv over the fretboard span.
    inline float gridlineArch()
    {
        return (float) kGridlineWidth * std::abs(PositionConstants::NOTE_CURVATURE_DRUMS);
    }

    inline int gridlineCanvasHeight(float thickness, float arch)
    {
        return (int) std::ceil(kGridlineTopPad + thickness + std::abs(arch) + kGridlineBottomPad);
    }

    // The bar's centreline in canvas px at canvas x. Anything that has to meet a gridline
    // exactly, like a phrase fill, traces this rather than guessing at the bow.
    inline float gridlineCentreY(float canvasX, float thickness, float arch)
    {
        const float half = ((float) kGridlineWidth - 2.0f * kGridlineMargin) * 0.5f;
        const float t = (canvasX - (float) kGridlineWidth * 0.5f) / half;
        return kGridlineTopPad + thickness * 0.5f + arch * t * t;
    }
}
