#pragma once

#include <vector>
#include <string>
#include "REAPER/MidiCache.h"
#include "../ChartTextEvent.h"
#include "../Discovery/TrackDiscovery.h"

class TrackNoteProvider {
public:
    virtual ~TrackNoteProvider() = default;
    virtual std::vector<MidiCache::CachedNote> fetchNotes(const InstrumentTrackInfo& track) = 0;
    virtual TrackTextEvents fetchTextEvents(const InstrumentTrackInfo& track) = 0;
    virtual std::string getTrackHash(const InstrumentTrackInfo& track) = 0;
};
