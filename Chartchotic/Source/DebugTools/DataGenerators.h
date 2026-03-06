/*
    ==============================================================================

        DataGenerators.h
        Author:  Noah Baxter

        Fake data generators for testing chart rendering.

    ==============================================================================
*/

#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../Utils/Utils.h"
#include "../Utils/PPQ.h"

namespace DataGenerators
{

inline TrackWindow generateFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
{
    TrackWindow fakeTrackWindow;

    fakeTrackWindow[trackWindowStartPPQ + PPQ(0.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(0.25)] = {Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(0.50)] = {Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(0.75)] = {Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(1.00)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(1.25)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(1.50)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE};
    fakeTrackWindow[trackWindowStartPPQ + PPQ(1.75)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE};

    return fakeTrackWindow;
}

inline TrackWindow generateFullFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
{
    TrackWindow fakeTrackWindow;

    for (int i = 0; i <= 16; i++)
    {
        fakeTrackWindow[trackWindowStartPPQ + PPQ(i * 0.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
    }

    return fakeTrackWindow;
}

inline SustainWindow generateFakeSustainWindow(juce::ValueTree& state, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
{
    SustainWindow fakeSustainWindow;

    uint laneCount = isPart(state, Part::GUITAR) ? 6u : 5u;
    for (uint i = 0; i < laneCount; i++)
    {
        SustainEvent sustain;
        sustain.startPPQ = trackWindowStartPPQ + PPQ(0.0);
        sustain.endPPQ = trackWindowStartPPQ + PPQ(2.0);
        sustain.gemColumn = i;
        sustain.gemType = Gem::NOTE;
        fakeSustainWindow.push_back(sustain);
    }

    return fakeSustainWindow;
}

} // namespace DataGenerators

#endif // DEBUG
