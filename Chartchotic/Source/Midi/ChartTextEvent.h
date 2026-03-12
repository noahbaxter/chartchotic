#pragma once

#include <vector>
#include <JuceHeader.h>
#include "../Utils/PPQ.h"

struct ChartTextEvent {
    PPQ position;
    juce::String text;
};

using TrackTextEvents = std::vector<ChartTextEvent>;
