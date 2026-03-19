#include "MidiProcessor.h"
#include "../Pipelines/MidiPipeline.h"
#include "../Providers/REAPER/ReaperMidiProvider.h"
#include "../Utils/MidiConstants.h"

MidiProcessor::MidiProcessor(juce::ValueTree &state,
                             NoteStateMapArray &noteStateMapArray,
                             juce::CriticalSection &noteStateMapLock)
    : state(state),
      noteStateMapArray(noteStateMapArray),
      noteStateMapLock(noteStateMapLock)
{
}

void MidiProcessor::process(juce::MidiBuffer &midiMessages,
                            const juce::AudioPlayHead::PositionInfo &positionInfo,
                            uint blockSizeInSamples,
                            uint latencyInSamples,
                            double sampleRate)
{
    auto ppqPosition = positionInfo.getPpqPosition();
    auto bpm = positionInfo.getBpm();
    auto timeSig = positionInfo.getTimeSignature();

    if (!ppqPosition.hasValue() || !bpm.hasValue() || !timeSig.hasValue()) return;

    PPQ startPPQ = *ppqPosition;
    PPQ endPPQ = startPPQ + calculatePPQSegment(blockSizeInSamples, *bpm, sampleRate);
    PPQ latencyPPQ = calculatePPQSegment(latencyInSamples, *bpm, sampleRate);

    cleanupOldEvents(startPPQ, endPPQ, latencyPPQ);
    processMidiMessages(midiMessages, startPPQ, sampleRate, *bpm);

    lastProcessedPPQ = std::max(endPPQ, lastProcessedPPQ);
}

PPQ MidiProcessor::calculatePPQSegment(uint samples, double bpm, double sampleRate)
{
    double timeInSeconds = samples / sampleRate;
    double beatsPerSecond = bpm / 60.0;
    return PPQ(timeInSeconds * beatsPerSecond);
}


//================================================================================
// CLEANUP
//================================================================================

void MidiProcessor::cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ)
{
    PPQ conservativeStartPPQ = startPPQ - latencyPPQ;
    PPQ conservativeEndPPQ = startPPQ + latencyPPQ;

    PPQ currentVisualStart = PPQ(0.0);
    PPQ currentVisualEnd = PPQ(0.0);
    {
        const juce::ScopedLock lock(visualWindowLock);
        currentVisualStart = visualWindowStartPPQ;
        currentVisualEnd = visualWindowEndPPQ;
    }

    if (currentVisualStart > PPQ(0.0) && currentVisualEnd > PPQ(0.0))
    {
        conservativeStartPPQ = std::min(conservativeStartPPQ, currentVisualStart);
        conservativeEndPPQ = std::max(conservativeEndPPQ, currentVisualEnd);
    }

    {
        const juce::ScopedLock lock(noteStateMapLock);
        for (auto &noteStateMap : noteStateMapArray)
        {
            auto lower = noteStateMap.upper_bound(conservativeStartPPQ);
            if (lower != noteStateMap.begin()) --lower;
            if (lower != noteStateMap.begin()) --lower;
            noteStateMap.erase(noteStateMap.begin(), lower);

            auto upper = noteStateMap.upper_bound(conservativeEndPPQ);
            noteStateMap.erase(upper, noteStateMap.end());
        }
    }
}


//================================================================================
// NOTE STATE MAP
//================================================================================

void MidiProcessor::processMidiMessages(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate, double bpm)
{
    struct NoteMessage {
        juce::MidiMessage message;
        PPQ position;
        uint pitch;
        bool isModifier;
    };

    std::vector<NoteMessage> noteMessages;
    uint numMessages = 0;

    for (const auto message : midiMessages)
    {
        auto midiMessage = message.getMessage();
        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            PPQ messagePositionPPQ = startPPQ + calculatePPQSegment(message.samplePosition, bpm, sampleRate);
            uint pitch = midiMessage.getNoteNumber();
            bool isModifier = InstrumentMapper::isModifier(pitch);
            noteMessages.push_back({midiMessage, messagePositionPPQ, pitch, isModifier});
        }

        if (++numMessages >= MIDI_MAX_MESSAGES_PER_BLOCK) break;
    }

    // Sort: modifiers first, then by time
    std::sort(noteMessages.begin(), noteMessages.end(), [](const NoteMessage& a, const NoteMessage& b) {
        if (a.isModifier != b.isModifier) return a.isModifier > b.isModifier;
        return a.position < b.position;
    });

    // Store raw velocity — no gem calculation
    for (const auto& noteMsg : noteMessages) {
        processNoteMessage(noteMsg.message, noteMsg.position);
    }
}

void MidiProcessor::processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ)
{
    uint noteNumber = midiMessage.getNoteNumber();
    uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0;

    if (midiMessage.isNoteOff()) {
        messagePPQ -= PPQ(1);
    }

    const juce::ScopedLock lock(noteStateMapLock);
    noteStateMapArray[noteNumber][messagePPQ] = NoteData(velocity);
}

void MidiProcessor::clearNoteDataInRange(PPQ startPPQ, PPQ endPPQ)
{
    const juce::ScopedLock lock(noteStateMapLock);

    for (auto& noteStateMap : noteStateMapArray)
    {
        auto lower = noteStateMap.lower_bound(startPPQ);
        auto upper = noteStateMap.upper_bound(endPPQ);
        noteStateMap.erase(lower, upper);
    }
}
