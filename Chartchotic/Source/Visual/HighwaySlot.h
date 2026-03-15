#pragma once

#include <JuceHeader.h>
#include "HighwayComponent.h"
#include "../Midi/Processing/MidiInterpreter.h"

struct HighwaySlot {
    std::unique_ptr<HighwayComponent> highway;
    std::unique_ptr<MidiInterpreter> interpreter;
    Part part = Part::GUITAR;

    // Non-copyable, movable
    HighwaySlot() = default;
    HighwaySlot(HighwaySlot&&) = default;
    HighwaySlot& operator=(HighwaySlot&&) = default;
};
