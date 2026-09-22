/*
    ==============================================================================

        WaveformTuning.h
        Author: Noah Baxter

        The waveform's appearance, as live values rather than constants, so it
        can be dialled in against real audio instead of guessed at and rebuilt.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DrawingConstants.h"

namespace Waveform
{
    /*
        One shared set of values rather than one per highway: four highways showing four
        differently-tuned waveforms is not a thing anyone wants, and it keeps the panel from
        having to know how many highways exist.

        Message thread only. The panel writes here and the renderer reads it, and both run
        on the message thread already.
    */
    struct Tuning
    {
        // How much time one drawn point covers, in milliseconds. Lower is finer detail and more
        // geometry per frame; this is the one value that actually costs something.
        //
        // Set as a duration rather than as a point count on purpose. The point count depends on
        // how much time is on screen, which changes with zoom AND with tempo, so asking for a
        // count meant the real resolution drifted underneath it. Asking for a duration pins the
        // group boundaries to the project's clock and nothing shifts them.
        float resolutionMs = WAVEFORM_RESOLUTION_MS;

        // Pulls the drawing from "all the audio" toward "only what is rising". A held chord
        // and the snare over it can be the same height, which is the case where a waveform
        // stops helping; at 1 the held part falls away and the hits are what is left.
        float transients = WAVEFORM_TRANSIENTS;

        // Blends each row's amplitude with its neighbours. 0 is raw and is the default, because
        // a readable attack is a vertical wall and any smoothing at all turns it into a ramp.
        // Here for a noisy source, not for normal use.
        float smoothing = WAVEFORM_SMOOTHING;

        // How far a full-scale peak reaches from the centre, as a fraction of the half-board.
        float width = WAVEFORM_WIDTH_FRACTION;

        // Straight multiplier on the samples. The board's edge is 0dBFS, so 1.0 draws the audio
        // at its real level and anything over full scale is clipped at the edge rather than the
        // picture being rescaled to accommodate it.
        float gain = 1.0f;

        // The fill's opacity. The outline has its own below, deliberately not derived from this
        // one: dropping the fill back so the notes read over it is exactly when the edge has to
        // stay solid.
        float opacity = WAVEFORM_OPACITY;

        // Where the drawing calls it silence, in dB. Pulled toward zero it drops the body of
        // the mix and leaves the peaks standing alone, which is what makes an attack findable;
        // pushed down it shows everything, room noise included.
        float floorDb = WAVEFORM_FLOOR_DB;

        // Bends what survives the floor. Above 1 the quiet end shrinks faster than the loud
        // end, steepening the difference between a hit and the mix underneath it.
        float curve = 1.0f;

        // The outline's width in pixels at the strikeline, thinning into the distance from
        // there. It traces the fill's OWN edge, so it is the boundary of the shape it outlines
        // rather than a second signal drawn over the top, which is what was wrong before.
        float outlineWidth = WAVEFORM_OUTLINE_WIDTH;

        float outlineOpacity = WAVEFORM_OUTLINE_OPACITY;
    };

    inline Tuning& tuning()
    {
        static Tuning t;
        return t;
    }

    // The most rows the renderer will ever be asked for, so its row buffer can be a fixed
    // array rather than an allocation every frame. Past roughly one row per cache bucket in
    // view there is nothing left to resolve, and every row costs a perspective solve, so this
    // is a ceiling rather than a target.
    inline constexpr int MAX_ROWS = 2048;
}
