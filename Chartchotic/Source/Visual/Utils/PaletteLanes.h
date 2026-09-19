#pragma once

#include <JuceHeader.h>
#include <vector>
#include "LaneColours.h"
#include "../Geometry/PositionConstants.h"
#include "../../UI/ControlConstants.h"

// What a part's palette offers to edit, in board order left to right.
//
// Labelled by what the lane IS, never by what colour it currently wears: the whole point of
// the picker is that the colour changes, and a swatch called "Yellow" that has been painted
// red is a lie. Elite's three toms share the orange slot, so they are one entry: the palette
// holds a colour per hue, not per lane, and pretending otherwise would offer three swatches
// that silently overwrite each other.
namespace PaletteLanes
{
    // Which entry of the palette a swatch edits. Kick and 2x kick are their own entries
    // rather than hues: they are bars, not lanes, and nothing else shares their colour.
    enum class Slot { Pad, Cymbal, Kick, Kick2x, Open };

    struct Lane
    {
        juce::String label;
        LaneColours::Hue hue;   // ignored by the Kick slots
        Slot slot;
    };

    inline std::vector<Lane> forPart(Part part)
    {
        using H = LaneColours::Hue;
        using S = Slot;

        if (part == Part::ELITE_DRUMS)
            return { { "Snare",       H::Red,    S::Pad    },
                     { "Toms",        H::Orange, S::Pad    },
                     { "Hi-hat",      H::Yellow, S::Cymbal },
                     { "Left crash",  H::Purple, S::Cymbal },
                     { "Ride",        H::Blue,   S::Cymbal },
                     { "Right crash", H::Green,  S::Cymbal },
                     { "Kick",        H::Orange, S::Kick   },
                     { "2x kick",     H::Orange, S::Kick2x } };

        if (part == Part::DRUMS)
            return { { "Snare",   H::Red,    S::Pad    },
                     { "Tom 1",   H::Yellow, S::Pad    },
                     { "Tom 2",   H::Blue,   S::Pad    },
                     { "Tom 3",   H::Green,  S::Pad    },
                     { "Hi-hat",  H::Yellow, S::Cymbal },
                     { "Ride",    H::Blue,   S::Cymbal },
                     { "Crash",   H::Green,  S::Cymbal },
                     { "Kick",    H::Orange, S::Kick   },
                     { "2x kick", H::Orange, S::Kick2x } };

        if (isGuitarLike(part))
            return { { "Fret 1", H::Green,  S::Pad },
                     { "Fret 2", H::Red,    S::Pad },
                     { "Fret 3", H::Yellow, S::Pad },
                     { "Fret 4", H::Blue,   S::Pad },
                     { "Fret 5", H::Orange, S::Pad },
                     { "Open",   H::Purple, S::Open } };

        return {};
    }

    inline const LaneColours::Lane& colourOf(const LaneColours::Palette& palette, const Lane& lane)
    {
        switch (lane.slot)
        {
        case Slot::Cymbal: return palette.cymbalFor(lane.hue);
        case Slot::Kick:   return palette.kick;
        case Slot::Kick2x: return palette.kick2x;
        case Slot::Open:   return palette.open;
        case Slot::Pad:    break;
        }
        return palette.padFor(lane.hue);
    }

    inline void setColourOf(LaneColours::Palette& palette, const Lane& lane,
                            const LaneColours::Lane& colour)
    {
        switch (lane.slot)
        {
        case Slot::Cymbal: palette.cymbal[(int)lane.hue] = colour; return;
        case Slot::Kick:   palette.kick   = colour;                return;
        case Slot::Kick2x: palette.kick2x = colour;                return;
        case Slot::Open:   palette.open   = colour;                return;
        case Slot::Pad:    palette.pad[(int)lane.hue] = colour;    return;
        }
    }
}
