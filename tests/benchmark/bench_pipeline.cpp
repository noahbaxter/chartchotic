/*
  ==============================================================================

    bench_pipeline.cpp
    MIDI processing pipeline benchmark for Chartchotic.

    Measures: MIDI parsing, NoteProcessor, GemCalculator, MidiInterpreter
    window generation, and setting-change simulation.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Midi/Processing/NoteProcessor.h"
#include "Midi/Processing/MidiProcessor.h"
#include "Midi/Processing/MidiInterpreter.h"
#include "Midi/Utils/GemCalculator.h"
#include "Midi/Utils/InstrumentMapper.h"
#include "Midi/Utils/MidiTypes.h"
#include "Midi/Providers/REAPER/MidiCache.h"
#include "Midi/ChartTextEvent.h"
#include "Utils/Utils.h"
#include "Utils/TimeConverter.h"
#include "Visual/HighwayComponent.h"
#include "Visual/Managers/AssetManager.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <map>
#include <fstream>

//==============================================================================
// MIDI file parser (inlined from DebugMidiFilePlayer without #ifdef DEBUG)

namespace MidiFileParser
{

struct LoadedChart
{
    TempoTimeSignatureMap tempoMap;
    double initialBPM = 120.0;
    double lengthInBeats = 0.0;
    std::vector<Part> foundParts;
    std::map<juce::String, std::vector<MidiCache::CachedNote>> trackNotes;
    std::map<juce::String, TrackTextEvents> trackTextEvents;
};

static int findTrackByName(const juce::MidiFile& midiFile, const juce::String& name)
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

static LoadedChart loadMidiFile(const char* data, int dataSize)
{
    LoadedChart result;

    juce::MemoryInputStream stream(data, (size_t)dataSize, false);
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return result;

    short timeFormat = midiFile.getTimeFormat();
    if (timeFormat <= 0)
        return result;

    double ticksPerQN = (double)timeFormat;

    // Extract tempo/timesig from track 0
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
                    it->second.bpm = bpm;
                else
                    result.tempoMap[PPQ(beatPos)] = TempoTimeSignatureEvent(PPQ(beatPos), bpm, 4, 4);
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
                    result.tempoMap[PPQ(beatPos)] = TempoTimeSignatureEvent(PPQ(beatPos), bpm, num, denom);
                }
            }
        }
    }

    if (result.tempoMap.empty())
        result.tempoMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4);

    // Detect instrument parts
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

    // Parse ALL tracks
    struct ActiveNote { double startBeat; uint velocity; uint channel; };
    std::map<uint, ActiveNote> activeNotes;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (!track) continue;

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

} // namespace MidiFileParser

//==============================================================================
// Stats

struct TimingStats
{
    double mean = 0;
    double p95 = 0;
    double max = 0;
};

static TimingStats computeStats(std::vector<double> samples)
{
    if (samples.empty()) return {};
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (auto s : samples) sum += s;

    TimingStats st;
    st.mean = sum / samples.size();
    st.p95 = samples[(size_t)(samples.size() * 0.95)];
    st.max = samples.back();
    return st;
}

//==============================================================================
// Helpers

using Clock = std::chrono::high_resolution_clock;

static double usElapsed(Clock::time_point t0, Clock::time_point t1)
{
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

static const char* partName(Part p)
{
    switch (p) {
        case Part::GUITAR: return "GUITAR";
        case Part::BASS:   return "BASS";
        case Part::KEYS:   return "KEYS";
        case Part::DRUMS:  return "DRUMS";
        default:           return "OTHER";
    }
}

static const char* skillName(SkillLevel s)
{
    switch (s) {
        case SkillLevel::EASY:   return "EASY";
        case SkillLevel::MEDIUM: return "MEDIUM";
        case SkillLevel::HARD:   return "HARD";
        case SkillLevel::EXPERT: return "EXPERT";
    }
    return "?";
}

static juce::String trackNameForPart(Part p)
{
    switch (p) {
        case Part::GUITAR: return "PART GUITAR";
        case Part::BASS:   return "PART BASS";
        case Part::KEYS:   return "PART KEYS";
        case Part::DRUMS:  return "PART DRUMS";
        default:           return "";
    }
}

static constexpr int ITERATIONS = 10;

//==============================================================================
// Table printing

static void printSectionHeader(const char* name)
{
    std::cout << std::endl;
    std::cout << "=== " << name << " ===" << std::endl;
    std::cout << std::endl;
}

static void printRow(const char* label, double meanUs, double p95Us, double maxUs)
{
    std::cout << std::left << std::setw(40) << label
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(10) << meanUs
              << std::setw(10) << p95Us
              << std::setw(10) << maxUs
              << std::endl;
}

static void printRowWithCount(const char* label, double meanUs, double p95Us, double maxUs, size_t count, double usPerNote)
{
    std::cout << std::left << std::setw(40) << label
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(10) << meanUs
              << std::setw(10) << p95Us
              << std::setw(10) << maxUs
              << std::setw(8) << count
              << std::setw(10) << std::setprecision(3) << usPerNote
              << std::endl;
}

//==============================================================================

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Load ALL MIDI files from the asset directory
    std::string midiDir = CHARTCHOTIC_MIDI_ASSET_DIR;

    juce::File midiDirFile(midiDir);
    auto midiFiles = midiDirFile.findChildFiles(juce::File::findFiles, false, "*.mid");

    if (midiFiles.isEmpty())
    {
        std::cerr << "No .mid files found in: " << midiDir << std::endl;
        return 1;
    }

    // Sort for deterministic output
    std::sort(midiFiles.begin(), midiFiles.end(),
              [](const juce::File& a, const juce::File& b) {
                  return a.getFileName() < b.getFileName();
              });

    struct LoadedFile
    {
        juce::String filename;
        juce::MemoryBlock data;
        MidiFileParser::LoadedChart chart;
        double parseTimeUs = 0.0;
    };

    std::vector<LoadedFile> allFiles;
    LoadedFile* embeddedFile = nullptr;

    //==========================================================================
    // Phase 1: MIDI File Parsing (all files)
    //==========================================================================
    printSectionHeader("Phase 1: MIDI File Parsing (all files)");

    std::cout << std::left << std::setw(30) << "File"
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::setw(10) << "tracks"
              << std::setw(10) << "notes"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& f : midiFiles)
    {
        LoadedFile lf;
        lf.filename = f.getFileName();
        f.loadFileAsData(lf.data);

        std::vector<double> parseSamples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();
            lf.chart = MidiFileParser::loadMidiFile((const char*)lf.data.getData(), (int)lf.data.getSize());
            auto t1 = Clock::now();
            parseSamples.push_back(usElapsed(t0, t1));
        }

        auto parseStats = computeStats(parseSamples);
        lf.parseTimeUs = parseStats.mean;

        size_t totalNotes = 0;
        for (const auto& [name, notes] : lf.chart.trackNotes)
            totalNotes += notes.size();

        std::cout << std::left << std::setw(30) << lf.filename.toStdString()
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(10) << parseStats.mean
                  << std::setw(10) << parseStats.p95
                  << std::setw(10) << parseStats.max
                  << std::setw(10) << lf.chart.trackNotes.size()
                  << std::setw(10) << totalNotes
                  << std::endl;

        allFiles.push_back(std::move(lf));
    }

    // Find embedded.mid for existing phases 3-5
    for (auto& lf : allFiles)
    {
        if (lf.filename == "embedded.mid")
        {
            embeddedFile = &lf;
            break;
        }
    }

    if (embeddedFile == nullptr)
    {
        std::cerr << "embedded.mid not found in asset directory, phases 3-5 will be skipped" << std::endl;
    }

    // Print detailed info for embedded.mid
    if (embeddedFile != nullptr)
    {
        auto& chart = embeddedFile->chart;
        std::cout << std::endl;
        std::cout << "--- embedded.mid details ---" << std::endl;
        std::cout << "Song length: " << std::fixed << std::setprecision(1) << chart.lengthInBeats << " beats" << std::endl;
        std::cout << "Initial BPM: " << chart.initialBPM << std::endl;
        std::cout << "Tempo events: " << chart.tempoMap.size() << std::endl;
        std::cout << "Found parts:";
        for (auto p : chart.foundParts)
            std::cout << " " << partName(p);
        std::cout << std::endl;

        std::cout << std::endl << "Track note counts:" << std::endl;
        for (const auto& [name, notes] : chart.trackNotes)
            std::cout << "  " << name << ": " << notes.size() << " notes" << std::endl;
    }

    //==========================================================================
    // Phase 1b: NoteProcessor across all files (largest tracks)
    //==========================================================================
    printSectionHeader("Phase 1b: NoteProcessor across all files (largest tracks)");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::setw(8) << "notes"
              << std::endl;
    std::cout << std::string(78, '-') << std::endl;

    // Shared MidiProcessor for all Phase 1b/2 runs
    juce::ValueTree dummyState1b("PluginState");
    dummyState1b.setProperty("part", (int)Part::GUITAR, nullptr);
    dummyState1b.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    dummyState1b.setProperty("drumType", (int)DrumType::PRO, nullptr);
    dummyState1b.setProperty("autoHopo", true, nullptr);
    dummyState1b.setProperty("hopoThreshold", 2, nullptr);
    dummyState1b.setProperty("starPower", 0, nullptr);
    dummyState1b.setProperty("dynamics", 1, nullptr);
    dummyState1b.setProperty("kick2x", 1, nullptr);
    dummyState1b.setProperty("discoFlip", 0, nullptr);
    MidiProcessor midiProcessor1b(dummyState1b);

    for (auto& lf : allFiles)
    {
        // Find largest guitar-like and drum-like tracks
        const std::vector<MidiCache::CachedNote>* largestGuitar = nullptr;
        const std::vector<MidiCache::CachedNote>* largestDrum = nullptr;
        juce::String largestGuitarName, largestDrumName;
        Part guitarPart = Part::GUITAR, drumPart = Part::DRUMS;

        for (const auto& [name, notes] : lf.chart.trackNotes)
        {
            Part p = Part::GUITAR;
            if (name == "PART GUITAR") p = Part::GUITAR;
            else if (name == "PART BASS") p = Part::BASS;
            else if (name == "PART KEYS") p = Part::KEYS;
            else if (name == "PART DRUMS" || name == "PART DRUM") p = Part::DRUMS;
            else continue;

            if (isGuitarLike(p))
            {
                if (!largestGuitar || notes.size() > largestGuitar->size())
                {
                    largestGuitar = &notes;
                    largestGuitarName = name;
                    guitarPart = p;
                }
            }
            else if (isDrumLike(p))
            {
                if (!largestDrum || notes.size() > largestDrum->size())
                {
                    largestDrum = &notes;
                    largestDrumName = name;
                    drumPart = p;
                }
            }
        }

        auto benchTrack = [&](const std::vector<MidiCache::CachedNote>* track,
                              const juce::String& trackName, Part part)
        {
            if (!track || track->empty()) return;

            juce::ValueTree state("PluginState");
            state.setProperty("part", (int)part, nullptr);
            state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
            state.setProperty("drumType", (int)DrumType::PRO, nullptr);
            state.setProperty("autoHopo", true, nullptr);
            state.setProperty("hopoThreshold", 2, nullptr);
            state.setProperty("starPower", 0, nullptr);
            state.setProperty("dynamics", 1, nullptr);
            state.setProperty("kick2x", 1, nullptr);
            state.setProperty("discoFlip", 0, nullptr);

            NoteProcessor noteProcessor;
            std::vector<double> samples;

            for (int i = 0; i < ITERATIONS; ++i)
            {
                NoteStateMapArray nsma{};
                juce::CriticalSection lock;

                auto t0 = Clock::now();
                noteProcessor.processModifierNotes(*track, nsma, lock, state);
                noteProcessor.processPlayableNotes(*track, nsma, lock, midiProcessor1b, state,
                                                   lf.chart.initialBPM, 44100.0);
                auto t1 = Clock::now();
                samples.push_back(usElapsed(t0, t1));
            }

            auto stats = computeStats(samples);
            std::string label = lf.filename.toStdString() + " " + trackName.toStdString();
            printRowWithCount(label.c_str(), stats.mean, stats.p95, stats.max, track->size(), stats.mean / track->size());
        };

        benchTrack(largestGuitar, largestGuitarName, guitarPart);
        benchTrack(largestDrum, largestDrumName, drumPart);
    }

    // Use embedded.mid chart for phases 2-5 (backward compat)
    MidiFileParser::LoadedChart& chart = embeddedFile != nullptr
        ? embeddedFile->chart
        : allFiles[0].chart;

    //==========================================================================
    // Phase 2: NoteProcessor
    //==========================================================================
    printSectionHeader("Phase 2: NoteProcessor (per instrument, per difficulty)");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::setw(8) << "notes"
              << std::setw(10) << "us/note"
              << std::endl;
    std::cout << std::string(88, '-') << std::endl;

    SkillLevel skills[] = { SkillLevel::EASY, SkillLevel::MEDIUM, SkillLevel::HARD, SkillLevel::EXPERT };

    // We need a MidiProcessor for the processPlayableNotes signature (unused internally but required)
    juce::ValueTree dummyState("PluginState");
    dummyState.setProperty("part", (int)Part::GUITAR, nullptr);
    dummyState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    dummyState.setProperty("drumType", (int)DrumType::PRO, nullptr);
    dummyState.setProperty("autoHopo", true, nullptr);
    dummyState.setProperty("hopoThreshold", 2, nullptr);
    dummyState.setProperty("starPower", 0, nullptr);
    dummyState.setProperty("dynamics", 1, nullptr);
    dummyState.setProperty("kick2x", 1, nullptr);
    dummyState.setProperty("discoFlip", 0, nullptr);

    MidiProcessor midiProcessor(dummyState);

    for (Part part : chart.foundParts)
    {
        if (!isGuitarLike(part) && !isDrumLike(part))
            continue;

        juce::String trackName = trackNameForPart(part);
        auto trackIt = chart.trackNotes.find(trackName);
        if (trackIt == chart.trackNotes.end())
            continue;

        const auto& notes = trackIt->second;

        for (SkillLevel skill : skills)
        {
            // Configure state for this combination
            juce::ValueTree state("PluginState");
            state.setProperty("part", (int)part, nullptr);
            state.setProperty("skillLevel", (int)skill, nullptr);
            state.setProperty("drumType", (int)DrumType::PRO, nullptr);
            state.setProperty("autoHopo", true, nullptr);
            state.setProperty("hopoThreshold", 2, nullptr);
            state.setProperty("starPower", 0, nullptr);
            state.setProperty("dynamics", 1, nullptr);
            state.setProperty("kick2x", 1, nullptr);
            state.setProperty("discoFlip", 0, nullptr);

            NoteProcessor noteProcessor;

            std::vector<double> modSamples, playSamples;
            size_t playableCount = 0;

            for (int i = 0; i < ITERATIONS; ++i)
            {
                NoteStateMapArray nsma{};
                juce::CriticalSection lock;

                // Time modifier processing
                auto t0 = Clock::now();
                noteProcessor.processModifierNotes(notes, nsma, lock, state);
                auto t1 = Clock::now();
                modSamples.push_back(usElapsed(t0, t1));

                // Time playable processing
                auto t2 = Clock::now();
                noteProcessor.processPlayableNotes(notes, nsma, lock, midiProcessor, state, chart.initialBPM, 44100.0);
                auto t3 = Clock::now();
                playSamples.push_back(usElapsed(t2, t3));

                // Count playable notes on last iteration
                if (i == ITERATIONS - 1)
                {
                    for (uint p = 0; p < 128; ++p)
                        for (const auto& [ppq, data] : nsma[p])
                            if (data.velocity > 0) playableCount++;
                }
            }

            auto modStats = computeStats(modSamples);
            auto playStats = computeStats(playSamples);
            double usPerNote = playableCount > 0 ? playStats.mean / playableCount : 0.0;

            std::string modLabel = std::string(partName(part)) + " " + skillName(skill) + " modifiers";
            std::string playLabel = std::string(partName(part)) + " " + skillName(skill) + " playable";

            printRow(modLabel.c_str(), modStats.mean, modStats.p95, modStats.max);
            printRowWithCount(playLabel.c_str(), playStats.mean, playStats.p95, playStats.max, playableCount, usPerNote);
        }
    }

    //==========================================================================
    // Phase 3: GemCalculator isolation
    //==========================================================================
    printSectionHeader("Phase 3: GemCalculator isolation (Expert)");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::setw(8) << "notes"
              << std::setw(10) << "us/note"
              << std::endl;
    std::cout << std::string(88, '-') << std::endl;

    for (Part part : chart.foundParts)
    {
        if (!isGuitarLike(part) && !isDrumLike(part))
            continue;

        juce::String trackName = trackNameForPart(part);
        auto trackIt = chart.trackNotes.find(trackName);
        if (trackIt == chart.trackNotes.end())
            continue;

        const auto& notes = trackIt->second;

        juce::ValueTree state("PluginState");
        state.setProperty("part", (int)part, nullptr);
        state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        state.setProperty("drumType", (int)DrumType::PRO, nullptr);
        state.setProperty("autoHopo", true, nullptr);
        state.setProperty("hopoThreshold", 2, nullptr);
        state.setProperty("starPower", 0, nullptr);
        state.setProperty("dynamics", 1, nullptr);
        state.setProperty("kick2x", 1, nullptr);
        state.setProperty("discoFlip", 0, nullptr);

        // First, populate the NoteStateMapArray with modifiers so GemCalculator has context
        NoteProcessor noteProcessor;
        NoteStateMapArray nsma{};
        juce::CriticalSection lock;
        noteProcessor.processModifierNotes(notes, nsma, lock, state);

        // Collect playable notes for this instrument at Expert
        std::vector<uint> validPitches;
        if (isGuitarLike(part))
            validPitches = InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::EXPERT);
        else
            validPitches = InstrumentMapper::getDrumPitchesForSkill(SkillLevel::EXPERT);

        struct PlayableNote { uint pitch; PPQ position; uint velocity; };
        std::vector<PlayableNote> playableNotes;
        for (const auto& note : notes)
        {
            if (note.muted || note.velocity == 0) continue;
            bool valid = std::find(validPitches.begin(), validPitches.end(), note.pitch) != validPitches.end();
            if (valid)
                playableNotes.push_back({note.pitch, note.startPPQ, note.velocity});
        }

        std::vector<double> gemSamples;
        size_t gemCallCount = playableNotes.size();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();
            for (const auto& pn : playableNotes)
            {
                if (isGuitarLike(part))
                    GemCalculator::getGuitarGemType(pn.pitch, pn.position, state, nsma, lock);
                else
                    GemCalculator::getDrumGemType(pn.pitch, pn.position, (Dynamic)pn.velocity, state, nsma, lock);
            }
            auto t1 = Clock::now();
            gemSamples.push_back(usElapsed(t0, t1));
        }

        auto gemStats = computeStats(gemSamples);
        double usPerCall = gemCallCount > 0 ? gemStats.mean / gemCallCount : 0.0;

        std::string label = std::string(partName(part)) + " GemCalculator";
        printRowWithCount(label.c_str(), gemStats.mean, gemStats.p95, gemStats.max, gemCallCount, usPerCall);
    }

    //==========================================================================
    // Phase 4: MidiInterpreter window generation
    //==========================================================================
    printSectionHeader("Phase 4: MidiInterpreter window generation");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    // Build a fully-populated NoteStateMapArray for a guitar track at Expert
    for (Part part : chart.foundParts)
    {
        if (!isGuitarLike(part) && !isDrumLike(part))
            continue;

        juce::String trackName = trackNameForPart(part);
        auto trackIt = chart.trackNotes.find(trackName);
        if (trackIt == chart.trackNotes.end())
            continue;

        const auto& notes = trackIt->second;

        juce::ValueTree state("PluginState");
        state.setProperty("part", (int)part, nullptr);
        state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        state.setProperty("drumType", (int)DrumType::PRO, nullptr);
        state.setProperty("autoHopo", true, nullptr);
        state.setProperty("hopoThreshold", 2, nullptr);
        state.setProperty("starPower", 0, nullptr);
        state.setProperty("dynamics", 1, nullptr);
        state.setProperty("kick2x", 1, nullptr);
        state.setProperty("discoFlip", 0, nullptr);

        NoteProcessor noteProcessor;
        NoteStateMapArray nsma{};
        juce::CriticalSection lock;
        noteProcessor.processModifierNotes(notes, nsma, lock, state);
        noteProcessor.processPlayableNotes(notes, nsma, lock, midiProcessor, state, chart.initialBPM, 44100.0);

        MidiInterpreter interpreter(part, state, nsma, lock);

        double songLen = chart.lengthInBeats;
        double windowSize = 2.0; // 2 beats window

        struct WindowPos { const char* name; double start; };
        WindowPos positions[] = {
            {"start",  0.0},
            {"middle", songLen / 2.0},
            {"end",    std::max(0.0, songLen - windowSize)},
        };

        for (const auto& pos : positions)
        {
            PPQ wStart(pos.start);
            PPQ wEnd(pos.start + windowSize);

            // Track window
            {
                std::vector<double> samples;
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    auto t0 = Clock::now();
                    auto tw = interpreter.generateTrackWindow(wStart, wEnd);
                    auto t1 = Clock::now();
                    samples.push_back(usElapsed(t0, t1));
                }
                auto stats = computeStats(samples);
                std::string label = std::string(partName(part)) + " trackWindow @" + pos.name;
                printRow(label.c_str(), stats.mean, stats.p95, stats.max);
            }

            // Sustain window
            {
                PPQ latencyEnd = wEnd + PPQ(4.0);
                std::vector<double> samples;
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    auto t0 = Clock::now();
                    auto sw = interpreter.generateSustainWindow(wStart, wEnd, latencyEnd);
                    auto t1 = Clock::now();
                    samples.push_back(usElapsed(t0, t1));
                }
                auto stats = computeStats(samples);
                std::string label = std::string(partName(part)) + " sustainWindow @" + pos.name;
                printRow(label.c_str(), stats.mean, stats.p95, stats.max);
            }
        }
    }

    //==========================================================================
    // Phase 5: Setting change simulation (drums)
    //==========================================================================
    printSectionHeader("Phase 5: Setting change simulation (drums Expert)");

    {
        juce::String drumsTrack = "PART DRUMS";
        auto trackIt = chart.trackNotes.find(drumsTrack);

        if (trackIt != chart.trackNotes.end())
        {
            const auto& notes = trackIt->second;

            std::cout << std::left << std::setw(40) << ""
                      << std::right << std::setw(10) << "mean"
                      << std::setw(10) << "p95"
                      << std::setw(10) << "max"
                      << std::endl;
            std::cout << std::string(70, '-') << std::endl;

            // Run 1: PRO drums
            {
                juce::ValueTree state("PluginState");
                state.setProperty("part", (int)Part::DRUMS, nullptr);
                state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
                state.setProperty("drumType", (int)DrumType::PRO, nullptr);
                state.setProperty("autoHopo", true, nullptr);
                state.setProperty("hopoThreshold", 2, nullptr);
                state.setProperty("starPower", 0, nullptr);
                state.setProperty("dynamics", 1, nullptr);
                state.setProperty("kick2x", 1, nullptr);
                state.setProperty("discoFlip", 0, nullptr);

                NoteProcessor noteProcessor;

                std::vector<double> samples;
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    NoteStateMapArray nsma{};
                    juce::CriticalSection lock;

                    auto t0 = Clock::now();
                    noteProcessor.processModifierNotes(notes, nsma, lock, state);
                    noteProcessor.processPlayableNotes(notes, nsma, lock, midiProcessor, state, chart.initialBPM, 44100.0);
                    auto t1 = Clock::now();
                    samples.push_back(usElapsed(t0, t1));
                }
                auto stats = computeStats(samples);
                printRow("drumType=PRO full reprocess", stats.mean, stats.p95, stats.max);
            }

            // Run 2: NORMAL drums (simulating toggle)
            {
                juce::ValueTree state("PluginState");
                state.setProperty("part", (int)Part::DRUMS, nullptr);
                state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
                state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
                state.setProperty("autoHopo", true, nullptr);
                state.setProperty("hopoThreshold", 2, nullptr);
                state.setProperty("starPower", 0, nullptr);
                state.setProperty("dynamics", 1, nullptr);
                state.setProperty("kick2x", 1, nullptr);
                state.setProperty("discoFlip", 0, nullptr);

                NoteProcessor noteProcessor;

                std::vector<double> samples;
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    NoteStateMapArray nsma{};
                    juce::CriticalSection lock;

                    auto t0 = Clock::now();
                    noteProcessor.processModifierNotes(notes, nsma, lock, state);
                    noteProcessor.processPlayableNotes(notes, nsma, lock, midiProcessor, state, chart.initialBPM, 44100.0);
                    auto t1 = Clock::now();
                    samples.push_back(usElapsed(t0, t1));
                }
                auto stats = computeStats(samples);
                printRow("drumType=NORMAL full reprocess", stats.mean, stats.p95, stats.max);
            }

            // Total setting-change latency = both back-to-back
            {
                juce::ValueTree state("PluginState");
                state.setProperty("part", (int)Part::DRUMS, nullptr);
                state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
                state.setProperty("drumType", (int)DrumType::PRO, nullptr);
                state.setProperty("autoHopo", true, nullptr);
                state.setProperty("hopoThreshold", 2, nullptr);
                state.setProperty("starPower", 0, nullptr);
                state.setProperty("dynamics", 1, nullptr);
                state.setProperty("kick2x", 1, nullptr);
                state.setProperty("discoFlip", 0, nullptr);

                NoteProcessor noteProcessor;

                std::vector<double> samples;
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    auto t0 = Clock::now();

                    // First run: PRO
                    NoteStateMapArray nsma1{};
                    juce::CriticalSection lock1;
                    state.setProperty("drumType", (int)DrumType::PRO, nullptr);
                    noteProcessor.processModifierNotes(notes, nsma1, lock1, state);
                    noteProcessor.processPlayableNotes(notes, nsma1, lock1, midiProcessor, state, chart.initialBPM, 44100.0);

                    // Second run: NORMAL (toggle)
                    NoteStateMapArray nsma2{};
                    juce::CriticalSection lock2;
                    state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
                    noteProcessor.processModifierNotes(notes, nsma2, lock2, state);
                    noteProcessor.processPlayableNotes(notes, nsma2, lock2, midiProcessor, state, chart.initialBPM, 44100.0);

                    auto t1 = Clock::now();
                    samples.push_back(usElapsed(t0, t1));
                }
                auto stats = computeStats(samples);
                printRow("Setting change (PRO->NORMAL)", stats.mean, stats.p95, stats.max);
            }
        }
        else
        {
            std::cout << "No PART DRUMS track found, skipping Phase 5" << std::endl;
        }
    }

    //==========================================================================
    // Phase 6: HighwayComponent construction + rebuildTrack
    //==========================================================================
    printSectionHeader("Phase 6: HighwayComponent construction + rebuildTrack");

    {
        AssetManager assetManager; // no BinaryData loaded (CHARTCHOTIC_NO_BINARY_DATA)

        std::cout << std::left << std::setw(40) << ""
                  << std::right << std::setw(10) << "mean"
                  << std::setw(10) << "p95"
                  << std::setw(10) << "max"
                  << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        // 6a: HighwayComponent construction
        {
            juce::ValueTree hwState("PluginState");
            hwState.setProperty("part", (int)Part::GUITAR, nullptr);
            hwState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
            hwState.setProperty("drumType", (int)DrumType::PRO, nullptr);
            hwState.setProperty("autoHopo", true, nullptr);
            hwState.setProperty("hopoThreshold", 2, nullptr);
            hwState.setProperty("starPower", 0, nullptr);
            hwState.setProperty("dynamics", 1, nullptr);
            hwState.setProperty("kick2x", 1, nullptr);
            hwState.setProperty("discoFlip", 0, nullptr);

            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                auto t0 = Clock::now();
                auto hc = std::make_unique<HighwayComponent>(hwState, assetManager);
                auto t1 = Clock::now();
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            printRow("HighwayComponent ctor", stats.mean, stats.p95, stats.max);
        }

        // 6b: rebuildTrack at multiple resolutions
        struct Resolution { int w; int h; const char* name; };
        Resolution resolutions[] = {
            {600,  800,  "600x800"},
            {960,  720,  "960x720"},
            {1440, 1080, "1440x1080"},
        };

        for (const auto& res : resolutions)
        {
            juce::ValueTree hwState("PluginState");
            hwState.setProperty("part", (int)Part::GUITAR, nullptr);
            hwState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
            hwState.setProperty("drumType", (int)DrumType::PRO, nullptr);
            hwState.setProperty("autoHopo", true, nullptr);
            hwState.setProperty("hopoThreshold", 2, nullptr);
            hwState.setProperty("starPower", 0, nullptr);
            hwState.setProperty("dynamics", 1, nullptr);
            hwState.setProperty("kick2x", 1, nullptr);
            hwState.setProperty("discoFlip", 0, nullptr);

            HighwayComponent hc(hwState, assetManager);
            hc.renderWidth = res.w;
            hc.renderHeight = res.h;

            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                hc.getTrackRenderer().invalidate();
                auto t0 = Clock::now();
                hc.rebuildTrack();
                auto t1 = Clock::now();
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            std::string label = std::string("rebuildTrack @") + res.name;
            printRow(label.c_str(), stats.mean, stats.p95, stats.max);
        }

        // 6c: setActivePart cost
        {
            juce::ValueTree hwState("PluginState");
            hwState.setProperty("part", (int)Part::GUITAR, nullptr);
            hwState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
            hwState.setProperty("drumType", (int)DrumType::PRO, nullptr);
            hwState.setProperty("autoHopo", true, nullptr);
            hwState.setProperty("hopoThreshold", 2, nullptr);
            hwState.setProperty("starPower", 0, nullptr);
            hwState.setProperty("dynamics", 1, nullptr);
            hwState.setProperty("kick2x", 1, nullptr);
            hwState.setProperty("discoFlip", 0, nullptr);

            HighwayComponent hc(hwState, assetManager);

            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                auto t0 = Clock::now();
                hc.setActivePart(Part::DRUMS);
                hc.setActivePart(Part::GUITAR);
                auto t1 = Clock::now();
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            printRow("setActivePart (2x toggle)", stats.mean, stats.p95, stats.max);
        }

        // 6d: Full rebuildVisibleSlots simulation (1, 2, 4 highways)
        for (int slotCount : {1, 2, 4})
        {
            juce::ValueTree hwState("PluginState");
            hwState.setProperty("part", (int)Part::GUITAR, nullptr);
            hwState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
            hwState.setProperty("drumType", (int)DrumType::PRO, nullptr);
            hwState.setProperty("autoHopo", true, nullptr);
            hwState.setProperty("hopoThreshold", 2, nullptr);
            hwState.setProperty("starPower", 0, nullptr);
            hwState.setProperty("dynamics", 1, nullptr);
            hwState.setProperty("kick2x", 1, nullptr);
            hwState.setProperty("discoFlip", 0, nullptr);

            std::vector<std::unique_ptr<HighwayComponent>> slots;
            for (int s = 0; s < slotCount; ++s)
            {
                auto hc = std::make_unique<HighwayComponent>(hwState, assetManager);
                hc->renderWidth = 960;
                hc->renderHeight = 720;
                slots.push_back(std::move(hc));
            }

            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                for (auto& hc : slots)
                    hc->getTrackRenderer().invalidate();

                auto t0 = Clock::now();
                for (auto& hc : slots)
                    hc->rebuildTrack();
                auto t1 = Clock::now();
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            std::string label = std::to_string(slotCount) + " highway(s) rebuild @960x720";
            printRow(label.c_str(), stats.mean, stats.p95, stats.max);
        }
    }

    //==========================================================================
    // Phase 7: NoteStateMapArray operations
    //==========================================================================
    printSectionHeader("Phase 7: NoteStateMapArray operations");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    // 7a: Clear fully populated NoteStateMapArray
    {
        std::vector<double> samples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            // Populate
            NoteStateMapArray nsma{};
            for (uint p = 0; p < 128; ++p)
                for (int n = 0; n < 50; ++n)
                    nsma[p][PPQ((double)n * 0.5)] = NoteData(100, Gem::NOTE);

            auto t0 = Clock::now();
            for (uint p = 0; p < 128; ++p)
                nsma[p].clear();
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }
        auto stats = computeStats(samples);
        printRow("Clear 128 maps (50 entries each)", stats.mean, stats.p95, stats.max);
    }

    // 7b: Insertion rate
    for (size_t noteCount : {(size_t)1000, (size_t)5000, (size_t)10000})
    {
        std::vector<double> samples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            NoteStateMapArray nsma{};
            auto t0 = Clock::now();
            for (size_t n = 0; n < noteCount; ++n)
            {
                uint pitch = (uint)(n % 128);
                nsma[pitch][PPQ((double)n * 0.1)] = NoteData(100, Gem::NOTE);
            }
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }
        auto stats = computeStats(samples);
        std::string label = "Insert " + std::to_string(noteCount) + " notes";
        printRow(label.c_str(), stats.mean, stats.p95, stats.max);
    }

    // 7c: Lookup rate (lower_bound/upper_bound)
    {
        NoteStateMapArray nsma{};
        for (size_t n = 0; n < 5000; ++n)
        {
            uint pitch = (uint)(n % 128);
            nsma[pitch][PPQ((double)n * 0.1)] = NoteData(100, Gem::NOTE);
        }

        std::vector<double> samples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();
            for (int q = 0; q < 1000; ++q)
            {
                uint pitch = (uint)(q % 128);
                PPQ pos((double)q * 0.5);
                auto lb = nsma[pitch].lower_bound(pos);
                auto ub = nsma[pitch].upper_bound(pos);
                (void)lb; (void)ub;
            }
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }
        auto stats = computeStats(samples);
        printRow("1000 lower/upper_bound lookups", stats.mean, stats.p95, stats.max);
    }

    // 7d: Copy cost (shared_ptr vs deep copy)
    {
        auto nsmaPtr = std::make_shared<NoteStateMapArray>();
        for (size_t n = 0; n < 5000; ++n)
        {
            uint pitch = (uint)(n % 128);
            (*nsmaPtr)[pitch][PPQ((double)n * 0.1)] = NoteData(100, Gem::NOTE);
        }

        // shared_ptr copy
        {
            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                auto t0 = Clock::now();
                auto copy = nsmaPtr;
                auto t1 = Clock::now();
                (void)copy;
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            printRow("shared_ptr copy (5000 notes)", stats.mean, stats.p95, stats.max);
        }

        // Deep copy
        {
            std::vector<double> samples;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                auto t0 = Clock::now();
                auto deep = std::make_shared<NoteStateMapArray>(*nsmaPtr);
                auto t1 = Clock::now();
                (void)deep;
                samples.push_back(usElapsed(t0, t1));
            }
            auto stats = computeStats(samples);
            printRow("Deep copy NoteStateMapArray (5000)", stats.mean, stats.p95, stats.max);
        }
    }

    //==========================================================================
    // Phase 8: ValueTree operations
    //==========================================================================
    printSectionHeader("Phase 8: ValueTree operations");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    // 8a: createCopy cost
    {
        juce::ValueTree src("PluginState");
        src.setProperty("part", (int)Part::GUITAR, nullptr);
        src.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        src.setProperty("drumType", (int)DrumType::PRO, nullptr);
        src.setProperty("autoHopo", true, nullptr);
        src.setProperty("hopoThreshold", 2, nullptr);
        src.setProperty("starPower", 0, nullptr);
        src.setProperty("dynamics", 1, nullptr);
        src.setProperty("kick2x", 1, nullptr);
        src.setProperty("discoFlip", 0, nullptr);
        src.setProperty("noteSpeed", 1.0, nullptr);
        src.setProperty("highwayLength", 0.85, nullptr);
        src.setProperty("showGems", true, nullptr);
        src.setProperty("showBars", true, nullptr);
        src.setProperty("showSustains", true, nullptr);
        src.setProperty("showLanes", true, nullptr);
        src.setProperty("showGridlines", true, nullptr);
        src.setProperty("showTrack", true, nullptr);
        src.setProperty("showLaneSeparators", true, nullptr);
        src.setProperty("showStrikeline", true, nullptr);

        std::vector<double> samples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();
            for (int j = 0; j < 100; ++j)
            {
                auto copy = src.createCopy();
                (void)copy;
            }
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }
        auto stats = computeStats(samples);
        printRow("createCopy x100 (20 props)", stats.mean, stats.p95, stats.max);
    }

    // 8b: setProperty cost
    {
        juce::ValueTree vt("PluginState");
        vt.setProperty("part", (int)Part::GUITAR, nullptr);

        std::vector<double> samples;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();
            for (int j = 0; j < 1000; ++j)
            {
                vt.setProperty("part", (int)(j % 4), nullptr);
                vt.setProperty("skillLevel", (int)(j % 4), nullptr);
            }
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }
        auto stats = computeStats(samples);
        printRow("setProperty x2000 (2 props)", stats.mean, stats.p95, stats.max);
    }

    //==========================================================================
    // Phase 9: Stress test — synthetic worst case
    //==========================================================================
    printSectionHeader("Phase 9: Stress test (synthetic notes)");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::setw(8) << "notes"
              << std::setw(10) << "us/note"
              << std::endl;
    std::cout << std::string(88, '-') << std::endl;

    for (size_t syntheticCount : {(size_t)50000, (size_t)100000})
    {
        // Generate synthetic Expert guitar notes (pitches 96-100, spread across song)
        std::vector<MidiCache::CachedNote> syntheticNotes;
        syntheticNotes.reserve(syntheticCount);

        for (size_t n = 0; n < syntheticCount; ++n)
        {
            MidiCache::CachedNote cn;
            cn.startPPQ = PPQ((double)n * 0.05);
            cn.endPPQ = PPQ((double)n * 0.05 + 0.04);
            cn.pitch = 96 + (uint)(n % 5); // Expert guitar pitches
            cn.velocity = 100;
            cn.channel = 0;
            cn.muted = false;
            cn.processed = false;
            syntheticNotes.push_back(cn);
        }

        juce::ValueTree synState("PluginState");
        synState.setProperty("part", (int)Part::GUITAR, nullptr);
        synState.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        synState.setProperty("drumType", (int)DrumType::PRO, nullptr);
        synState.setProperty("autoHopo", true, nullptr);
        synState.setProperty("hopoThreshold", 2, nullptr);
        synState.setProperty("starPower", 0, nullptr);
        synState.setProperty("dynamics", 1, nullptr);
        synState.setProperty("kick2x", 1, nullptr);
        synState.setProperty("discoFlip", 0, nullptr);

        NoteProcessor synNoteProcessor;
        std::vector<double> samples;

        for (int i = 0; i < ITERATIONS; ++i)
        {
            NoteStateMapArray nsma{};
            juce::CriticalSection lock;

            auto t0 = Clock::now();
            synNoteProcessor.processModifierNotes(syntheticNotes, nsma, lock, synState);
            synNoteProcessor.processPlayableNotes(syntheticNotes, nsma, lock, midiProcessor, synState, 120.0, 44100.0);
            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }

        auto stats = computeStats(samples);
        double usPerNote = stats.mean / syntheticCount;
        std::string label = "Synthetic " + std::to_string(syntheticCount) + " notes (guitar)";
        printRowWithCount(label.c_str(), stats.mean, stats.p95, stats.max, syntheticCount, usPerNote);
    }

    //==========================================================================
    // Phase 10: Full setting-change simulation (multi-track session)
    //==========================================================================
    printSectionHeader("Phase 10: Full session rebuild simulation");

    std::cout << std::left << std::setw(40) << ""
              << std::right << std::setw(10) << "mean"
              << std::setw(10) << "p95"
              << std::setw(10) << "max"
              << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    // Simulate what updateSessionData does: for each renderable track, for each difficulty, run NoteProcessor
    auto runSessionRebuild = [&](MidiFileParser::LoadedChart& chartRef, const std::string& fileLabel)
    {
        std::vector<double> samples;

        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto t0 = Clock::now();

            for (Part part : chartRef.foundParts)
            {
                if (!isGuitarLike(part) && !isDrumLike(part))
                    continue;

                juce::String tn = trackNameForPart(part);
                auto trackIt = chartRef.trackNotes.find(tn);
                if (trackIt == chartRef.trackNotes.end())
                    continue;

                const auto& notes = trackIt->second;

                for (SkillLevel skill : skills)
                {
                    juce::ValueTree sessionState("PluginState");
                    sessionState.setProperty("part", (int)part, nullptr);
                    sessionState.setProperty("skillLevel", (int)skill, nullptr);
                    sessionState.setProperty("drumType", (int)DrumType::PRO, nullptr);
                    sessionState.setProperty("autoHopo", true, nullptr);
                    sessionState.setProperty("hopoThreshold", 2, nullptr);
                    sessionState.setProperty("starPower", 0, nullptr);
                    sessionState.setProperty("dynamics", 1, nullptr);
                    sessionState.setProperty("kick2x", 1, nullptr);
                    sessionState.setProperty("discoFlip", 0, nullptr);

                    NoteProcessor sessionNP;
                    NoteStateMapArray nsma{};
                    juce::CriticalSection lock;
                    sessionNP.processModifierNotes(notes, nsma, lock, sessionState);
                    sessionNP.processPlayableNotes(notes, nsma, lock, midiProcessor, sessionState,
                                                   chartRef.initialBPM, 44100.0);
                }
            }

            auto t1 = Clock::now();
            samples.push_back(usElapsed(t0, t1));
        }

        auto stats = computeStats(samples);
        printRow(fileLabel.c_str(), stats.mean, stats.p95, stats.max);
    };

    // Run on embedded.mid
    if (embeddedFile != nullptr)
        runSessionRebuild(embeddedFile->chart, "embedded.mid full session");

    // Find largest .mid file by total note count
    LoadedFile* largestFile = nullptr;
    size_t largestNoteCount = 0;
    for (auto& lf : allFiles)
    {
        size_t total = 0;
        for (const auto& [name, notes] : lf.chart.trackNotes)
            total += notes.size();
        if (total > largestNoteCount)
        {
            largestNoteCount = total;
            largestFile = &lf;
        }
    }

    if (largestFile != nullptr && largestFile != embeddedFile)
    {
        std::string label = largestFile->filename.toStdString() + " full session (" + std::to_string(largestNoteCount) + " total notes)";
        runSessionRebuild(largestFile->chart, label);
    }

    std::cout << std::endl;
    std::cout << "(all values in microseconds, averaged over " << ITERATIONS << " iterations)" << std::endl;

    return 0;
}
