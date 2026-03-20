#pragma once

#include <set>
#include "TrackDiscovery.h"
#include "../../Host/ReaperTrackDetector.h"

class ReaperGlobalDiscovery : public TrackDiscovery
{
public:
    explicit ReaperGlobalDiscovery(ReaperTrackDetector::ReaperGetFunc getFunc)
        : reaperGetFunc(getFunc) {}

    std::vector<InstrumentTrackInfo> discoverTracks() override
    {
        std::vector<InstrumentTrackInfo> tracks;
        if (!reaperGetFunc) return tracks;

        int trackCount = ReaperTrackDetector::getTrackCount(reaperGetFunc);
        std::set<Part> seenParts;

        for (int i = 0; i < trackCount; ++i)
        {
            std::string name = ReaperTrackDetector::getTrackName(reaperGetFunc, i);
            Part part;
            if (!matchTrackNameToPart(name, part))
                continue;

            // First match wins — skip duplicate instrument types
            if (seenParts.count(part))
                continue;
            seenParts.insert(part);

            InstrumentTrackInfo info;
            info.part = part;
            info.sourceTrackIndex = i;
            info.trackName = juce::String(name);
            tracks.push_back(info);
        }

        return tracks;
    }

private:
    ReaperTrackDetector::ReaperGetFunc reaperGetFunc;
};
