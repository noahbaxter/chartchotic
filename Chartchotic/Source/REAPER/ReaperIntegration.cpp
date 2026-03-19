/*
  ==============================================================================

    ReaperIntegration.cpp
    REAPER timeline MIDI processing — raw storage only, no gem calculation.

  ==============================================================================
*/

#include "ReaperIntegration.h"
#include "../PluginProcessor.h"
#include "../Utils/Utils.h"

void ReaperIntegration::processReaperTimelineMidi(
    ChartchoticAudioProcessor& processor,
    PPQ startPPQ,
    PPQ endPPQ,
    double bpm,
    uint timeSignatureNumerator,
    uint timeSignatureDenominator)
{
    if (!processor.isReaperHost || !processor.reaperMidiProvider.isReaperApiAvailable())
        return;

    auto& midiProcessor = processor.getMidiProcessor();

    // Clear existing note data in this range
    midiProcessor.clearNoteDataInRange(startPPQ, endPPQ);

    // Get notes from REAPER timeline
    auto reaperNotes = processor.reaperMidiProvider.getNotesInRange(startPPQ.toDouble(), endPPQ.toDouble());

    // Store all notes raw — no pitch filtering, no gem calculation
    {
        const juce::ScopedLock lock(midiProcessor.noteStateMapLock);

        for (const auto& reaperNote : reaperNotes)
        {
            if (reaperNote.muted) continue;

            uint pitch = reaperNote.pitch;
            PPQ noteStartPPQ = PPQ(reaperNote.startPPQ);
            PPQ noteEndPPQ = PPQ(reaperNote.endPPQ);

            midiProcessor.noteStateMapArray[pitch][noteStartPPQ] = NoteData(reaperNote.velocity);
            midiProcessor.noteStateMapArray[pitch][noteEndPPQ - PPQ(1)] = NoteData(0);
        }
    }
}
