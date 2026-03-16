#pragma once

#include <JuceHeader.h>
#include "HighwayComponent.h"
#include "../Midi/Processing/MidiInterpreter.h"

struct HighwaySlot {
    std::unique_ptr<HighwayComponent> highway;
    std::unique_ptr<MidiInterpreter> interpreter;
    Part part = Part::GUITAR;

    // Per-slot note data (used in multi-highway mode)
    // Heap-allocated because CriticalSection is non-movable
    std::unique_ptr<NoteStateMapArray> noteStateMapArray = std::make_unique<NoteStateMapArray>();
    std::unique_ptr<juce::CriticalSection> noteStateMapLock = std::make_unique<juce::CriticalSection>();

    // Non-copyable, movable
    HighwaySlot() = default;
    HighwaySlot(HighwaySlot&&) = default;
    HighwaySlot& operator=(HighwaySlot&&) = default;
};
