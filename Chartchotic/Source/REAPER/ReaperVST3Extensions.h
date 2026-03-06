/*
  ==============================================================================

    ReaperVST3Extensions.h
    VST3-specific REAPER integration extensions

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

class ChartchoticAudioProcessor;
class IReaperHostApplication;
class TrackInfoListener;

#if JucePlugin_Build_VST3

class ChartchoticVST3Extensions : public juce::VST3ClientExtensions
{
public:
    ChartchoticVST3Extensions(ChartchoticAudioProcessor* proc);
    ~ChartchoticVST3Extensions();

    void setIHostApplication(Steinberg::FUnknown* host) override;
    int32_t queryIEditController(const Steinberg::TUID tuid, void** obj) override;

private:
    ChartchoticAudioProcessor* processor;
    std::unique_ptr<TrackInfoListener> trackInfoListener;
};

#endif // JucePlugin_Build_VST3