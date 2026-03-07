#pragma once

#include <catch2/catch_test_macros.hpp>
#include "Midi/Utils/MidiTypes.h"
#include "Midi/Utils/MidiConstants.h"
#include "Midi/Utils/InstrumentMapper.h"
#include "Utils/Utils.h"
#include "Utils/PPQ.h"

struct TestFixture
{
    NoteStateMapArray array{};
    juce::CriticalSection lock;
    juce::ValueTree state{"PluginState"};

    TestFixture()
    {
        state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        state.setProperty("part", (int)Part::GUITAR, nullptr);
        state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
        state.setProperty("autoHopo", false, nullptr);
        state.setProperty("hopoThreshold", 2, nullptr);
        state.setProperty("starPower", 1, nullptr);
        state.setProperty("kick2x", 1, nullptr);
        state.setProperty("dynamics", 1, nullptr);
    }

    // Add a playable note with proper note-on/note-off
    void addNote(uint pitch, PPQ start, PPQ end, uint8_t vel = 100, Gem gem = Gem::NOTE)
    {
        array[pitch][start] = NoteData(vel, gem);
        array[pitch][std::max(start + PPQ(1), end - PPQ(1))] = NoteData(0, Gem::NONE);
    }

    // Add a modifier (sustained note-on/note-off pair)
    void addModifier(uint pitch, PPQ start, PPQ end)
    {
        array[pitch][start] = NoteData(100, Gem::NONE);
        array[pitch][std::max(start + PPQ(1), end - PPQ(1))] = NoteData(0, Gem::NONE);
    }
};
