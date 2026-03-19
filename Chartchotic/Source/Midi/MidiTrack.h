/*
  ==============================================================================

    MidiTrack.h
    Raw MIDI data for one track — notes + text events + lock.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Utils/MidiTypes.h"
#include "ChartTextEvent.h"

struct MidiTrack {
    NoteStateMapArray notes{};
    juce::CriticalSection notesLock;
    TrackTextEvents textEvents;
};
