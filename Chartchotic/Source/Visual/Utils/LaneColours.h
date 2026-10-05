/*
    ==============================================================================

        LaneColours.h
        Author: Noah Baxter

        Single source of truth for the gameplay lane colours (the note/pad hues:
        red, yellow, blue, green, orange, purple, white). Every consumer that
        needs a lane colour, the strikeline pads (TrackRenderer) and the gem
        tints (AssetManager), reads from here, so a colour changes in ONE place.

        Each lane is a { dark, bright } pair (ARGB): the pad gradient runs dark
        (top) -> bright (bottom), and gem tint ramps use the same pair.

        Every lane-tinted gem is now rebuilt from a greyscale master against these
        values (AssetManager), so there is no PNG holding a colour of its own and
        nothing here has to stay matched to baked art.

        The constants are the DEFAULTS. What actually gets drawn is a Palette,
        which a part can hold its own copy of, so two parts can disagree about
        what green is.

    ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace LaneColours
{
    struct Lane { juce::uint32 dark, bright; };

    constexpr Lane red    { 0xFFA91E1A, 0xFFEB1C22 };
    constexpr Lane yellow { 0xFF987F0B, 0xFFFFD800 };
    constexpr Lane blue   { 0xFF0D447F, 0xFF1678E4 };
    constexpr Lane green  { 0xFF226D2C, 0xFF36B047 };   // softer/less-saturated (reverted from the deepened 145A20/1FA838, which read too neon on the metallic cymbals)
    constexpr Lane orange { 0xFF8A4A08, 0xFFE97500 };   // purer, less amber (was A85A14/E88A20)
    constexpr Lane purple { 0xFF5B2A8C, 0xFFAF3EE6 };   // bright = vivid open-bar purple
    constexpr Lane white  { 0xFFAEAEAE, 0xFFEAEAEA };

    // Kick / 2x-kick bar tubes: bright is the gloss line along the top, dark is the rim.
    // The kick is barRampKick's own rim and gloss, so a default install draws the yellow tube
    // the art was made as. The 2x is a plain orange, far enough round the wheel that the two
    // cannot be mistaken: as two shades of one orange they could not be told apart on a board.
    constexpr Lane kick   { 0xFFF0650A, 0xFFFEF81D };   // kick bar: yellow gloss, orange rim
    constexpr Lane kick2x { 0xFFD04A08, 0xFFFF7A14 };   // 2x-kick: orange
    // The open bar's own purple, which is NOT the purple gem's: the bar art runs far brighter
    // than the gem hue. Sharing one entry made the open bar darker than its art and tied it to
    // elite's left crash, so recolouring either moved the other.
    constexpr Lane openBar { 0xFF7200D3, 0xFFFD93FE };

    inline juce::Colour dark   (const Lane& l) { return juce::Colour(l.dark); }
    inline juce::Colour bright (const Lane& l) { return juce::Colour(l.bright); }

    // The hues a lane can be, in one order that everything indexes by. DrumLaneTint over in
    // PositionConstants says which hue an elite lane wears; it maps onto this and is not a
    // second palette.
    enum class Hue { Red, Yellow, Blue, Green, Orange, Purple, White };
    inline constexpr int hueCount = 7;

    // What a part is actually drawn with. Cymbals get their own entry per hue: a yellow
    // hi-hat and a yellow tom are two colours, and recolouring the cymbal leaves the pad
    // under it alone. The strikeline reads `pad` for every lane, cymbal lanes included,
    // which is what keeps a cymbal recolour self-contained.
    struct Palette
    {
        Lane pad[hueCount];
        Lane cymbal[hueCount];
        Lane kick, kick2x, open;

        const Lane& padFor   (Hue h) const { return pad[(int)h]; }
        const Lane& cymbalFor(Hue h) const { return cymbal[(int)h]; }

        bool operator==(const Palette& o) const
        {
            for (int i = 0; i < hueCount; ++i)
                if (pad[i].dark != o.pad[i].dark || pad[i].bright != o.pad[i].bright
                    || cymbal[i].dark != o.cymbal[i].dark || cymbal[i].bright != o.cymbal[i].bright)
                    return false;
            return kick.dark == o.kick.dark && kick.bright == o.kick.bright
                && kick2x.dark == o.kick2x.dark && kick2x.bright == o.kick2x.bright
                && open.dark == o.open.dark && open.bright == o.open.bright;
        }
        bool operator!=(const Palette& o) const { return !(*this == o); }
    };

    // Cymbals start from the same hues as the pads. The metallic master's green is darkened
    // at bake time rather than here, since that is a property of the art, not of the colour.
    constexpr Palette defaultPalette()
    {
        return Palette{ { red, yellow, blue, green, orange, purple, white },
                        { red, yellow, blue, green, orange, purple, white },
                        kick, kick2x, openBar };
    }
}

// Structural / neutral render colours (highway backing, strikeline pad bevels, side
// rails). Kept here so every colour in the renderer lives in ONE place — no inline
// juce::Colour literals scattered through the drawing code. The juce::Colour is built
// once, here; consumers just name the constant.
namespace TrackColours
{
    inline const juce::Colour highwayFill     = juce::Colour(0xFF111111);  // highway backing fill
    inline const juce::Colour sheenWhite      = juce::Colour(0x40FFFFFF);  // translucent white sheen
    inline const juce::Colour debugPoly       = juce::Colour(0xFFFF0000);  // debug poly-shade overlay

    // Strikeline pad bevel (TrackRenderer FretColour gradient stops)
    inline const juce::Colour padBevelInner   = juce::Colour(0xFF8C8C8C);
    inline const juce::Colour padBevelOuter   = juce::Colour(0xFF606060);
    inline const juce::Colour padBevelDark    = juce::Colour(0xFF1A1A1A);
    inline const juce::Colour padFallbackDark = juce::Colour(0xFF888888);  // unmapped lane
    inline const juce::Colour padFallbackLite = juce::Colour(0xFFBBBBBB);

    // Side rail bands (ProceduralTrackArt)
    inline const juce::Colour railInner       = juce::Colour(0xFF404040);
    inline const juce::Colour railLine        = juce::Colour(0xFF606060);
    inline const juce::Colour railBlack       = juce::Colour(0xFF060606);

    // Procedural gridline markers (flat grey bar). Alpha differs per subdivision, matching the
    // old baked marker PNGs so the renderer's per-type opacity multipliers stay tuned. STEP
    // reuses the half-beat bake (opacity differentiates it in GridlineRenderer).
    inline const juce::Colour gridlineMeasure  = juce::Colour(0xBFA6A6A6);
    inline const juce::Colour gridlineBeat     = juce::Colour(0x80A6A6A6);
    inline const juce::Colour gridlineHalfBeat = juce::Colour(0x80A6A6A6);
}
