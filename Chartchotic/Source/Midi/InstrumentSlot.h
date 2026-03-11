#pragma once

#include <JuceHeader.h>
#include "Utils/MidiTypes.h"
#include "../UI/ControlConstants.h"

struct InstrumentSlot {
    Part part;
    NoteStateMapArray noteStateMapArray{};
    juce::CriticalSection noteStateMapLock;
};
