/*
  ==============================================================================

    MidiPipelineFactory.h
    Factory for creating the appropriate MIDI processing pipeline

  ==============================================================================
*/

#pragma once

#include <memory>
#include "MidiPipeline.h"

class MidiProcessor;
class MidiProject;
class ReaperMidiProvider;

class MidiPipelineFactory
{
public:
    static std::unique_ptr<MidiPipeline> createPipeline(
        bool isReaperHost,
        bool useReaperTimeline,
        MidiProcessor& midiProcessor,
        MidiProject& midiProject,
        ReaperMidiProvider* reaperProvider,
        juce::ValueTree& state,
        std::function<void(const juce::String&)> printFunc = nullptr
    );
};
