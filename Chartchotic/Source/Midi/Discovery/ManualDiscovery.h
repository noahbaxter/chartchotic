#pragma once

#include "TrackDiscovery.h"

class ManualDiscovery : public TrackDiscovery
{
public:
    explicit ManualDiscovery(Part part)
        : selectedPart(part) {}

    std::vector<InstrumentTrackInfo> discoverTracks() override
    {
        InstrumentTrackInfo info;
        info.part = selectedPart;
        info.sourceTrackIndex = -1;  // No specific track (reads from plugin's own MIDI input)
        info.trackName = partToTrackName(selectedPart);
        return { info };
    }

    void setPart(Part part) { selectedPart = part; }

private:
    static juce::String partToTrackName(Part p)
    {
        switch (p) {
            case Part::GUITAR:     return "PART GUITAR";
            case Part::BASS:       return "PART BASS";
            case Part::KEYS:       return "PART KEYS";
            case Part::DRUMS:      return "PART DRUMS";
            case Part::VOCALS:     return "PART VOCALS";
            case Part::GHL_GUITAR: return "PART GUITAR GHL";
            case Part::GHL_BASS:   return "PART BASS GHL";
            case Part::PRO_GUITAR: return "PART REAL_GUITAR";
            case Part::PRO_BASS:   return "PART REAL_BASS";
            case Part::PRO_KEYS:   return "PART REAL_KEYS";
            default:               return "PART GUITAR";
        }
    }

    Part selectedPart;
};
