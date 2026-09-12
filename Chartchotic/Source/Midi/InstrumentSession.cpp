#include "InstrumentSession.h"
#include "Utils/InstrumentMapper.h"

#include <algorithm>

const std::vector<MidiCache::CachedNote> InstrumentSession::emptyNotes;
const TrackTextEvents InstrumentSession::emptyTextEvents;
const DiscoFlipState InstrumentSession::emptyDiscoFlip;
const StarPowerState InstrumentSession::emptyStarPower;
const StrictHatPedalState InstrumentSession::emptyStrictHatPedal;

InstrumentSession::InstrumentSession(std::unique_ptr<TrackDiscovery> disc,
                                     std::unique_ptr<TrackNoteProvider> prov)
    : discovery(std::move(disc)), provider(std::move(prov))
{
}

void InstrumentSession::discover()
{
    tracks = discovery->discoverTracks();
    trackData.resize(tracks.size());

    for (int i = 0; i < (int)tracks.size(); ++i)
        fetchTrackData(i);
}

bool InstrumentSession::pollForChanges()
{
    // Re-run discovery to detect added/removed/renamed tracks
    auto freshTracks = discovery->discoverTracks();
    bool structureChanged = freshTracks.size() != tracks.size();
    if (!structureChanged)
    {
        for (int i = 0; i < (int)tracks.size(); ++i)
        {
            if (freshTracks[i].part != tracks[i].part
                || freshTracks[i].sourceTrackIndex != tracks[i].sourceTrackIndex)
            {
                structureChanged = true;
                break;
            }
        }
    }

    if (structureChanged)
    {
        tracks = std::move(freshTracks);
        trackData.resize(tracks.size());
        for (int i = 0; i < (int)tracks.size(); ++i)
            fetchTrackData(i);
        return true;
    }

    // No structural change — just check for note edits on existing tracks
    bool anyChanged = false;
    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        std::string currentHash = provider->getTrackHash(tracks[i]);
        if (currentHash != trackData[i].lastHash)
        {
            fetchTrackData(i);
            anyChanged = true;
        }
    }
    return anyChanged;
}

const std::vector<MidiCache::CachedNote>& InstrumentSession::getNotes(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyNotes;
    return trackData[trackIdx].notes;
}

const TrackTextEvents& InstrumentSession::getTextEvents(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyTextEvents;
    return trackData[trackIdx].textEvents;
}

const DiscoFlipState& InstrumentSession::getDiscoFlipState(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyDiscoFlip;
    return trackData[trackIdx].discoFlipState;
}

const StarPowerState& InstrumentSession::getStarPowerState(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyStarPower;
    return trackData[trackIdx].starPowerState;
}

const StrictHatPedalState& InstrumentSession::getStrictHatPedalState(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyStrictHatPedal;
    return trackData[trackIdx].strictHatPedalState;
}

void InstrumentSession::setDiscovery(std::unique_ptr<TrackDiscovery> newDiscovery)
{
    discovery = std::move(newDiscovery);
}

// Clips overlap across tracks — guitar, drums and EVENTS all cover the same song — so the
// union is what matters: one span per chart on the timeline, whatever it is made of.
void InstrumentSession::rebuildClipSpans()
{
    std::vector<ClipSpan> all;
    for (const auto& td : trackData)
        all.insert(all.end(), td.clipSpans.begin(), td.clipSpans.end());

    std::sort(all.begin(), all.end(),
              [](const ClipSpan& a, const ClipSpan& b) { return a.startQN < b.startQN; });

    clipSpans.clear();
    for (const auto& span : all)
    {
        if (!clipSpans.empty() && span.startQN <= clipSpans.back().endQN)
            clipSpans.back().endQN = std::max(clipSpans.back().endQN, span.endQN);
        else
            clipSpans.push_back(span);
    }
}

void InstrumentSession::fetchTrackData(int idx)
{
    auto& td = trackData[idx];
    td.notes = provider->fetchNotes(tracks[idx]);
    td.textEvents = provider->fetchTextEvents(tracks[idx]);
    td.clipSpans = provider->fetchClipSpans(tracks[idx]);
    td.discoFlipState.buildFromTextEvents(td.textEvents);
    td.strictHatPedalState.buildFromTextEvents(td.textEvents);
    td.starPowerState.buildFromNotes(td.notes, InstrumentMapper::starPowerPitch(tracks[idx].part));
    td.lastHash = provider->getTrackHash(tracks[idx]);

    // Song-scoped, so it lives on the session rather than the track it happens to come from.
    // The song list is built from this track's own spans rather than the merged ones: the
    // merge is what would collapse an album item and the per-song items inside it into one.
    if (tracks[idx].part == Part::EVENTS)
    {
        songSections.buildFromTextEvents(td.textEvents);
        songs.buildFromSpans(td.clipSpans);
    }

    rebuildClipSpans();
}

void InstrumentSession::invalidateTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return;
    fetchTrackData(trackIdx);
}
