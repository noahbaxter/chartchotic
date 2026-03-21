#pragma once

#include "TrackDiscovery.h"
#include "../../Host/ReaperTrackDetector.h"

class ReaperLocalDiscovery : public TrackDiscovery
{
public:
    // Auto-detect Part from REAPER track name. Falls back to fallbackPart if track name
    // doesn't match a known instrument (e.g. user renamed the track).
    ReaperLocalDiscovery(ReaperTrackDetector::ReaperGetFunc getFunc,
                         int trackIndex, Part fallbackPart)
        : reaperGetFunc(getFunc), localTrackIndex(trackIndex), localFallback(fallbackPart) {}

    std::vector<InstrumentTrackInfo> discoverTracks() override
    {
        InstrumentTrackInfo info;
        info.sourceTrackIndex = localTrackIndex;

        // Try to detect Part from the actual REAPER track name
        if (reaperGetFunc && localTrackIndex >= 0)
        {
            std::string name = ReaperTrackDetector::getTrackName(reaperGetFunc, localTrackIndex);
            info.trackName = juce::String(name);

            Part detected;
            if (matchTrackNameToPart(name, detected))
                info.part = detected;
            else
                info.part = localFallback;
        }
        else
        {
            info.part = localFallback;
            info.trackName = juce::String("Track ") + juce::String(localTrackIndex + 1);
        }

        return { info };
    }

private:
    ReaperTrackDetector::ReaperGetFunc reaperGetFunc;
    int localTrackIndex;
    Part localFallback;
};
