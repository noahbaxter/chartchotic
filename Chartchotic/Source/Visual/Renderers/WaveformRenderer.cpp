/*
    ==============================================================================

        WaveformRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "WaveformRenderer.h"
#include "../Geometry/RenderTypeConfig.h"
#include "../../UI/Theme.h"
#include "../../DebugTools/ClipReadLog.h"
#include <array>
#include <algorithm>
#include <cmath>

using namespace PositionConstants;

// The widest neighbourhood smoothing can average over, at amount 1.0. Wide enough to turn a
// spiky waveform into a rolling envelope, short of erasing the shape entirely.
static constexpr int MAX_SMOOTHING_RADIUS = 12;

/*
    Walks a run of points as a CURVE rather than as segments, appending cubic Beziers.

    Smoothing cannot fix a faceted edge, which is worth being clear about because it is the
    obvious thing to reach for and it does not work: smoothing averages the AMPLITUDES, so it
    changes where the points are and leaves the path joining them a polyline either way. Worse,
    a moving average of a spiky signal is locally linear, so turning it up lengthens the straight
    runs. The facets are geometry, so the fix has to be geometry.

    Catmull-Rom, which passes THROUGH every point rather than near it. That matters more here
    than it usually would: the points are transients, and a fit that merely approximates them
    (a quadratic through the midpoints, say, which is the cheaper standard trick) pulls the top
    off every isolated spike, which is exactly the content the view exists to show. This rounds
    the corners between points and moves no point at all.

    Each span is converted to the equivalent cubic: the tangent at a point is half the vector
    between its neighbours, and a Bezier control point sits a third of the way along that
    tangent, so the two control points are the neighbour difference over six. The ends clamp to
    themselves, which makes the first and last tangents one-sided rather than wrapping onto a
    neighbour that is not there.
*/
template <typename PointAt>
static void curveThrough(juce::Path& path, int count, bool startNewSubPath, PointAt pointAt)
{
    if (count <= 0) return;

    // Clamped rather than wrapped, so asking past either end gives the end point itself.
    auto at = [&](int n) { return pointAt(juce::jlimit(0, count - 1, n)); };

    if (startNewSubPath) path.startNewSubPath(at(0));
    else                 path.lineTo(at(0));

    for (int n = 0; n < count - 1; ++n)
    {
        const auto p0 = at(n - 1), p1 = at(n), p2 = at(n + 1), p3 = at(n + 2);
        path.cubicTo(p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
    }
}

void WaveformRenderer::emphasiseTransients(Row* rows, int rowCount, float amount,
                                           double groupSeconds)
{
    amount = juce::jlimit(0.0f, 1.0f, amount);
    if (amount <= 0.0f || rows == nullptr || groupSeconds <= 0.0) return;

    const int lookbackRows =
        juce::jmax(1, (int)std::round((WAVEFORM_ONSET_LOOKBACK_MS / 1000.0) / groupSeconds));

    // Per-row decay that adds up to the same tail in milliseconds however many rows that is.
    const float release =
        (float)std::exp(-groupSeconds / (WAVEFORM_ONSET_RELEASE_MS / 1000.0));

    // How far each row rises above the moving average of the audio just before it. Measured
    // against a short run rather than the single previous row, which would call every bit of
    // sample-to-sample jitter an onset and light the whole board up.
    std::array<float, Waveform::MAX_ROWS + 1> rise;

    // Running sum over the trailing window rather than re-adding it per row: at high Detail
    // with a short view the window is tens of rows wide, and the naive form turns into a few
    // hundred thousand adds a frame for an answer that only moves by one value at each step.
    float trailingSum = 0.0f;
    int   trailingCount = 0;

    for (int i = 0; i <= rowCount; ++i)
    {
        const float mean = trailingCount > 0 ? trailingSum / (float)trailingCount
                                             : rows[i].envelope;

        rise[(size_t)i] = juce::jmax(0.0f, rows[i].envelope - mean);

        trailingSum += rows[i].envelope;
        ++trailingCount;

        if (trailingCount > lookbackRows)
        {
            trailingSum -= rows[i - lookbackRows].envelope;
            --trailingCount;
        }
    }

    /*
        A DECISION about each moment, then that decision held. Not a proportional scale, which
        is what the last two attempts did and why the slider only ever dimmed the whole picture.

        Scaling by how much a moment rose ducks everything, because in a dense mix nothing
        rises by much: the envelope is already loud when the next hit lands, so every rise is a
        small fraction and every row gets multiplied down. Twice now that produced "ducks
        EVERYTHING, slightly unevenly", which is exactly what the arithmetic says it must do.

        So the rise is run through a threshold. Above it, a row is an attack and keeps its
        height, whatever that height is. Below it, a row is steady state and gets ducked.
        Transients stay at their natural volume and only the held content moves, which is what
        the slider should have been doing all along.

        The threshold is in absolute amplitude. It used to be a share of the biggest rise ON
        SCREEN, which quietly made the answer for every row depend on what else was in view:
        one loud hit scrolling onto the board rescaled the emphasis of everything already
        there, so the shape morphed as it travelled. The envelope is already normalised to
        full scale by shapeMagnitude, so a fixed threshold is a fixed decision about the audio.

        The decision then decays forward rather than applying to one row alone: an attack is
        instant and a note rings out after it, so a hit reads as a spike with a tail instead of
        an eight-millisecond sliver.
    */
    std::array<float, Waveform::MAX_ROWS + 1> attack;

    for (int i = 0; i <= rowCount; ++i)
    {
        attack[(size_t)i] = juce::jmap(juce::jlimit(WAVEFORM_ONSET_LOW, WAVEFORM_ONSET_HIGH,
                                                    rise[(size_t)i]),
                                       WAVEFORM_ONSET_LOW, WAVEFORM_ONSET_HIGH, 0.0f, 1.0f);
    }

    for (int i = 1; i <= rowCount; ++i)
        attack[(size_t)i] = juce::jmax(attack[(size_t)i], attack[(size_t)i - 1] * release);

    for (int i = 0; i <= rowCount; ++i)
    {
        const float keep = juce::jmap(amount, 1.0f, attack[(size_t)i]);

        rows[i].leftMag  *= keep;
        rows[i].rightMag *= keep;
    }
}

void WaveformRenderer::smoothRows(Row* rows, int rowCount, float amount)
{
    const int radius = (int)std::round(juce::jlimit(0.0f, 1.0f, amount) * MAX_SMOOTHING_RADIUS);
    if (radius <= 0 || rows == nullptr) return;

    // Against a copy, so each row averages its neighbours' original values rather than the
    // already-smoothed ones behind it, which would smear the waveform backwards up the board.
    // Uninitialised for the same reason as the caller's: the copy fills everything read below.
    std::array<Row, Waveform::MAX_ROWS + 1> source;
    std::copy(rows, rows + rowCount + 1, source.begin());

    for (int i = 0; i <= rowCount; ++i)
    {
        const int from = juce::jmax(0, i - radius);
        const int to   = juce::jmin(rowCount, i + radius);

        float left = 0.0f, right = 0.0f, envelope = 0.0f;
        for (int j = from; j <= to; ++j)
        {
            left     += source[(size_t)j].leftMag;
            right    += source[(size_t)j].rightMag;
            envelope += source[(size_t)j].envelope;
        }

        const float count = (float)(to - from + 1);
        rows[i].leftMag  = left / count;
        rows[i].rightMag = right / count;
        // Kept in step, because transient emphasis runs after this and reads the envelope. A
        // stale one would have it hunting attacks in the signal it no longer draws.
        rows[i].envelope = envelope / count;
    }
}

void WaveformRenderer::populate(DrawCallMap& drawCallMap, const Waveform::Peaks& peaks,
                                double windowStartTime, double windowEndTime,
                                double windowOriginTime,
                                uint width, uint height, float posEnd,
                                float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    setFrame(activePart, width, height, posEnd, farFadeEnd, farFadeLen, farFadeCurve);

    const double windowTimeSpan = windowEndTime - windowStartTime;
    if (windowTimeSpan <= 0.0) return;

    // How far up the board to draw. NOT posEnd: that is the fixed reference the perspective is
    // built against (HIGHWAY_POS_END), while farFadeEnd is the highway length the user set, so
    // at 250% the board keeps going long after posEnd. GridlineRenderer.cpp:86 culls against
    // farFadeEnd for the same reason. Stopping at posEnd left the waveform ending a third of
    // the way up a long highway.
    const float drawEnd = juce::jmax(posEnd, farFadeEnd);

    if (drawEnd <= HIGHWAY_POS_START) return;

    const auto renderType = getRenderType(activePart);

    const auto& tuning = Waveform::tuning();

    /*
        Sampled in the audio's own time, then projected onto the board. NOT the other way
        round, which is what made the waveform crawl and shimmer while scrolling.

        Walking fixed screen rows and asking each one "what audio is here" means every row
        covers a slightly different slice of audio each frame as the playhead moves. The slice
        boundaries slide across the cache's buckets, so each row picks up a different set of
        peaks frame to frame and the shape morphs in place instead of travelling. Classic
        aliasing: the cache holds 1ms detail and the board has a few hundred rows to show it
        with, so the excess has to land somewhere, and it landed in the motion.

        So the groups are anchored to the project's clock instead. A group is a whole number of
        cache buckets starting at a multiple of its own length from time zero, which means a
        given group always covers exactly the same audio and always reports the same amplitude.
        The playhead moving changes only where that group is drawn. The shape travels up the
        board without changing, which is what scrolling is supposed to look like.
    */
    const double viewOrigin = windowOriginTime + windowStartTime;
    const double viewStart = viewOrigin + (double)HIGHWAY_POS_START * windowTimeSpan;
    const double viewEnd   = viewOrigin + (double)drawEnd * windowTimeSpan;

    // Straight from the requested duration, in whole buckets, and nothing else feeds into it.
    // Deriving it from how much time was on screen was the last of the jitter: the window's
    // length in SECONDS changes whenever the tempo does, since the board shows a fixed number
    // of beats, so the group size drifted with the tempo and the drifting edges covered
    // different audio each frame. Taken from a duration, the boundaries are pinned to the
    // project's clock and a tempo change cannot move them.
    int bucketsPerGroup = juce::jmax(1, (int)std::round((double)tuning.resolutionMs
                                                        * Waveform::PEAKS_PER_SECOND / 1000.0));

    // Coarsen rather than truncate if that many points will not fit. Clamping the count instead
    // would draw the bottom of the board and leave the top empty. The off-board context counts
    // against the budget too, since it is sampled like any other row.
    //
    // Note this decision cannot change while scrolling: the span it is taken from is
    // displayWindowTimeSeconds (PluginEditor.cpp:1670), a fixed number of SECONDS set by the
    // speed slider, so a tempo change does not move it and cannot flip the group size underneath
    // a scroll.
    double groupSeconds = (double)bucketsPerGroup * Waveform::SECONDS_PER_PEAK;
    auto rowsNeeded = [&](double seconds)
    {
        return (viewEnd - viewStart) / seconds + 2.0 * std::ceil(WAVEFORM_CONTEXT_SECONDS / seconds);
    };

    while (rowsNeeded(groupSeconds) > (double)Waveform::MAX_ROWS - 1.0)
    {
        bucketsPerGroup *= 2;
        groupSeconds = (double)bucketsPerGroup * Waveform::SECONDS_PER_PEAK;
    }

    // Sampled past both ends of the board so that smoothing and onset detection, which both
    // answer for a row from its neighbours, have the same neighbourhood for every row they are
    // asked about. Without it a row's width depended on how close it was to the edge of the
    // buffer, which changes as it scrolls, so the shape reformed on the way up the board.
    const int marginRows = (int)std::ceil(WAVEFORM_CONTEXT_SECONDS / groupSeconds);

    const long long onScreen = (long long)std::ceil((viewEnd - viewStart) / groupSeconds) + 1;
    const long long firstGroup = (long long)std::floor(viewStart / groupSeconds) - marginRows;
    const int rowCount = (int)juce::jlimit(2LL, (long long)Waveform::MAX_ROWS,
                                           onScreen + 2LL * marginRows);

    // The slice of the buffer that actually reaches the board. Everything outside it exists
    // only so the rows inside it have context.
    const int firstDrawn = juce::jlimit(0, rowCount, marginRows);
    const int lastDrawn  = juce::jlimit(firstDrawn, rowCount, marginRows + (int)onScreen);

    // The board's edge is 0dBFS. Nothing is scaled to fit: a full-scale sample reaches the edge
    // and anything past it is clipped by the width clamp, which is the honest picture of a mix
    // that is over. Normalising to the loudest moment was the alternative and it cost more than
    // it gave, since it needed the whole source read to know what that moment was, and it made
    // a quiet passage look loud.
    const float gain = tuning.gain;

    // A stereo source draws a side each; anything else draws its one channel both ways.
    const int rightChannel = peaks.channelCount() > 1 ? 1 : 0;

    // The board's half-width at the strikeline, which the outline's width is measured against:
    // a width set in pixels means pixels THERE, and thins from there into the distance.
    const auto strikeEdge = PositionMath::getFretboardEdge(renderType, 0.0f, width, height,
                                                           HIGHWAY_POS_START, posEnd);
    const float strikeHalf = (strikeEdge.rightX - strikeEdge.leftX) * 0.5f;

    // Deliberately not value-initialised: at the maximum row count that is a 40KB memset per
    // highway per frame for values the loop below overwrites before anything reads them.
    std::array<Row, Waveform::MAX_ROWS + 1> rows;

    float  loudestDrawn = 0.0f;
    double queryStart = 0.0, queryEnd = 0.0;

    for (int i = 0; i <= rowCount; ++i)
    {
        // The group's own fixed span on the project's clock. Nothing here depends on where the
        // playhead is, which is the point.
        const double rowStart = (double)(firstGroup + i) * groupSeconds;
        const double rowEnd   = rowStart + groupSeconds;

        // Where that lands on the board. The exact inverse of what every other renderer does
        // (GridlineRenderer.cpp:86 maps time to position as (t - windowStartTime) /
        // windowTimeSpan), so the waveform and the gridlines agree. Position is NOT 0..1: the
        // board runs from HIGHWAY_POS_START (-0.3, behind the strikeline) to drawEnd, and
        // normalising it into that range would slide the audio against the notes.
        //
        // Taken from the group's midpoint so the drawn point sits in the middle of the audio
        // it describes rather than at its leading edge.
        const float position = (float)((rowStart + groupSeconds * 0.5 - viewOrigin)
                                       / windowTimeSpan);

        const auto edge = PositionMath::getFretboardEdge(renderType, position, width, height,
                                                         HIGHWAY_POS_START, posEnd);
        const float centreX = (edge.leftX + edge.rightX) * 0.5f;
        const float halfWidth = (edge.rightX - edge.leftX) * 0.5f * tuning.width;

        // Extremes over the row's own span, so nothing between two rows is skipped and a
        // transient cannot fall through the gap. Past what the cache has built, over() answers
        // flat and the shape collapses to the centre line on its own.
        //
        // A channel a side, which is the layout this was designed against: an editor's clip
        // view rotated so time runs up the board, then the two lanes folded together about the
        // centre line. What the BACKLOG ruled out is a different thing that looks similar,
        // |peak| of one MERGED signal drawn both ways, which is symmetric by construction and
        // therefore two mirrored blobs rather than a waveform. Left and right are genuinely
        // different data. On a correlated mix they will still read near-mirrored, and that is
        // the mix being correlated, not the drawing lying.
        const auto left  = peaks.over(0, rowStart, rowEnd);
        const auto right = peaks.over(rightChannel, rowStart, rowEnd);

        // Peak, not RMS. RMS draws a smooth rounded mass and the serrated edge of a peak
        // envelope is what actually makes an attack findable; the old split of an RMS body
        // inside a peak outline had the outline tracing a signal the shape was not made of.
        const float leftMag  = Waveform::shapeMagnitude(left.magnitude() * gain,
                                                        tuning.floorDb, tuning.curve);
        const float rightMag = Waveform::shapeMagnitude(right.magnitude() * gain,
                                                        tuning.floorDb, tuning.curve);

        // Taken from the board's own width at this depth rather than from a second perspective
        // solve, since the edge is already here.
        const float boardHalf = (edge.rightX - edge.leftX) * 0.5f;

        rows[(size_t)i] = { centreX, halfWidth, edge.centerY, leftMag, rightMag,
                            // Energy, not peak, is what transient emphasis reads: a lone
                            // sample spiking says nothing about whether a note started. Free
                            // here, since over() returned it alongside the extremes.
                            Waveform::shapeMagnitude((left.rms + right.rms) * 0.5f * gain,
                                                     tuning.floorDb, tuning.curve),
                            strikeHalf > 0.0f ? boardHalf / strikeHalf : 1.0f };

        // Reported over the drawn slice rather than the whole buffer, so the log says what is
        // on the board rather than including the context rows either side of it.
        if (i >= firstDrawn && i <= lastDrawn)
            loudestDrawn = juce::jmax(loudestDrawn, juce::jmax(leftMag, rightMag));

        if (i == firstDrawn) queryStart = rowStart;
        if (i == lastDrawn)  queryEnd   = rowEnd;
    }

    // Smoothing first, emphasis second. The other way round was wrong in a way that showed:
    // smoothing a spike spreads it in BOTH directions, so every attack grew a ramp leading
    // into it and read as a fade-in rather than a hit. Smoothing the envelope first denoises
    // what the detector looks at, and the sharp edge it then produces is the last thing to
    // happen, so an attack starts where it starts.
    smoothRows(rows.data(), rowCount, tuning.smoothing);
    emphasiseTransients(rows.data(), rowCount, tuning.transients, groupSeconds);

    // The one line that makes a flat waveform diagnosable: what this frame asked the cache for
    // against what the cache actually holds. Overlapping ranges mean the cache is the problem;
    // disjoint ones mean the view is reading somewhere the audio is not.
    if (ClipReadLog::due(lastLogMs, LOG_INTERVAL_MS))
        ClipReadLog::audio("drawing " + ClipReadLog::range(queryStart, queryEnd)
                           + " | cache holds " + ClipReadLog::range(peaks.firstTime(),
                                                                    peaks.builtUntil())
                           + " | gain " + juce::String(gain, 2)
                           + " | loudest drawn " + juce::String(loudestDrawn, 4)
                           // Whether the two sides are actually different data. Asked of the
                           // SAMPLES, not of the track's channel count, which only says how the
                           // audio is stored: a stereo file carrying identical channels is mono
                           // content and draws symmetric no matter what the split does.
                           + " | " + juce::String(peaks.channelCount()) + "ch "
                           + (peaks.isStereo() ? "L/R split" : "mono, mirrored")
                           + (peaks.isEmpty() ? "   CACHE EMPTY"
                              : loudestDrawn <= 0.0f ? "   NOTHING IN RANGE" : ""));

    /*
        One shape, filled solid: up the left channel's edge and back down the right channel's,
        closed. No outline and no centre line, because the reference this was designed against
        has neither. What reads as an outline in an editor IS the edge of the fill, and drawing
        a separate stroke over the top was how the two ended up tracing different signals.

        A vertex per row costs no detail, since at these row counts a vertex is finer than the
        screen can show.
    */
    const int drawnCount = lastDrawn - firstDrawn + 1;

    // Where each edge of the shape sits for a given row. Both the fill and the outline are built
    // from these two, so neither can trace a different signal than the other.
    auto leftEdge = [&](int i)
    {
        const auto& row = rows[(size_t)i];
        return juce::Point<float>(row.centreX - row.halfWidth * row.leftMag, row.y);
    };

    auto rightEdge = [&](int i)
    {
        const auto& row = rows[(size_t)i];
        return juce::Point<float>(row.centreX + row.halfWidth * row.rightMag, row.y);
    };

    juce::Path wave;
    // A cubic carries three points where a line carried one.
    wave.preallocateSpace(drawnCount * 2 * 7 + 8);

    curveThrough(wave, drawnCount, true,
                 [&](int n) { return leftEdge(firstDrawn + n); });
    curveThrough(wave, drawnCount, false,
                 [&](int n) { return rightEdge(lastDrawn - n); });
    wave.closeSubPath();

    /*
        The outline, tracing the fill's OWN two edges. This is the whole correction: the outline
        used to trace the peak while the fill was drawn from RMS, so it was the boundary of a
        shape that was never on screen and visibly did not match what it outlined. Both now come
        from the same leftMag and rightMag, so the outline is the edge of its own shape by
        construction and cannot drift from it.

        A filled ribbon rather than a stroke, because a stroke has ONE width and the board does
        not: a constant-width line is too heavy at the far end, where the whole board is a few
        pixels across, and too light at the strikeline where everything around it is full size.
        Each side runs up its own left edge and back down its right, taking its half-width per
        row from that row's taper. Both sides go into one path, so the outline is a single fill.
    */
    juce::Path outline;

    if (tuning.outlineWidth > 0.0f)
    {
        outline.preallocateSpace(drawnCount * 4 * 7 + 16);

        // Each side of the ribbon is the edge itself, offset by half the width at that row's
        // depth. Curved the same way as the fill, or the outline would facet against a smooth
        // edge and read worse than no outline at all.
        auto appendRibbon = [&](auto edgeAt)
        {
            auto offsetEdge = [&](int i, float direction)
            {
                auto point = edgeAt(i);
                point.x += direction * tuning.outlineWidth * rows[(size_t)i].taper * 0.5f;
                return point;
            };

            curveThrough(outline, drawnCount, true,
                         [&](int n) { return offsetEdge(firstDrawn + n, -1.0f); });
            curveThrough(outline, drawnCount, false,
                         [&](int n) { return offsetEdge(lastDrawn - n, 1.0f); });
            outline.closeSubPath();
        };

        appendRibbon(leftEdge);
        appendRibbon(rightEdge);
    }

    // Flat opacity, NOT the scene's depth fade. The fade reaches zero at farFadeEnd, and a
    // gradient stretched from the strikeline to there dissolves the waveform the entire way up
    // rather than only in the fade zone: the far half washes out and the layer reads as though
    // it stops early. Audio is reference material, not scenery, so it stays as legible at the
    // top of the board as at the bottom. Opacity is the control for that.
    const auto fillColour = juce::Colour(Theme::waveform).withAlpha(tuning.opacity);
    const auto outlineColour = juce::Colour(Theme::waveform).withAlpha(tuning.outlineOpacity);

    // execute walks each bucket back to front (SceneRenderer.cpp:339), so the FIRST call pushed
    // is drawn LAST and ends up on top: the outline over the shape it is the edge of.
    if (!outline.isEmpty())
        drawCallMap[(int)DrawOrder::WAVEFORM][0].push_back(
            [outline = std::move(outline), outlineColour](juce::Graphics& g)
            {
                g.setColour(outlineColour);
                g.fillPath(outline);
            });

    drawCallMap[(int)DrawOrder::WAVEFORM][0].push_back(
        [wave = std::move(wave), fillColour](juce::Graphics& g)
        {
            g.setColour(fillColour);
            g.fillPath(wave);
        });
}
