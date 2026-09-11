/*
  ==============================================================================

    ReaperMidiWriter.cpp
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#include "ReaperMidiWriter.h"
#include "ReaperApiHelpers.h"

#include <algorithm>

ReaperMidiWriter::ReaperMidiWriter(const ReaperAPIs& apis,
                                   std::function<void*(const char*)> reaperGetFunc)
    : apis(apis), getReaperApi(std::move(reaperGetFunc)),
      itemManager(apis, getReaperApi)
{
}

bool ReaperMidiWriter::isAvailable() const
{
    return apis.writeApisLoaded() && getReaperApi != nullptr;
}

void ReaperMidiWriter::endUndoBlock(void* project, const char* description)
{
    if (apis.MarkProjectDirty) apis.MarkProjectDirty(project);
    if (apis.Undo_OnStateChange) apis.Undo_OnStateChange(description);
}

void ReaperMidiWriter::addBatchTake(void* take)
{
    if (std::find(batchTakes.begin(), batchTakes.end(), take) != batchTakes.end())
        return;
    if (apis.MIDI_DisableSort) apis.MIDI_DisableSort(take);
    batchTakes.push_back(take);
}

// =============================================================================
// Single-note operations
// =============================================================================

bool ReaperMidiWriter::insertNote(int trackIndex, double startQN, double endQN,
                                  int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getTakeForWrite(project, trackIndex, startQN, endQN);
    if (!take) return false;

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    bool ok = apis.MIDI_InsertNote(take, true, false,
                                   takePPQStart, takePPQEnd,
                                   channel, pitch, velocity, nullptr);
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    endUndoBlock(project, "Chartchotic: Insert note");
    return ok;
}

MidiWriter::NoteInfo ReaperMidiWriter::findNote(int trackIndex, double positionQN, int pitch)
{
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return {};
    return itemManager.findNote(project, trackIndex, positionQN, pitch);
}

std::vector<MidiWriter::NoteInfo>
ReaperMidiWriter::findNotesInRange(int trackIndex, double startQN,
                                    double endQN, int pitch)
{
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return {};
    return itemManager.findNotesInRange(project, trackIndex, startQN, endQN, pitch);
}

bool ReaperMidiWriter::deleteNote(int trackIndex, int noteIndex)
{
    return deleteNoteAtQN(trackIndex, noteIndex, -1.0);
}

bool ReaperMidiWriter::deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;

    bool ok = apis.MIDI_DeleteNote(take, noteIndex);
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    endUndoBlock(project, "Chartchotic: Delete note");
    return ok;
}

bool ReaperMidiWriter::moveNote(int trackIndex, int noteIndex,
                                double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;

    int ch = 0, vel = 100;
    double oldStart = 0, oldEnd = 0;
    int oldPitch = 0;
    bool sel = false, muted = false;
    if (apis.MIDI_GetNote(take, noteIndex, &sel, &muted, &oldStart, &oldEnd, &ch, &oldPitch, &vel))
    {
        // keep original velocity and channel
    }

    if (!apis.MIDI_DeleteNote(take, noteIndex)) return false;
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);

    void* destTake = itemManager.getTakeForWrite(project, trackIndex, newStartQN, newEndQN);
    if (!destTake) return false;

    double ppqStart = apis.MIDI_GetPPQPosFromProjQN(destTake, newStartQN);
    double ppqEnd   = apis.MIDI_GetPPQPosFromProjQN(destTake, newEndQN);

    bool ok = apis.MIDI_InsertNote(destTake, sel, muted, ppqStart, ppqEnd, ch, newPitch, vel, nullptr);
    if (apis.MIDI_Sort) apis.MIDI_Sort(destTake);
    endUndoBlock(project, "Chartchotic: Move note");
    return ok;
}

// =============================================================================
// Batch operations
// =============================================================================

void ReaperMidiWriter::beginBatch(const char* undoDescription)
{
    juce::ScopedLock lock(writeLock);
    batchProject = ReaperApiHelpers::getProject(getReaperApi);
    if (!batchProject) return;

    batchDescription = undoDescription ? undoDescription : "Chartchotic: Batch edit";
    inBatch = true;
    batchTakes.clear();
}

bool ReaperMidiWriter::batchInsertNote(int trackIndex, double startQN, double endQN,
                                       int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getTakeForWrite(batchProject, trackIndex, startQN, endQN);
    if (!take) return false;
    addBatchTake(take);

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    const bool noSort = true;
    return apis.MIDI_InsertNote(take, true, false,
                                takePPQStart, takePPQEnd,
                                channel, pitch, velocity, &noSort);
}

bool ReaperMidiWriter::batchDeleteNote(int trackIndex, int noteIndex, double hintQN)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;
    addBatchTake(take);

    return apis.MIDI_DeleteNote(take, noteIndex);
}

bool ReaperMidiWriter::batchMoveNote(int trackIndex, int noteIndex,
                                     double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* srcTake = itemManager.getLastFoundTake();
    if (!srcTake) return false;
    addBatchTake(srcTake);

    int ch = 0, vel = 100;
    double oldStart = 0, oldEnd = 0;
    int oldPitch = 0;
    bool sel = false, muted = false;
    if (apis.MIDI_GetNote(srcTake, noteIndex, &sel, &muted, &oldStart, &oldEnd, &ch, &oldPitch, &vel))
    {
        // keep original velocity and channel
    }

    if (!apis.MIDI_DeleteNote(srcTake, noteIndex)) return false;

    void* destTake = itemManager.getTakeForWrite(batchProject, trackIndex, newStartQN, newEndQN);
    if (!destTake) return false;
    addBatchTake(destTake);

    double ppqStart = apis.MIDI_GetPPQPosFromProjQN(destTake, newStartQN);
    double ppqEnd   = apis.MIDI_GetPPQPosFromProjQN(destTake, newEndQN);

    const bool noSort = true;
    return apis.MIDI_InsertNote(destTake, sel, muted, ppqStart, ppqEnd, ch, newPitch, vel, &noSort);
}

void ReaperMidiWriter::endBatch()
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return;

    for (auto* take : batchTakes)
        if (apis.MIDI_Sort) apis.MIDI_Sort(take);

    endUndoBlock(batchProject, batchDescription.toRawUTF8());

    inBatch = false;
    batchProject = nullptr;
    batchTakes.clear();
    batchDescription.clear();
}

//==============================================================================
// Text events

namespace
{
    constexpr int kTextEventType = 1;   // FF 01

    constexpr const char* kEventsTrackName = "EVENTS";
    constexpr const char* kSectionPrefix   = "[section ";

    // Inner name, or empty when the text is not a section event. "[prc_NAME]" and
    // "[section_NAME]" are both documented equivalents, so all three are read even
    // though only the "[section NAME]" form is ever written.
    std::string parseSectionName(const std::string& raw)
    {
        if (raw.size() < 7 || raw.front() != '[' || raw.back() != ']') return {};

        if (raw.compare(0, 5, "[prc_") == 0)
            return raw.substr(5, raw.size() - 6);

        if (raw.size() < 11 || raw.compare(0, 8, "[section") != 0) return {};
        char sep = raw[8];
        if (sep != ' ' && sep != '_') return {};
        return raw.substr(9, raw.size() - 10);
    }
}

bool ReaperMidiWriter::ensureTrackTextEvent(int trackIndex, const std::string& text)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project || !apis.GetTrack || !apis.CountTrackMediaItems || !apis.GetTrackMediaItem
        || !apis.GetActiveTake || !apis.MIDI_CountEvts || !apis.MIDI_GetTextSysexEvt
        || !apis.MIDI_InsertTextSysexEvt)
        return false;

    void* track = apis.GetTrack(project, trackIndex);
    if (!track || apis.CountTrackMediaItems(track) <= 0) return false;

    void* item = apis.GetTrackMediaItem(track, 0);
    void* take = item ? apis.GetActiveTake(item) : nullptr;
    if (!take) return false;

    int noteCount = 0, ccCount = 0, textCount = 0;
    apis.MIDI_CountEvts(take, &noteCount, &ccCount, &textCount);
    for (int i = 0; i < textCount; ++i)
    {
        bool selected = false, muted = false;
        double ppq = 0.0;
        int type = 0;
        char msg[1024] = {};
        int msgSize = sizeof(msg);

        if (!apis.MIDI_GetTextSysexEvt(take, i, &selected, &muted, &ppq, &type, msg, &msgSize))
            continue;
        if (type != kTextEventType) continue;
        if (std::string(msg, (size_t)msgSize) == text) return true;   // already present
    }

    // ppq 0 is the start of the item, which is where charts conventionally put
    // switches like this.
    apis.MIDI_InsertTextSysexEvt(take, false, false, 0.0, kTextEventType,
                                 text.c_str(), (int)text.size());
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    return true;
}

//==============================================================================
// Sections
//
// FF 01 text events of the form "[section NAME]" on a track named EVENTS. They are
// global to the chart, so they deliberately do not live on any instrument track.

void* ReaperMidiWriter::findEventsTake(void* project) const
{
    if (!apis.CountTracks || !apis.GetTrack || !apis.GetSetMediaTrackInfo_String) return nullptr;

    int trackCount = apis.CountTracks(project);
    for (int i = 0; i < trackCount; ++i)
    {
        void* track = apis.GetTrack(project, i);
        if (!track) continue;

        char name[512] = {};
        if (!apis.GetSetMediaTrackInfo_String(track, "P_NAME", name, false)) continue;
        if (!juce::String(name).trim().equalsIgnoreCase(kEventsTrackName)) continue;

        if (!apis.CountTrackMediaItems || !apis.GetTrackMediaItem || !apis.GetActiveTake)
            return nullptr;
        if (apis.CountTrackMediaItems(track) <= 0) return nullptr;

        void* item = apis.GetTrackMediaItem(track, 0);
        return item ? apis.GetActiveTake(item) : nullptr;
    }
    return nullptr;
}

void* ReaperMidiWriter::ensureEventsTake(void* project)
{
    if (void* existing = findEventsTake(project)) return existing;

    if (!apis.CountTracks || !apis.GetTrack || !apis.InsertTrackAtIndex
        || !apis.GetSetMediaTrackInfo_String || !apis.CreateNewMIDIItemInProj)
        return nullptr;

    // An EVENTS track may exist with no MIDI item on it, in which case the track is
    // reused and only the item is added.
    void* track = nullptr;
    int trackCount = apis.CountTracks(project);
    for (int i = 0; i < trackCount && !track; ++i)
    {
        void* t = apis.GetTrack(project, i);
        if (!t) continue;
        char name[512] = {};
        if (apis.GetSetMediaTrackInfo_String(t, "P_NAME", name, false)
            && juce::String(name).trim().equalsIgnoreCase(kEventsTrackName))
            track = t;
    }

    if (!track)
    {
        apis.InsertTrackAtIndex(trackCount, true);
        track = apis.GetTrack(project, trackCount);
        if (!track) return nullptr;

        char name[512] = {};
        juce::String(kEventsTrackName).copyToUTF8(name, sizeof(name));
        apis.GetSetMediaTrackInfo_String(track, "P_NAME", name, true);
        if (apis.TrackList_AdjustWindows) apis.TrackList_AdjustWindows(false);
    }

    // One item spanning a generous range, so sections placed later in the song still
    // land inside it. QN units keep this independent of tempo.
    const bool qnIn = true;
    void* item = apis.CreateNewMIDIItemInProj(track, 0.0, 4.0 * 1000.0, &qnIn);
    return item ? apis.GetActiveTake(item) : nullptr;
}

std::vector<MidiWriter::SectionInfo> ReaperMidiWriter::getSections()
{
    std::vector<SectionInfo> out;
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return out;

    void* take = findEventsTake(project);
    if (!take || !apis.MIDI_CountEvts || !apis.MIDI_GetTextSysexEvt
        || !apis.MIDI_GetProjQNFromPPQPos)
        return out;

    int noteCount = 0, ccCount = 0, textCount = 0;
    if (apis.MIDI_CountEvts(take, &noteCount, &ccCount, &textCount) == 0 || textCount == 0)
        return out;

    for (int i = 0; i < textCount; ++i)
    {
        bool selected = false, muted = false;
        double ppq = 0.0;
        int type = 0;
        char msg[1024] = {};
        int msgSize = sizeof(msg);

        if (!apis.MIDI_GetTextSysexEvt(take, i, &selected, &muted, &ppq, &type, msg, &msgSize))
            continue;
        if (type != kTextEventType) continue;

        std::string name = parseSectionName(std::string(msg, (size_t)msgSize));
        if (name.empty()) continue;

        out.push_back({ apis.MIDI_GetProjQNFromPPQPos(take, ppq), name });
    }

    std::sort(out.begin(), out.end(),
              [](const SectionInfo& a, const SectionInfo& b) { return a.qn < b.qn; });
    return out;
}

std::vector<ReaperMidiWriter::EventsTake> ReaperMidiWriter::eventsTakes(void* project) const
{
    std::vector<EventsTake> takes;
    if (!apis.CountTracks || !apis.GetTrack || !apis.GetSetMediaTrackInfo_String
        || !apis.CountTrackMediaItems || !apis.GetTrackMediaItem || !apis.GetActiveTake)
        return takes;

    int trackCount = apis.CountTracks(project);
    for (int i = 0; i < trackCount; ++i)
    {
        void* track = apis.GetTrack(project, i);
        if (!track) continue;

        char name[512] = {};
        if (!apis.GetSetMediaTrackInfo_String(track, "P_NAME", name, false)) continue;
        if (!juce::String(name).trim().equalsIgnoreCase(kEventsTrackName)) continue;

        int itemCount = apis.CountTrackMediaItems(track);
        for (int n = 0; n < itemCount; ++n)
        {
            void* item = apis.GetTrackMediaItem(track, n);
            if (!item) continue;

            void* take = apis.GetActiveTake(item);
            if (!take) continue;

            EventsTake et;
            et.take = take;
            if (apis.GetMediaItemInfo_Value && apis.TimeMap2_timeToQN)
            {
                double pos = apis.GetMediaItemInfo_Value(item, "D_POSITION");
                double len = apis.GetMediaItemInfo_Value(item, "D_LENGTH");
                et.startQN = apis.TimeMap2_timeToQN(project, pos);
                et.endQN = len > 0.0 ? apis.TimeMap2_timeToQN(project, pos + len) : et.startQN;
            }
            takes.push_back(et);
        }
    }
    return takes;
}

void* ReaperMidiWriter::takeCovering(const std::vector<EventsTake>& takes, double qn) const
{
    if (takes.empty()) return nullptr;

    for (const auto& et : takes)
        if (et.endQN > et.startQN && qn >= et.startQN && qn < et.endQN)
            return et.take;

    return takes.front().take;
}

int ReaperMidiWriter::clearSections(void* take)
{
    if (!take || !apis.MIDI_CountEvts || !apis.MIDI_GetTextSysexEvt
        || !apis.MIDI_DeleteTextSysexEvt)
        return 0;

    int noteCount = 0, ccCount = 0, textCount = 0;
    apis.MIDI_CountEvts(take, &noteCount, &ccCount, &textCount);

    // Back to front, so deleting one does not shift the indices still to be looked at.
    int removed = 0;
    for (int i = textCount - 1; i >= 0; --i)
    {
        bool selected = false, muted = false;
        double ppq = 0.0;
        int type = 0;
        char msg[1024] = {};
        int msgSize = sizeof(msg);

        if (!apis.MIDI_GetTextSysexEvt(take, i, &selected, &muted, &ppq, &type, msg, &msgSize))
            continue;
        if (type != kTextEventType) continue;
        if (parseSectionName(std::string(msg, (size_t)msgSize)).empty()) continue;

        apis.MIDI_DeleteTextSysexEvt(take, i);
        ++removed;
    }
    return removed;
}

bool ReaperMidiWriter::setSections(const std::vector<SectionInfo>& sections)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    if (!apis.MIDI_CountEvts || !apis.MIDI_GetTextSysexEvt
        || !apis.MIDI_DeleteTextSysexEvt || !apis.MIDI_InsertTextSysexEvt
        || !apis.MIDI_GetPPQPosFromProjQN)
        return false;

    // Make sure the track and an item exist before gathering, so a first edit on a project
    // with no EVENTS track still has somewhere to write.
    if (ensureEventsTake(project) == nullptr) return false;

    auto takes = eventsTakes(project);
    if (takes.empty()) return false;

    // Cleared everywhere before anything is written. Reading gathers sections from every
    // item, so leaving one item's markers in place is what made them accumulate.
    // Renaming one section can renumber the later ones, so replace-all is also simpler and
    // a single undo point.
    for (const auto& et : takes)
        clearSections(et.take);

    // Each section goes into the item that covers it, rather than all of them into the
    // first. On a shared timeline that keeps each chart's sections inside its own item.
    for (const auto& s : sections)
    {
        void* take = takeCovering(takes, s.qn);
        if (!take) continue;

        std::string text = std::string(kSectionPrefix) + s.name + "]";
        double ppq = apis.MIDI_GetPPQPosFromProjQN(take, s.qn);
        apis.MIDI_InsertTextSysexEvt(take, false, false, ppq, kTextEventType,
                                     text.c_str(), (int)text.size());
    }

    if (apis.MIDI_Sort)
        for (const auto& et : takes)
            apis.MIDI_Sort(et.take);

    endUndoBlock(project, "Chartchotic: Edit sections");
    return true;
}
