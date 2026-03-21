#pragma once
#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/TimeConverter.h"
#include "../Utils/MidiTypes.h"
#include "../Utils/InstrumentMapper.h"

class MidiProcessor
{
public:
    MidiProcessor(juce::ValueTree &state,
                  NoteStateMapArray &noteStateMapArray,
                  juce::CriticalSection &noteStateMapLock);

    void process(juce::MidiBuffer &midiMessages,
                 const juce::AudioPlayHead::PositionInfo &positionInfo,
                 uint blockSizeInSamples,
                 uint latencyInSamples,
                 double sampleRate);

    // References into MidiProject — not owned
    NoteStateMapArray &noteStateMapArray;
    juce::CriticalSection &noteStateMapLock;

    PPQ lastProcessedPPQ = 0.0;

    void setLastProcessedPosition(const juce::AudioPlayHead::PositionInfo &positionInfo)
    {
        lastProcessedPPQ = positionInfo.getPpqPosition().orFallback(lastProcessedPPQ);
    }

    void setVisualWindowBounds(PPQ startPPQ, PPQ endPPQ)
    {
        const juce::ScopedLock lock(visualWindowLock);
        visualWindowStartPPQ = startPPQ;
        visualWindowEndPPQ = endPPQ;
    }

    void clearNoteDataInRange(PPQ startPPQ, PPQ endPPQ);

private:
    juce::ValueTree &state;

    PPQ calculatePPQSegment(uint samples, double bpm, double sampleRate);
    void cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ);
    void processMidiMessages(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate, double bpm);
    void processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ);

    PPQ visualWindowStartPPQ = PPQ(0.0);
    PPQ visualWindowEndPPQ = PPQ(0.0);
    mutable juce::CriticalSection visualWindowLock;
};
