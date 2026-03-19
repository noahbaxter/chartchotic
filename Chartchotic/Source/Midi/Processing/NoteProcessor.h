/*
  ==============================================================================

    NoteProcessor.h
    Writes raw MIDI notes into NoteStateMapArray.
    No gem calculation — that happens at render time in TrackResolver.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Providers/REAPER/MidiCache.h"
#include "../Utils/MidiTypes.h"
#include "../../Utils/Utils.h"

class NoteProcessor
{
public:
    NoteProcessor() = default;

    // Process all notes into note state map — raw velocity only.
    // Caller MUST hold noteStateMapLock.
    void processAllNotes(
        const std::vector<MidiCache::CachedNote>& notes,
        NoteStateMapArray& noteStateMapArray);

private:
    void addNoteToMap(NoteStateMapArray& noteStateMapArray, uint pitch, PPQ startPPQ, PPQ endPPQ, const NoteData& data);
};
