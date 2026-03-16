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

    if (upper == "PART GUITAR" || upper == "PART GUITAR COOP" || upper == "PART RHYTHM")
        { outPart = Part::GUITAR; return true; }
    if (upper == "PART BASS")
        { outPart = Part::BASS; return true; }
    if (upper == "PART KEYS")
        { outPart = Part::KEYS; return true; }
    if (upper == "PART DRUMS")
        { outPart = Part::DRUMS; return true; }
    if (upper == "PART GUITAR GHL")
        { outPart = Part::GHL_GUITAR; return true; }
    if (upper == "PART BASS GHL")
        { outPart = Part::GHL_BASS; return true; }
    if (upper == "PART REAL_GUITAR" || upper == "PART REAL_GUITAR_22")
        { outPart = Part::PRO_GUITAR; return true; }
    if (upper == "PART REAL_BASS" || upper == "PART REAL_BASS_22")
        { outPart = Part::PRO_BASS; return true; }

    return false;
}

class TrackDiscovery {
public:
    virtual ~TrackDiscovery() = default;
    virtual std::vector<InstrumentTrackInfo> discoverTracks() = 0;
};
