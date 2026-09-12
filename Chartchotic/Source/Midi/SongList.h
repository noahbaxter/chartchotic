#pragma once

#include <JuceHeader.h>
#include "SongSections.h"
#include "Providers/TrackNoteProvider.h"   // defines ClipSpan
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

    // No song picked, so nothing is confined. The default, and where every project with
    // fewer than two EVENTS items stays.
    static constexpr int SESSION = -1;

    // Unmerged on purpose, unlike InstrumentSession's clipSpans: merging is what would
    // destroy the album-over-singles arrangement this exists to support.
    void buildFromSpans(const std::vector<ClipSpan>& spans, const SongSections& sections)
    {
        songs.clear();

        for (const auto& span : spans)
            if (span.endQN > span.startQN)
                songs.push_back({ span.startQN, span.endQN, span.name });

        std::sort(songs.begin(), songs.end(),
                  [](const Song& a, const Song& b) { return a.startQN < b.startQN; });

        // Resolved once here rather than per paint, since the picker repaints with the strip.
        for (size_t i = 0; i < songs.size(); ++i)
            if (songs[i].name.isEmpty())
                songs[i].name = fallbackName(songs[i], sections, i);
    }

    const std::vector<Song>& getSongs() const { return songs; }
    int size() const { return (int)songs.size(); }

    // The picker costs a row of vertical space, which one song does not justify.
    bool isPickable() const { return songs.size() >= 2; }

    const Song* at(int index) const
    {
        return index >= 0 && index < (int)songs.size() ? &songs[(size_t)index] : nullptr;
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
    static juce::String fallbackName(const Song& song, const SongSections& sections, size_t index)
    {
        for (const auto& s : sections.getSections())
        {
            double qn = s.position.toDouble();
            if (qn < song.startQN) continue;
            if (qn >= song.endQN) break;
            return s.name;
        }

        return "Song " + juce::String((int)index + 1);
    }

    std::vector<Song> songs;
};
