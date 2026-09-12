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

// Friction at a chart edge.
//
// Pressure rather than a count. Travel that would carry the view out of the chart it sits in
// is held at the edge and banked; only once enough has banked does the view cross. Resting
// zeroes the bank, so from a standstill it always takes the same two quick notches to get
// out, and a lone notch after a pause re-arms the edge instead of springing through it. A
// "you already tried once" flag would be the opposite: state you can walk away from and be
// surprised by ten seconds later.
struct BoundaryDetent
{
    double pushQN = 0.0;
    double heldEdgeQN = 0.0;
    long long lastPushMs = 0;

    // `songStartQN`/`songEndQN` are the chart the view is currently sitting in. Answers the
    // start the view should actually take.
    double resolve(double wantStartQN, double spanQN,
                   double songStartQN, double songEndQN,
                   long long nowMs, int resetMs, double thresholdSpan)
    {
        if (nowMs - lastPushMs > resetMs) pushQN = 0.0;
        lastPushMs = nowMs;

        // Wider than the chart already, so the view straddles both edges no matter what and
        // a detent would only be friction against nothing.
        if (spanQN >= songEndQN - songStartQN) return wantStartQN;

        double edge = 0.0;
        if (wantStartQN < songStartQN)             edge = songStartQN;
        else if (wantStartQN + spanQN > songEndQN) edge = songEndQN - spanQN;
        else { pushQN = 0.0; return wantStartQN; }

        // A different edge from the one being leaned on means the bank belongs somewhere
        // else, so it starts again here rather than carrying over and firing instantly.
        if (std::abs(edge - heldEdgeQN) > 1.0e-9) { heldEdgeQN = edge; pushQN = 0.0; }

        pushQN += std::abs(wantStartQN - edge);
        if (pushQN < spanQN * thresholdSpan) return edge;

        pushQN = 0.0;
        return wantStartQN;
    }
};

}
