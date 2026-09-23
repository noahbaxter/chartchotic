#pragma once

#include <JuceHeader.h>
#include "Providers/TrackNoteProvider.h"   // defines ClipSpan
#include "../UI/ControlConstants.h"        // defines trackNameFor
#include "Utils/GridMath.h"
#include <algorithm>

/**
 * The songs in a project. One MIDI item on the EVENTS track is one song.
 *
 * Items MAY NOT OVERLAP. One position is in at most one song, so which song you are in is a
 * fact about where the playhead is rather than something to choose, and the feature carries no
 * selection state at all. `UtilityStripComponent::songAt` depends on this directly: it returns
 * the first match and stops.
 *
 * An earlier draft allowed overlap, so that a gapless album could be twelve per-song items plus
 * one spanning all of them. That bought one arrangement and cost a selection model, column
 * packing in the overview, and a rule about which of two items a rename meant. Charts sitting
 * end to end is the arrangement people actually build.
 *
 * Nothing here rejects an overlapping item; the invariant is assumed, not enforced. Two items
 * that do overlap resolve to whichever sorts first, which is a wrong answer rather than a
 * crash.
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

    // How wide the strip opens on a project it knows nothing about yet.
    static constexpr int DEFAULT_ZOOM_BARS = 64;

    // Unmerged on purpose, unlike InstrumentSession's clipSpans. Two charts sitting end to end
    // are two songs, and merging them would make them one with the wrong name.
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
