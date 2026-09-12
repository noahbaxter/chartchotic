/*
    ==============================================================================

        StripViewMath.h
        Author: Noah Baxter

        The utility strip's view window, as plain arithmetic. Kept out of the
        component so the two parts worth arguing about, what counts as a legal
        view and how hard a chart edge pushes back, can be tested without a
        Graphics context.

    ==============================================================================
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace StripViewMath
{

// Where the bottom of the view sits and how much of the project it spans. A length rather
// than a rung on a ladder: the strip is a window on the whole timeline, and a chart is a
// region it may happen to sit inside.
struct View
{
    double startQN = 0.0;
    double spanQN = 0.0;
};

// Held inside the project and nothing narrower. The only bound a view has.
inline View clamp(double startQN, double spanQN, double projectEndQN, double minSpanQN)
{
    double span = std::min(std::max(spanQN, minSpanQN), std::max(minSpanQN, projectEndQN));
    double start = std::min(std::max(startQN, 0.0), std::max(0.0, projectEndQN - span));
    return { start, span };
}

// The zoom rungs, in quarter notes, narrowest first.
//
// 2 and 4 bars, then every power of two from 8 with its halfway point: 8 12 16 24 32 48 64.
// Doubling is a fine step while the window is small enough to be editing in, and too coarse
// once it is wide enough to be navigating with. Round bar counts because the footer reads
// them out and because landing back where you were should take the same notches every time.
//
// One ladder for the whole project, not one per chart: a ladder whose ends moved with
// whichever chart you were in is what made zoom a mode with somewhere to get stuck.
inline std::vector<double> zoomSpansQN(double projectEndQN, double barQN, double minBars)
{
    std::vector<double> spans;
    if (barQN <= 0.0 || projectEndQN <= 0.0) return { std::max(projectEndQN, 0.0) };

    auto push = [&](double bars)
    {
        if (bars < minBars) return;
        double span = bars * barQN;
        if (span < projectEndQN) spans.push_back(span);
    };

    push(2.0);
    push(4.0);
    for (double p = 8.0; p * barQN < projectEndQN; p *= 2.0) { push(p); push(p * 1.5); }

    // Zooming all the way out lands exactly on the whole project rather than on some rung
    // of mostly empty timeline past the end of it.
    spans.push_back(projectEndQN);
    return spans;
}

// The next rung in `direction`. Tolerant of a span that is not on the ladder at all, which
// is the normal case straight after fitting a chart: it steps to the next rung that way
// rather than snapping sideways first and eating the notch.
inline double nextSpanQN(const std::vector<double>& spans, double current, int direction)
{
    if (spans.empty()) return current;

    const double eps = 1.0e-6;

    if (direction > 0)
    {
        for (double s : spans)
            if (s > current * (1.0 + eps)) return s;

        return spans.back();
    }

    for (auto it = spans.rbegin(); it != spans.rend(); ++it)
        if (*it < current * (1.0 - eps)) return *it;

    return spans.front();
}

// Zoom that holds one point still, which is what makes it feel like a map rather than a
// jump. `anchor` is 0 at the bottom of the view and 1 at the top.
inline View spanAbout(View v, double anchor, double spanQN,
                      double projectEndQN, double minSpanQN)
{
    double span = std::min(std::max(spanQN, minSpanQN), std::max(minSpanQN, projectEndQN));
    double held = v.startQN + anchor * v.spanQN;
    return clamp(held - anchor * span, span, projectEndQN, minSpanQN);
}

// The grid step in bars, given how many pixels a bar is worth. The finest step that still
// leaves `minGapPx` between lines, so the grid gets denser as you zoom in and thins out as
// you zoom out, and never becomes a wash either way.
//
// Values below 1 are subdivisions of a bar. Snapping follows whatever is drawn, so this is
// also the finest position the strip can place anything at: floored at a whole bar, a section
// on beat 3 was not expressible at any zoom.
inline double gridStepBars(double pxPerBar, double minGapPx)
{
    for (double step : { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 })
        if (step * pxPerBar >= minGapPx) return step;

    return 128.0;
}

// A stretch of timeline with something in it.
struct NoteSpan
{
    double startQN = 0.0;
    double endQN = 0.0;
};

// Runs of notes with no silence longer than `gapQN` between them, collapsed into one span
// each. What "some stuff out around here" means when it is written down: a scrap of MIDI off
// on its own is worth stopping at, and the eight notes inside it are not each worth stopping
// at. Input must be sorted by start, which is the order cached notes already arrive in.
//
// The gap, not the run length, is what separates two clusters, so a sparse intro stays one
// thing instead of fragmenting into a stop per note.
inline std::vector<NoteSpan> clusterSpans(const std::vector<NoteSpan>& sorted, double gapQN)
{
    std::vector<NoteSpan> out;

    for (const auto& s : sorted)
    {
        if (s.endQN < s.startQN) continue;

        if (!out.empty() && s.startQN - out.back().endQN <= gapQN)
        {
            out.back().endQN = std::max(out.back().endQN, s.endQN);
            continue;
        }
        out.push_back(s);
    }
    return out;
}

// Sorted, with anything landing together kept once. Two things at the same moment are one
// place to stop, not two notches that look like the wheel stuttered.
inline std::vector<double> mergedPoints(std::vector<double> points, double tolerance)
{
    std::sort(points.begin(), points.end());

    std::vector<double> out;
    for (double p : points)
        if (out.empty() || p - out.back() > tolerance) out.push_back(p);

    return out;
}

// Friction when leaving something you are inside.
//
// Resting zeroes the bank, so from a standstill it always takes the same two quick notches
// to get out, and a lone notch after a pause re-arms instead of springing through. A "you
// already tried once" flag would be the opposite: state you can walk away from and be
// surprised by ten seconds later.
struct Detent
{
    double push = 0.0;
    double heldStop = 0.0;
    long long lastMs = 0;

    void clear() { push = 0.0; }

    // Counted rather than measured. A snap step is one notch whatever distance it covers, so
    // banking the distance would clear any threshold instantly across a wide gap: what has to
    // be counted is how many times you asked, not how far the ask was.
    double resolveCounted(double want, double stop, int notches,
                          long long nowMs, int resetMs)
    {
        if (nowMs - lastMs > resetMs) push = 0.0;
        lastMs = nowMs;

        if (std::abs(stop - heldStop) > 1.0e-9) { heldStop = stop; push = 0.0; }

        push += 1.0;
        if (push < (double)notches) return stop;

        push = 0.0;
        return want;
    }

};

}
