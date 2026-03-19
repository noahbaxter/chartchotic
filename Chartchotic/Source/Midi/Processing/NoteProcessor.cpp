/*
  ==============================================================================

    NoteProcessor.cpp
    Raw MIDI note storage — no gem calculation, no pitch filtering.

  ==============================================================================
*/

#include "NoteProcessor.h"

void NoteProcessor::processAllNotes(
    const std::vector<MidiCache::CachedNote>& notes,
    NoteStateMapArray& noteStateMapArray)
{
    for (const auto& note : notes)
    {
        if (note.muted) continue;
        addNoteToMap(noteStateMapArray, note.pitch, note.startPPQ, note.endPPQ, NoteData(note.velocity));
    }
}

void NoteProcessor::addNoteToMap(NoteStateMapArray& noteStateMapArray, uint pitch, PPQ startPPQ, PPQ endPPQ, const NoteData& data)
{
    if (pitch < noteStateMapArray.size())
    {
        noteStateMapArray[pitch][startPPQ] = data;
        noteStateMapArray[pitch][std::max(startPPQ + PPQ(1), endPPQ - PPQ(1))] = NoteData(0);
    }
}
