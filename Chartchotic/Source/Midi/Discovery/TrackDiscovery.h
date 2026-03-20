#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include "../../UI/ControlConstants.h"

struct InstrumentTrackInfo {
    Part part;
    int sourceTrackIndex;    // Backend-opaque: REAPER track idx, MIDI file track idx, etc.
    juce::String trackName;  // "PART GUITAR", "PART DRUMS", etc.
};

// Shared track name → Part matching (used by all REAPER discovery implementations)
inline bool matchTrackNameToPart(const std::string& name, Part& outPart)
{
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    for (auto& entry : getImplementedTrackNames())
        if (upper == entry.name) { outPart = entry.part; return true; }
    for (auto& entry : getUnimplementedTrackNames())
        if (upper == entry.name) { outPart = entry.part; return true; }

    return false;
}

class TrackDiscovery {
public:
    virtual ~TrackDiscovery() = default;
    virtual std::vector<InstrumentTrackInfo> discoverTracks() = 0;
};
