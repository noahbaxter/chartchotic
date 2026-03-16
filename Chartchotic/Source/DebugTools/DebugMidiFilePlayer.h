#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../Midi/Providers/REAPER/MidiCache.h"
#include "../Midi/ChartTextEvent.h"
#include "../Utils/Utils.h"
#include "../Utils/TimeConverter.h"

namespace DebugMidiFilePlayer
{

struct LoadedChart
{
    TempoTimeSignatureMap tempoMap;
    double initialBPM = 120.0;
    double lengthInBeats = 0.0;
    std::vector<Part> foundParts;

    // Per-track data keyed by MIDI track name (e.g. "PART GUITAR", "PART DRUMS", "EVENTS")
    std::map<juce::String, std::vector<MidiCache::CachedNote>> trackNotes;
    std::map<juce::String, TrackTextEvents> trackTextEvents;
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

// Pure parser — extracts structured data from a MIDI file.
// No NoteProcessor, no NoteStateMapArray, no state mutation.
inline LoadedChart loadMidiFile(const char* data, int dataSize)
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

    // --- Detect which instrument parts exist ---
    // Detect instrument parts — order matches Part enum / display priority
    if (findTrackByName(midiFile, "PART GUITAR") >= 0)
        result.foundParts.push_back(Part::GUITAR);
    if (findTrackByName(midiFile, "PART BASS") >= 0)
        result.foundParts.push_back(Part::BASS);
    if (findTrackByName(midiFile, "PART KEYS") >= 0)
        result.foundParts.push_back(Part::KEYS);
    if (findTrackByName(midiFile, "PART DRUMS") >= 0 || findTrackByName(midiFile, "PART DRUM") >= 0)
        result.foundParts.push_back(Part::DRUMS);
    if (findTrackByName(midiFile, "PART VOCALS") >= 0)
        result.foundParts.push_back(Part::VOCALS);

    // --- Parse ALL tracks into per-track note + text event data ---
    struct ActiveNote { double startBeat; uint velocity; uint channel; };
    std::map<uint, ActiveNote> activeNotes;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (!track) continue;

        // Detect track name
        juce::String trackName;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer(i)->message;
            if (msg.isTrackNameEvent())
            {
                trackName = msg.getTextFromTextMetaEvent();
                break;
            }
        }

        activeNotes.clear();

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer(i)->message;
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

                    if (trackName.isNotEmpty())
                        result.trackNotes[trackName].push_back(cn);

                    if (beatPos > result.lengthInBeats)
                        result.lengthInBeats = beatPos;

                    activeNotes.erase(it);
                }
            }
            else if (msg.isTextMetaEvent())
            {
                ChartTextEvent textEvt;
                textEvt.position = PPQ(beatPos);
                textEvt.text = msg.getTextFromTextMetaEvent();
                if (trackName.isNotEmpty())
                    result.trackTextEvents[trackName].push_back(textEvt);
            }
        }
    }

    return result;
}

} // namespace DebugMidiFilePlayer

#endif
