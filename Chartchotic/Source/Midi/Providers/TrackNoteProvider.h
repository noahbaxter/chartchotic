#pragma once

#include <vector>
#include <string>
#include "REAPER/MidiCache.h"
#include "../ChartTextEvent.h"
#include "../Discovery/TrackDiscovery.h"

// One MIDI clip's extents on the timeline, in QN. A project can hold several charts end to
// end, and only the clips say where one stops: charts in the wild often have no [end].
struct ClipSpan {
    double startQN;
    double endQN;
    juce::String name;   // the take's name, empty where the source has none
};

class TrackNoteProvider {
public:
    virtual ~TrackNoteProvider() = default;
    virtual std::vector<MidiCache::CachedNote> fetchNotes(const InstrumentTrackInfo& track) = 0;
    virtual TrackTextEvents fetchTextEvents(const InstrumentTrackInfo& track) = 0;
    virtual std::string getTrackHash(const InstrumentTrackInfo& track) = 0;

    // Empty where the source has no notion of clips, eg a whole .mid file read off disk.
    virtual std::vector<ClipSpan> fetchClipSpans(const InstrumentTrackInfo&) { return {}; }
};
