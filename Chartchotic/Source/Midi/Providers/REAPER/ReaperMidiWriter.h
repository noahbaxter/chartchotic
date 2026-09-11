/*
  ==============================================================================

    ReaperMidiWriter.h
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../MidiWriter.h"
#include "ReaperApiHelpers.h"
#include "ReaperItemManager.h"

#include <vector>

class ReaperMidiWriter : public MidiWriter
{
public:
    ReaperMidiWriter(const ReaperAPIs& apis,
                     std::function<void*(const char*)> reaperGetFunc);
    ~ReaperMidiWriter() override = default;

    bool isAvailable() const override;

    bool insertNote(int trackIndex, double startQN, double endQN,
                   int channel, int pitch, int velocity) override;
    bool deleteNote(int trackIndex, int noteIndex) override;
    bool deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN) override;
    NoteInfo findNote(int trackIndex, double positionQN, int pitch) override;
    std::vector<NoteInfo> findNotesInRange(int trackIndex, double startQN,
                                            double endQN, int pitch) override;
    bool moveNote(int trackIndex, int noteIndex,
                 double newStartQN, double newEndQN, int newPitch) override;

    void beginBatch(const char* undoDescription) override;
    bool batchInsertNote(int trackIndex, double startQN, double endQN,
                        int channel, int pitch, int velocity) override;
    bool batchDeleteNote(int trackIndex, int noteIndex, double hintQN = -1.0) override;
    bool batchMoveNote(int trackIndex, int noteIndex,
                      double newStartQN, double newEndQN, int newPitch) override;
    void endBatch() override;

    bool ensureTrackTextEvent(int trackIndex, const std::string& text) override;

    std::vector<SectionInfo> getSections() override;
    bool setSections(const std::vector<SectionInfo>& sections) override;

private:
    // EVENTS is a plain REAPER track named "EVENTS". It can hold several MIDI items: one
    // per chart where a project has more than one song on a shared timeline.
    //
    // Reading gathers sections from every one of those items, so writing has to reach every
    // one of them too. It did not: it cleared the first item and wrote the whole set back
    // into it, so every save copied the other items' markers into the first and they piled
    // up. A project saved a dozen times grew a dozen copies of every section.
    void* findEventsTake(void* project) const;
    void* ensureEventsTake(void* project);

    // A take on the EVENTS track, with the span of the item holding it. The span is taken
    // while the item is in hand, since a take cannot be asked for its item.
    struct EventsTake
    {
        void* take = nullptr;
        double startQN = 0.0;
        double endQN = 0.0;
    };

    // Every take on the EVENTS track, in item order.
    std::vector<EventsTake> eventsTakes(void* project) const;

    // The take whose item covers `qn`, or the first take when nothing does, so a section
    // written past the end of every item still lands somewhere.
    void* takeCovering(const std::vector<EventsTake>& takes, double qn) const;

    // Drops every section marker from one take. Returns how many went.
    int clearSections(void* take);


    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;
    ReaperItemManager itemManager;

    void endUndoBlock(void* project, const char* description);
    void addBatchTake(void* take);

    bool inBatch = false;
    void* batchProject = nullptr;
    std::vector<void*> batchTakes;
    juce::String batchDescription;

    juce::CriticalSection writeLock;
};
