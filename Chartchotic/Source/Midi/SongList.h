#pragma once

#include <JuceHeader.h>
#include "Providers/TrackNoteProvider.h"   // defines ClipSpan
#include "../UI/ControlConstants.h"        // defines trackNameFor
#include "Utils/GridMath.h"
#include <algorithm>

/**
 * The songs in a project. One MIDI item on the EVENTS track is one song.
 *
 * Items may overlap, and that is the point: a gapless album is twelve per-song items plus
 * one item spanning all of them, so the singles and the album chart come off the same notes.
 * Overlap costs nothing because selection is explicit, never derived from the playhead.
 *
 * Design: docs/superpowers/specs/2026-09-11-song-boundaries-design.md
 */
class SongList
{
public:
    struct Song
    {
        double startQN = 0.0;
        double endQN = 0.0;
        juce::String name;
    };

    // The two zoom rungs that are views rather than bar counts.
    static constexpr int ZOOM_SESSION = -1;   // every chart on the timeline
    static constexpr int ZOOM_SONG    = 0;    // the whole of one chart

    static constexpr int BAR_STEPS[] = { 8, 16, 32, 64, 128, 256 };
    static constexpr int DEFAULT_ZOOM_BARS = 64;

    // The rungs a song of `songQN` quarter notes offers, narrowest first, where its bars are
    // `barQN` long. A bar step only earns a rung if it shows less than the whole song: 128
    // bars of a 40 bar chart is the song view with extra travel to reach it, not a rung.
    static std::vector<int> zoomLadder(double songQN, double barQN)
    {
        std::vector<int> rungs;

        for (int bars : BAR_STEPS)
            if (bars * barQN < songQN) rungs.push_back(bars);

        rungs.push_back(ZOOM_SONG);
        rungs.push_back(ZOOM_SESSION);
        return rungs;
    }

    // Unmerged on purpose, unlike InstrumentSession's clipSpans: merging is what would
    // destroy the album-over-singles arrangement this exists to support.
    void buildFromSpans(const std::vector<ClipSpan>& spans)
    {
        songs.clear();

        for (const auto& span : spans)
            if (span.endQN > span.startQN)
                songs.push_back({ span.startQN, span.endQN, span.name });

        std::sort(songs.begin(), songs.end(),
                  [](const Song& a, const Song& b) { return a.startQN < b.startQN; });

        // Resolved once here rather than per paint, since the picker repaints with the strip.
        for (size_t i = 0; i < songs.size(); ++i)
            if (isUntitled(songs[i].name))
                songs[i].name = "Chart " + juce::String((int)i + 1);
    }

    const std::vector<Song>& getSongs() const { return songs; }
    int size() const { return (int)songs.size(); }

    const Song* at(int index) const
    {
        return index >= 0 && index < (int)songs.size() ? &songs[(size_t)index] : nullptr;
    }

    // The song beginning at `qn`. Start is the handle a song keeps across the re-parse an
    // edit triggers, where an index shifts and a pointer dangles.
    const Song* startingAt(double qn) const
    {
        for (const auto& song : songs)
            if (GridMath::same(song.startQN, qn)) return &song;

        return nullptr;
    }

    // Slide `[start, end]` so it sits inside the song, keeping its length. Shifted rather
    // than truncated: a truncated window compresses the strip against the song's edges, so
    // the same zoom stops meaning the same distance. A song shorter than the window is the
    // window, since there is nothing left to slide against.
    static void confineWindow(double& start, double& end, double songStart, double songEnd)
    {
        double span = end - start;

        if (songEnd - songStart <= span)
        {
            start = songStart;
            end = songEnd;
            return;
        }

        if (start < songStart)   start = songStart;
        else if (end > songEnd)  start = songEnd - span;

        end = start + span;
    }

private:
    // Nothing anyone chose. Blank is the obvious case; "EVENTS" is the one REAPER hands you,
    // since a new item takes the name of the track it lands on, so every untouched song
    // would otherwise be called EVENTS and none of them would be told apart.
    static bool isUntitled(const juce::String& name)
    {
        auto trimmed = name.trim();
        return trimmed.isEmpty()
            || trimmed.equalsIgnoreCase(trackNameFor(Part::EVENTS));
    }

    std::vector<Song> songs;
};
