/*
    ==============================================================================

        WaveformRenderer.h
        Author:  Noah Baxter

        The audio drawn onto the highway: an editor's clip view stood on end so
        time runs up the board, with the left channel filling out from the
        centre one way and the right channel the other, foreshortened with the
        board.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Audio/WaveformPeaks.h"
#include "../../Utils/ChartTypes.h"
#include "../Geometry/PositionConstants.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/WaveformTuning.h"
#include "HighwayRenderer.h"

class WaveformRenderer : public HighwayRenderer
{
public:
    // Reads an already-built peak cache and never asks for samples: the whole point of the
    // cache is that the frame does no audio work. Emits one draw call, whatever the chart
    // holds, so the layer costs the same in a blast beat as in silence.
    //
    // windowOriginTime is the absolute project second that windowStartTime stands for. The
    // chart's own times are playhead-relative, but the peak cache is keyed to the project's
    // clock, so every lookup here is shifted by it. Without that the view reads the opening
    // seconds of the project no matter where the cursor is.
    void populate(DrawCallMap& drawCallMap, const Waveform::Peaks& peaks,
                  double windowStartTime, double windowEndTime, double windowOriginTime,
                  uint width, uint height, float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    // One horizontal slice of the shape: where its centre sits on screen, how far the board
    // reaches at that depth, and how much of that reach each channel fills. Amplitude is kept
    // apart from geometry so smoothing can work on the audio without touching the perspective.
    struct Row
    {
        float centreX, halfWidth, y;
        // Peak magnitude per channel: left of the centre is channel 0, right of it is channel
        // 1. Not a signed trace mirrored about the centre, and not |peak| of one merged signal
        // painted both ways, which is symmetric by construction and says nothing. A mono source
        // does draw symmetric, and for a mono source that is the truth.
        float leftMag, rightMag;
        // What this row is worth as a single number, kept so transient emphasis has an
        // envelope to work on without having to pick a side. Energy rather than peak, because a
        // lone sample spiking says nothing about whether a note started.
        float envelope;
        // This row's share of the board's width at the strikeline. The outline is scaled by it,
        // so it thins into the distance the way the lanes and gems around it already do. A
        // stroke cannot do this: a stroke has ONE width, which on a perspective board is too
        // heavy at the far end and too light at the near one, so the outline is a filled ribbon.
        float taper;
    };

    // Blends each row's amplitude into its neighbours. Amplitude only: smoothing the screen
    // positions instead would bend the waveform off the perspective it sits on.
    static void smoothRows(Row* rows, int rowCount, float amount);

    // Pulls the waveform toward showing only what is rising. A sustained chord and the snare
    // over it can be the same height, which is exactly the case where the waveform stops
    // helping: both read as "loud" and neither says where the hit is. An attack is a jump
    // above what came just before it, so that jump is what this keeps.
    //
    // amount 0 leaves the waveform alone; 1 leaves only the attacks. groupSeconds is how much
    // time one row covers, which is what turns the lookback and the release from milliseconds
    // into rows: both have to be fixed in time, or changing Detail would redefine what counts
    // as an attack and how long one lasts.
    static void emphasiseTransients(Row* rows, int rowCount, float amount, double groupSeconds);

    // A frame's worth of diagnostics is the same as the last frame's, so the readout is a
    // once-a-second sample rather than 60 identical lines.
    static constexpr double LOG_INTERVAL_MS = 1000.0;
    double lastLogMs = 0.0;
};
