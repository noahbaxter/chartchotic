/*
  ==============================================================================

    MidiPipelineFactory.cpp
    Factory for creating the appropriate MIDI processing pipeline

  ==============================================================================
*/

#include "MidiPipelineFactory.h"
#include "StandardMidiPipeline.h"
#include "ReaperMidiPipeline.h"
#include "../Providers/REAPER/ReaperMidiProvider.h"

std::unique_ptr<MidiPipeline> MidiPipelineFactory::createPipeline(
    bool isReaperHost,
    bool useReaperTimeline,
    MidiProcessor& midiProcessor,
    MidiProject& midiProject,
    ReaperMidiProvider* reaperProvider,
    juce::ValueTree& state,
    std::function<void(const juce::String&)> printFunc)
{
    if (isReaperHost && useReaperTimeline && reaperProvider && reaperProvider->isReaperApiAvailable())
    {
        return std::make_unique<ReaperMidiPipeline>(midiProject, *reaperProvider, state, printFunc);
    }

    return std::make_unique<StandardMidiPipeline>(midiProcessor, state);
}
