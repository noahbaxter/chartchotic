#pragma once

#include "TrackDiscovery.h"
#include "PartMapping.h"
#include "../../Host/ReaperTrackDetector.h"

class ReaperGlobalDiscovery : public TrackDiscovery
{
public:
    // The map is owned by the processor and may be null outside REAPER. Held by pointer so a
    // pin made in the parts menu is seen by the next scan without rebuilding discovery.
    ReaperGlobalDiscovery(ReaperTrackDetector::ReaperGetFunc getFunc,
                          const PartMapping::Map* mapping = nullptr)
        : reaperGetFunc(getFunc), partMap(mapping) {}

    // Resolved through PartMapping, the same path the parts menu draws from, so what the
    // table shows is what gets loaded. Pins win; names fill the rest.
    std::vector<InstrumentTrackInfo> discoverTracks() override
    {
        if (!reaperGetFunc) return {};

        std::vector<InstrumentTrackInfo> candidates;
        const int trackCount = ReaperTrackDetector::getTrackCount(reaperGetFunc);

        for (int i = 0; i < trackCount; ++i)
        {
            auto name = juce::String(ReaperTrackDetector::getTrackName(reaperGetFunc, i));
            const auto content = ReaperTrackDetector::getTrackContent(reaperGetFunc, i);

            if (!PartMapping::canBeGuessedAsPart(content.midi, content.audio,
                                                 PartMapping::isUtilityTrackName(name)))
                continue;

            candidates.push_back({ Part::GUITAR, i, name });
        }

        // No map yet means no pins to honour, and the guesses still stand on their own.
        static const PartMapping::Map empty;
        const auto& mapping = partMap != nullptr ? *partMap : empty;

        std::vector<InstrumentTrackInfo> tracks;
        for (const auto& assignment : mapping.resolve(candidates))
        {
            InstrumentTrackInfo info;
            info.part = assignment.part;
            info.sourceTrackIndex = assignment.sourceTrackIndex;
            info.trackName = juce::String(
                ReaperTrackDetector::getTrackName(reaperGetFunc, assignment.sourceTrackIndex));
            tracks.push_back(info);
        }

        return tracks;
    }

private:
    ReaperTrackDetector::ReaperGetFunc reaperGetFunc;
    const PartMapping::Map* partMap = nullptr;
};
