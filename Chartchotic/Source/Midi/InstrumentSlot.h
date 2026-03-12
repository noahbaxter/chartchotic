#pragma once

#include <JuceHeader.h>
#include "Utils/MidiTypes.h"
#include "ChartTextEvent.h"
#include "../UI/ControlConstants.h"

struct InstrumentSlot {
    Part part;
    NoteStateMapArray noteStateMapArray{};
    juce::CriticalSection noteStateMapLock;
    TrackTextEvents textEvents;
};
