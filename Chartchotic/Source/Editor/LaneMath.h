/*
    ==============================================================================

        LaneMath.h
        Author: Noah Baxter

    ==============================================================================
*/

#pragma once

#include "AuthoringConfig.h"
#include "../Midi/Utils/GridMath.h"
#include <cstddef>

//==============================================================================
// Roll / trill lane rules, as the .mid specs state them.
//
// Two shapes exist, and they are not variations on each other:
//
//   Shared markers (5-fret guitar, 4- and 5-lane drums) are one pair of pitches for the
//   whole track: 126 is a 1-lane roll / tremolo, 127 a 2-lane roll / trill. The marker
//   carries no column. The game reads the columns off the notes underneath it
//   ("Roll lanes are not inherently specific to any lanes, rather they are applied
//   dynamically to a lane based on the notes that are present, excluding kicks",
//   _refs/midi/drums.md:139). So which of the two pitches to write is derivable from the
//   drag, and where the lane lands is not ours to say.
//
//   Elite drums has one roll-lane pitch per pad (108-118, _refs/midi/elite-drums.md:153-163)
//   and supports arbitrary combinations of them. The column IS the pitch, there is no
//   1-vs-2 variant, and the drag's own column is the whole answer.
//
// TrackResolver::resolveLanes is the read-side twin of this: it derives the drawn columns
// the same way. Keep the two honest with each other.
//
// Neither spec states a minimum length, a snap requirement, or any rule against lanes
// overlapping or abutting, so nothing here enforces one.

namespace LaneMath
{
    // Whether a lane spanning these two points is long enough to write.
    //
    // Deliberately NOT MIDI_MIN_SUSTAIN_LENGTH. That is a third of a quarter note: the shortest
    // note that should read as a sustain rather than a plain gem. Rolls are charted far finer.
    // Plain 16ths are a quarter note apart, and 28 of the 48 step/tuplet settings the write bar
    // offers are shorter still, so borrowing the sustain minimum refused a lane over exactly the
    // runs lanes exist for. Nothing in the .mid specs states a minimum lane length.
    //
    // A lane needs two distinct notes to sit on and nothing beyond that, and since both edges
    // snap to real note positions, one tick apart is the whole rule. That also makes this
    // independent of whether a subdivision divides the tick resolution evenly: septuplets do
    // not (960/7 is not a whole number), but a lane snaps to where the notes actually are
    // rather than to a computed grid, so it never has to.
    inline bool isWritableLength(double startQN, double endQN)
    {
        return GridMath::after(endQN, startQN);
    }

    // One lane unless the drag was pulled sideways across columns, which is the author asking
    // for alternation between pads (_refs/midi/drums.md:141-142). Horizontal travel is the
    // expansion axis, the way dragging a marquee or a cursor selection widens it.
    //
    // Reading it off the notes underneath instead looked clever and was wrong in practice: a
    // plain roll that happened to have a note from a neighbouring column inside its span turned
    // itself into a trill, with nothing the author could do about it short of moving the notes.
    // The drag says what was meant; the notes only say where it lands.
    inline PhraseTarget targetForDragSpan(int columnsCovered)
    {
        return columnsCovered >= 2 ? PhraseTarget::Lane2 : PhraseTarget::Lane1;
    }

    // How many columns a marker of this kind reaches across once written, which is what the
    // preview draws and what TrackResolver reads back.
    inline size_t reachOf(PhraseTarget target)
    {
        return target == PhraseTarget::Lane2 ? 2u : 1u;
    }
}
