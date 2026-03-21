#pragma once

#include <JuceHeader.h>
#include "HighwayComponent.h"
#include "../Midi/Processing/MidiInterpreter.h"

struct HighwaySlot {
    std::unique_ptr<HighwayComponent> highway;
    std::unique_ptr<MidiInterpreter> interpreter;
    Part part = Part::GUITAR;
    SkillLevel skillLevel = SkillLevel::EXPERT;
    bool active = false;

    // Per-slot note data (used in multi-highway mode)
    // shared_ptr so session cache can share processed data with visible slots
    std::shared_ptr<NoteStateMapArray> noteStateMapArray = std::make_shared<NoteStateMapArray>();
    std::shared_ptr<juce::CriticalSection> noteStateMapLock = std::make_shared<juce::CriticalSection>();

    // Per-slot state override (all-difficulties mode needs per-slot skillLevel)
    std::unique_ptr<juce::ValueTree> ownedState;

    // Non-copyable, movable
    HighwaySlot() = default;
    HighwaySlot(HighwaySlot&&) = default;
    HighwaySlot& operator=(HighwaySlot&&) = default;
};
