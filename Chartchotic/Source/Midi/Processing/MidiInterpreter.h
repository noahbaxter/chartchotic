/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"
#include "../Utils/MidiTypes.h"
#include "../Utils/ChordAnalyzer.h"
#include "../Utils/InstrumentMapper.h"
#include "../Utils/GemCalculator.h"
#include "../Utils/LaneDetector.h"
#include "../DiscoFlipState.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
		MidiInterpreter(Part part, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);

		Part instrumentPart = Part::GUITAR;
		~MidiInterpreter();

		void setDiscoFlipState(const DiscoFlipState* flipState) { discoFlip = flipState; }

		NoteStateMapArray &noteStateMapArray;
		juce::CriticalSection &noteStateMapLock;

		bool isNoteHeld(uint pitch, PPQ position)
		{
			return ChordAnalyzer::isNoteHeld(pitch, position, noteStateMapArray, noteStateMapLock);
		}

		TrackWindow generateTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd);
		SustainWindow generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd);
		TrackFrame generateEmptyTrackFrame();

	private:
		juce::ValueTree &state;
		const DiscoFlipState* discoFlip = nullptr;

		void addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType);
		void addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType);

		// Disco flip: replace cymbal/tom flag while preserving dynamic (ghost/accent)
		static Gem swapCymbalFlag(Gem gem, bool cymbal)
		{
			switch (gem)
			{
				case Gem::CYM_GHOST:
				case Gem::HOPO_GHOST:  return cymbal ? Gem::CYM_GHOST  : Gem::HOPO_GHOST;
				case Gem::CYM:
				case Gem::NOTE:        return cymbal ? Gem::CYM        : Gem::NOTE;
				case Gem::CYM_ACCENT:
				case Gem::TAP_ACCENT:  return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
				default:               return gem;
			}
		}

		// Helper functions for testing
		TrackWindow generateFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ);
		SustainWindow generateFakeSustains(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ);
};