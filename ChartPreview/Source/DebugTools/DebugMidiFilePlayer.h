#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../Midi/Providers/REAPER/MidiCache.h"
#include "../Midi/Processing/NoteProcessor.h"
#include "../Midi/Processing/MidiProcessor.h"
#include "../Utils/Utils.h"
#include "../Utils/TimeConverter.h"

namespace DebugMidiFilePlayer
{

struct LoadedChart
{
    TempoTimeSignatureMap tempoMap;
    double initialBPM = 120.0;
    double lengthInBeats = 0.0;
};

inline int findTrackByName(const juce::MidiFile& midiFile, const juce::String& name)
{
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (!track) continue;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer(i)->message;
            if (msg.isTrackNameEvent() && msg.getTextFromTextMetaEvent() == name)
                return t;
        }
    }
    return -1;
}

inline LoadedChart loadMidiFile(
    const char* data, int dataSize,
    bool isDrums,
    NoteStateMapArray& noteStateMapArray,
    juce::CriticalSection& noteStateMapLock,
    MidiProcessor& midiProcessor,
    juce::ValueTree& state)
{
    LoadedChart result;

    juce::MemoryInputStream stream(data, (size_t)dataSize, false);
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return result;

    short timeFormat = midiFile.getTimeFormat();
    if (timeFormat <= 0)
        return result; // SMPTE not supported

    double ticksPerQN = (double)timeFormat;

    // --- Extract tempo/timesig from track 0 ---
    const auto* tempoTrack = midiFile.getTrack(0);
    if (tempoTrack)
    {
        for (int i = 0; i < tempoTrack->getNumEvents(); ++i)
        {
            const auto& msg = tempoTrack->getEventPointer(i)->message;
            double beatPos = msg.getTimeStamp() / ticksPerQN;

            if (msg.isTempoMetaEvent())
            {
                double bpm = 60.0 / msg.getTempoSecondsPerQuarterNote();

                if (result.tempoMap.empty())
                    result.initialBPM = bpm;

                // Merge with existing timesig at this position if present
                auto it = result.tempoMap.find(PPQ(beatPos));
                if (it != result.tempoMap.end())
                {
                    it->second.bpm = bpm;
                }
                else
                {
                    result.tempoMap[PPQ(beatPos)] = TempoTimeSignatureEvent(
                        PPQ(beatPos), bpm, 4, 4);
                }
            }
            else if (msg.isTimeSignatureMetaEvent())
            {
                int num, denom;
                msg.getTimeSignatureInfo(num, denom);

                auto it = result.tempoMap.find(PPQ(beatPos));
                if (it != result.tempoMap.end())
                {
                    it->second.timeSigNumerator = num;
                    it->second.timeSigDenominator = denom;
                }
                else
                {
                    double bpm = result.initialBPM;
                    if (!result.tempoMap.empty())
                        bpm = result.tempoMap.rbegin()->second.bpm;
                    result.tempoMap[PPQ(beatPos)] = TempoTimeSignatureEvent(
                        PPQ(beatPos), bpm, num, denom);
                }
            }
        }
    }

    // Ensure at least one tempo entry
    if (result.tempoMap.empty())
        result.tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4);

    // --- Find the instrument track ---
    juce::String trackName = isDrums ? "PART DRUMS" : "PART GUITAR";
    int trackIdx = findTrackByName(midiFile, trackName);
    if (trackIdx < 0)
        return result;

    const auto* noteTrack = midiFile.getTrack(trackIdx);
    if (!noteTrack)
        return result;

    // --- Pair note-on/note-off → CachedNote ---
    // Track active note-ons: pitch → (start tick, velocity, channel)
    struct ActiveNote { double startBeat; uint velocity; uint channel; };
    std::map<uint, ActiveNote> activeNotes;
    std::vector<MidiCache::CachedNote> cachedNotes;

    for (int i = 0; i < noteTrack->getNumEvents(); ++i)
    {
        const auto& msg = noteTrack->getEventPointer(i)->message;
        double beatPos = msg.getTimeStamp() / ticksPerQN;

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            uint pitch = (uint)msg.getNoteNumber();
            activeNotes[pitch] = {beatPos, (uint)msg.getVelocity(), (uint)msg.getChannel()};
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            uint pitch = (uint)msg.getNoteNumber();
            auto it = activeNotes.find(pitch);
            if (it != activeNotes.end())
            {
                MidiCache::CachedNote cn;
                cn.startPPQ = PPQ(it->second.startBeat);
                cn.endPPQ = PPQ(beatPos);
                cn.pitch = pitch;
                cn.velocity = it->second.velocity;
                cn.channel = it->second.channel;
                cn.muted = false;
                cn.processed = false;
                cachedNotes.push_back(cn);

                if (beatPos > result.lengthInBeats)
                    result.lengthInBeats = beatPos;

                activeNotes.erase(it);
            }
        }
    }

    // --- Populate noteStateMapArray via NoteProcessor ---
    // CriticalSection is reentrant — hold it for the entire clear+write to prevent
    // the renderer from reading between the clear and the NoteProcessor calls.
    {
        const juce::ScopedLock lock(noteStateMapLock);
        for (auto& map : noteStateMapArray)
            map.clear();

        // Also update MidiProcessor's tempo map
        {
            const juce::ScopedLock tempoLock(midiProcessor.tempoTimeSignatureMapLock);
            midiProcessor.tempoTimeSignatureMap = result.tempoMap;
        }

        NoteProcessor noteProcessor;
        noteProcessor.processModifierNotes(cachedNotes, noteStateMapArray, noteStateMapLock, state);
        noteProcessor.processPlayableNotes(cachedNotes, noteStateMapArray, noteStateMapLock,
                                            midiProcessor, state, result.initialBPM, 44100.0);
    }

    return result;
}

} // namespace DebugMidiFilePlayer

#endif
