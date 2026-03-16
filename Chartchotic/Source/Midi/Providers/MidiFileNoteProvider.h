#pragma once

#include "TrackNoteProvider.h"
#include <map>

class MidiFileNoteProvider : public TrackNoteProvider
{
public:
    // Set data for a track name (e.g. "PART GUITAR")
    void setTrackData(const juce::String& trackName,
                      const std::vector<MidiCache::CachedNote>& notes,
                      const TrackTextEvents& textEvents)
    {
        trackNotes[trackName] = notes;
        trackTexts[trackName] = textEvents;
        // Increment hash counter to signal change
        ++hashCounter;
    }

    void clear()
    {
        trackNotes.clear();
        trackTexts.clear();
        ++hashCounter;
    }

    std::vector<MidiCache::CachedNote> fetchNotes(const InstrumentTrackInfo& track) override
    {
        auto it = trackNotes.find(track.trackName);
        if (it != trackNotes.end())
            return it->second;
        return {};
    }

    TrackTextEvents fetchTextEvents(const InstrumentTrackInfo& track) override
    {
        auto it = trackTexts.find(track.trackName);
        if (it != trackTexts.end())
            return it->second;
        return {};
    }

    std::string getTrackHash(const InstrumentTrackInfo& track) override
    {
        // Static data — hash only changes when setTrackData is called
        return std::to_string(hashCounter);
    }

private:
    std::map<juce::String, std::vector<MidiCache::CachedNote>> trackNotes;
    std::map<juce::String, TrackTextEvents> trackTexts;
    int hashCounter = 0;
};
