/*
  ==============================================================================

    MidiProject.h
    Top-level MIDI container — tempo/timesig + vector of MidiTrack.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "MidiTrack.h"
#include "../Utils/Utils.h"
#include "../Utils/TimeConverter.h"

class MidiProject {
public:
    // Project-wide tempo and time signature data
    TempoTimeSignatureMap tempoTimeSignatures;
    juce::CriticalSection tempoLock;

    // Per-instrument tracks
    std::vector<std::unique_ptr<MidiTrack>> tracks;

    MidiTrack& ensureTrack(int index)
    {
        while ((int)tracks.size() <= index)
            tracks.push_back(std::make_unique<MidiTrack>());
        return *tracks[index];
    }

    MidiTrack& primaryTrack() { return ensureTrack(0); }

    void clear()
    {
        for (auto& t : tracks)
        {
            const juce::ScopedLock lock(t->notesLock);
            for (auto& map : t->notes)
                map.clear();
            t->textEvents.clear();
        }
        {
            const juce::ScopedLock lock(tempoLock);
            tempoTimeSignatures.clear();
        }
    }
};
