/*
  ==============================================================================

    NoteProcessor.cpp
    Implementation of MIDI note to gem type conversion

  ==============================================================================
*/

#include "NoteProcessor.h"
#include "../Utils/InstrumentMapper.h"
#include "../Utils/ChordAnalyzer.h"
#include "../Utils/GemCalculator.h"

void NoteProcessor::processModifierNotes(
    const std::vector<MidiCache::CachedNote>& notes,
    NoteStateMapArray& noteStateMapArray,
    juce::CriticalSection& noteStateMapLock,
    juce::ValueTree& state)
{
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> validModifierPitches;

    Part part = (Part)(int)state.getProperty("part");
    if (isDrumLike(part))
    {
        validModifierPitches = InstrumentMapper::getDrumModifierPitches();
    }
    else if (isGuitarLike(part))
    {
        validModifierPitches = InstrumentMapper::getGuitarModifierPitchesForSkill(currentSkill);
    }

    const juce::ScopedLock lock(noteStateMapLock);

    for (const auto& note : notes)
    {
        if (note.muted) continue;

        // Check if this is a valid modifier pitch
        bool isModifier = std::find(validModifierPitches.begin(), validModifierPitches.end(), note.pitch) != validModifierPitches.end();
        if (!isModifier) continue;

        // Add modifier to note state map (no gem type needed for modifiers)
        addNoteToMap(noteStateMapArray, note.pitch, note.startPPQ, note.endPPQ, NoteData(note.velocity, Gem::NONE));
    }
}

void NoteProcessor::processPlayableNotes(
    const std::vector<MidiCache::CachedNote>& notes,
    NoteStateMapArray& noteStateMapArray,
    juce::CriticalSection& noteStateMapLock,
    MidiProcessor& midiProcessor,
    juce::ValueTree& state,
    double bpm,
    double sampleRate)
{
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> validPlayablePitches;

    Part part = (Part)(int)state.getProperty("part");
    if (isDrumLike(part))
    {
        validPlayablePitches = InstrumentMapper::getDrumPitchesForSkill(currentSkill);
    }
    else if (isGuitarLike(part))
    {
        validPlayablePitches = InstrumentMapper::getGuitarPitchesForSkill(currentSkill);
    }

    // Track guitar-like note positions for chord fixing
    std::set<PPQ> guitarNotePositions;

    const juce::ScopedLock lock(noteStateMapLock);

    for (const auto& note : notes)
    {
        if (note.muted) continue;

        // Check if this is a valid playable pitch for current skill level
        bool isValidPlayablePitch = std::find(validPlayablePitches.begin(), validPlayablePitches.end(),
                                             note.pitch) != validPlayablePitches.end();
        if (!isValidPlayablePitch) continue;

        // Calculate gem type
        Gem gemType = Gem::NONE;
        if (note.velocity > 0)
        {
            if (isGuitarLike(part))
            {
                gemType = GemCalculator::getGuitarGemType(note.pitch, note.startPPQ, state,
                                                           noteStateMapArray, noteStateMapLock);
                guitarNotePositions.insert(note.startPPQ);
            }
            else if (isDrumLike(part))
            {
                Dynamic dynamic = (Dynamic)note.velocity;
                gemType = GemCalculator::getDrumGemType(note.pitch, note.startPPQ, dynamic, state,
                                                        noteStateMapArray, noteStateMapLock);
            }
        }

        // Add to note state map
        addNoteToMap(noteStateMapArray, note.pitch, note.startPPQ, note.endPPQ, NoteData(note.velocity, gemType));
    }

    // Fix chord HOPOs for guitar-like instruments
    if (isGuitarLike(part) && !guitarNotePositions.empty())
    {
        std::vector<PPQ> positions(guitarNotePositions.begin(), guitarNotePositions.end());
        ChordAnalyzer::fixChordHOPOs(positions, currentSkill, noteStateMapArray, noteStateMapLock);
    }
}

void NoteProcessor::addNoteToMap(NoteStateMapArray& noteStateMapArray, uint pitch, PPQ startPPQ, PPQ endPPQ, const NoteData& data)
{
    if (pitch < noteStateMapArray.size())
    {
        noteStateMapArray[pitch][startPPQ] = data;
        // startPPQ + 1 handles 0 length notes
        // endPPQ - 1 ensures we don't overwrite the next note's start
        noteStateMapArray[pitch][std::max(startPPQ + PPQ(1), endPPQ - PPQ(1))] = NoteData(0, Gem::NONE);
    }
}
