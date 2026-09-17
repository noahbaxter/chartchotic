/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/MidiTypes.h"
#include "../Utils/InstrumentMapper.h"
#include "../DiscoFlipState.h"
#include "../StrictHatPedalState.h"
#include "PartWindow.h"
#include "TrackResolver.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
		MidiInterpreter(Part part, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);

		Part instrumentPart = Part::GUITAR;
		~MidiInterpreter();

		void setDiscoFlipState(const DiscoFlipState* flipState) { discoFlip = flipState; }
		const DiscoFlipState* getDiscoFlipState() const { return discoFlip; }

		void setStrictHatPedalState(const StrictHatPedalState* s) { strictHatPedal = s; }
		const StrictHatPedalState* getStrictHatPedalState() const { return strictHatPedal; }

		const juce::ValueTree& getState() const { return state; }

		// Phrases as InstrumentSession read them off this part's OWN track. The note state map
		// is parsed from whichever track the plugin sits on, which is not necessarily this
		// part's, so a phrase written here would otherwise never reach the highway: neither as
		// a band nor as the star power that decides whether a gem draws as an SP gem.
		struct PhraseOverride
		{
			bool active = false;
			std::vector<ModifierRange> starPower;
			std::vector<ModifierRange> solo;
			std::vector<ModifierRange> fill;
			std::vector<ModifierRange> bre;
		};
		void setPhraseOverride(PhraseOverride o) { phraseOverride = std::move(o); }

		NoteStateMapArray &noteStateMapArray;
		juce::CriticalSection &noteStateMapLock;

		// Resolve all 4 difficulties in one pass, one lock
		PartWindow resolveAllDifficulties(PPQ windowStart, PPQ windowEnd, PPQ latencyEnd);

	private:
		juce::ValueTree &state;
		const DiscoFlipState* discoFlip = nullptr;
		const StrictHatPedalState* strictHatPedal = nullptr;
		PhraseOverride phraseOverride;
};
