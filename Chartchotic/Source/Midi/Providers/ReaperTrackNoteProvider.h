#pragma once

#include "TrackNoteProvider.h"
#include "REAPER/ReaperMidiProvider.h"

class ReaperTrackNoteProvider : public TrackNoteProvider
{
public:
    explicit ReaperTrackNoteProvider(ReaperMidiProvider& provider)
        : reaperProvider(provider) {}

    std::vector<MidiCache::CachedNote> fetchNotes(const InstrumentTrackInfo& track) override
    {
        auto notes = reaperProvider.getAllNotesFromTrack(track.sourceTrackIndex);

        std::vector<MidiCache::CachedNote> cached;
        cached.reserve(notes.size());
        for (const auto& reaperNote : notes)
        {
            MidiCache::CachedNote cn;
            cn.startPPQ = PPQ(reaperNote.startPPQ);
            cn.endPPQ = PPQ(reaperNote.endPPQ);
            cn.pitch = reaperNote.pitch;
            cn.velocity = reaperNote.velocity;
            cn.channel = reaperNote.channel;
            cn.muted = reaperNote.muted;
            cached.push_back(cn);
        }
        return cached;
    }

    TrackTextEvents fetchTextEvents(const InstrumentTrackInfo& track) override
    {
        return reaperProvider.getAllTextEventsFromTrack(track.sourceTrackIndex);
    }

    std::string getTrackHash(const InstrumentTrackInfo& track) override
    {
        return reaperProvider.getTrackHash(track.sourceTrackIndex, true);
    }

private:
    ReaperMidiProvider& reaperProvider;
};
