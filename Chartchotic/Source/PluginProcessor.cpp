/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ReaperVST3.h"
#include "REAPER/ReaperVST2Extensions.h"
#include "REAPER/ReaperVST3Extensions.h"
#include "REAPER/ReaperIntegration.h"
#include "Midi/Pipelines/MidiPipelineFactory.h"
#include "Midi/Pipelines/MidiPipeline.h"
#include "Midi/Pipelines/ReaperMidiPipeline.h"

//==============================================================================
ChartchoticAudioProcessor::ChartchoticAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       midiProcessor(state),
       debugLogger([this](const juce::String& msg) { print(msg); })
#endif
{
    debugText = "Plugin loaded at " + juce::Time::getCurrentTime().toString(true, true) + "\n";
    initializeDefaultState();

    // Create the default pipeline (will be recreated when REAPER is detected)
    midiPipeline = MidiPipelineFactory::createPipeline(false, false, midiProcessor, nullptr, state,
                                                      [this](const juce::String& msg) { print(msg); });
}

ChartchoticAudioProcessor::~ChartchoticAudioProcessor()
{
}

void ChartchoticAudioProcessor::initializeDefaultState()
{
    state = juce::ValueTree("state");
    
    state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
#ifdef DEBUG
    state.setProperty("part", (int)Part::GUITAR, nullptr);
#else
    state.setProperty("part", (int)Part::DRUMS, nullptr);
#endif
    state.setProperty("drumType", (int)DrumType::PRO, nullptr);
    state.setProperty("framerate", 3, nullptr); // 60 FPS
    state.setProperty("latency", 2, nullptr);   // 500 ms
    state.setProperty("latencyOffsetMs", 0, nullptr); // Manual latency adjustment (-2000 to +2000ms)
    state.setProperty("autoHopo", (int)HopoMode::OFF, nullptr);
    state.setProperty("hitIndicators", 1, nullptr);
    state.setProperty("starPower", 1, nullptr);
    state.setProperty("kick2x", 1, nullptr);
    state.setProperty("dynamics", 1, nullptr);
    state.setProperty("noteSpeed", 7, nullptr);
    state.setProperty("gemScale", 1.0f, nullptr);
    state.setProperty("showGems", 1, nullptr);
    state.setProperty("showBars", 1, nullptr);
    state.setProperty("showSustains", 1, nullptr);
    state.setProperty("showLanes", 1, nullptr);
    state.setProperty("showGridlines", 1, nullptr);
    state.setProperty("reaperTrack", 1, nullptr); // Track 1 (0-indexed) = Track 1 in UI
}

void ChartchoticAudioProcessor::setLatencyInSeconds(float latencyInSeconds)
{
    this->latencyInSeconds = latencyInSeconds;
    double sampleRate = getSampleRate();
    if (sampleRate > 0.0)
    {
        this->latencyInSamples = (uint)(latencyInSeconds * sampleRate);

        // In REAPER mode, don't report any latency to the host
        // (we read timeline data directly, no buffer delay)
        if (isReaperHost && reaperMidiProvider.isReaperApiAvailable())
        {
            setLatencySamples(0);
        }
        else
        {
            setLatencySamples(this->latencyInSamples);
        }
    }
}

void ChartchoticAudioProcessor::invalidateReaperCache()
{
    if (midiPipeline)
    {
        // Cast to ReaperMidiPipeline and trigger data refetch
        auto* reaperPipeline = dynamic_cast<ReaperMidiPipeline*>(midiPipeline.get());
        if (reaperPipeline)
        {
            reaperPipeline->refetchAllMidiData();
        }
    }
}

void ChartchoticAudioProcessor::applyTrackNumberChange(int trackNumberZeroBased)
{
    // Convert 0-based to 1-based for storage in state
    int trackNumber1Based = trackNumberZeroBased + 1;

    // Clamp to valid range
    if (trackNumber1Based < 1) trackNumber1Based = 1;
    if (trackNumber1Based > 999) trackNumber1Based = 999;

    // Update state if value actually changed
    int currentTrack = (int)state.getProperty("reaperTrack");
    if (currentTrack != trackNumber1Based)
    {
        state.setProperty("reaperTrack", trackNumber1Based, nullptr);
        invalidateReaperCache();
    }
}

// MANUAL OVERRIDES
void ChartchoticAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencyInSeconds(latencyInSeconds);
}

void ChartchoticAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void ChartchoticAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Can't process if there is no playhead
    auto* playHead = getPlayHead();
    if (playHead == nullptr)
        return;

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue())
        return;

    // Get playhead position (for compatibility with editor)
    playheadPositionInSamples = static_cast<uint>(positionInfo->getTimeInSamples().orFallback(0));
    playheadPositionInPPQ = positionInfo->getPpqPosition().orFallback(0.0);
    isPlaying = positionInfo->getIsPlaying();

    // Recreate pipeline if REAPER was just detected
    // NOTE: Must be per-instance, not static! Multiple instances need their own state tracking
    if (isReaperHost != lastReaperConnected)
    {
        lastReaperConnected = isReaperHost;
        bool useReaperTimeline = isReaperHost && reaperMidiProvider.isReaperApiAvailable();

        print("====================================");
        print("=== PIPELINE MODE SWITCH ===");
        print("isReaperHost: " + juce::String(isReaperHost ? "TRUE" : "FALSE"));
        print("reaperApiAvailable: " + juce::String(reaperMidiProvider.isReaperApiAvailable() ? "TRUE" : "FALSE"));
        print("useReaperTimeline: " + juce::String(useReaperTimeline ? "TRUE" : "FALSE"));

        midiPipeline = MidiPipelineFactory::createPipeline(isReaperHost, useReaperTimeline,
                                                          midiProcessor, &reaperMidiProvider, state,
                                                          [this](const juce::String& msg) { print(msg); });

        if (useReaperTimeline)
        {
            print(">>> USING REAPER TIMELINE PIPELINE <<<");
            print(">>> NO LATENCY, DIRECT TIMELINE ACCESS <<<");
        }
        else
        {
            print(">>> USING STANDARD MIDI BUFFER PIPELINE <<<");
        }
        print("====================================");
    }

    // Process using the pipeline
    if (midiPipeline)
    {
        // Set the display window from the processor's stored value
        // (updated by editor when zoom changes)
        PPQ currentPos = positionInfo->getPpqPosition().orFallback(0.0);
        PPQ windowEnd = currentPos + displayWindowSize;

        // Logging disabled for performance
        // #ifdef DEBUG
        // static PPQ lastLoggedPos = PPQ(-100.0);
        // if (std::abs((currentPos - lastLoggedPos).toDouble()) > 0.1) // Log position changes
        // {
        //     print("=== DISPLAY WINDOW SET ===");
        //     print("currentPos: " + juce::String(currentPos.toDouble(), 3));
        //     print("displayWindowSize: " + juce::String(displayWindowSize.toDouble(), 3));
        //     print("windowEnd: " + juce::String(windowEnd.toDouble(), 3));
        //     lastLoggedPos = currentPos;
        // }
        // #endif

        midiPipeline->setDisplayWindow(currentPos, windowEnd);

        midiPipeline->process(*positionInfo, buffer.getNumSamples(), getSampleRate());

        // If the pipeline needs realtime MIDI, process it
        if (midiPipeline->needsRealtimeMidiBuffer())
        {
            midiPipeline->processMidiBuffer(midiMessages, *positionInfo,
                                           buffer.getNumSamples(),
                                           latencyInSamples,
                                           getSampleRate());
        }
    }
}
//==============================================
// Stock JUCE
juce::AudioProcessorEditor *ChartchoticAudioProcessor::createEditor()
{
    return new ChartchoticAudioProcessorEditor(*this, state);
}

bool ChartchoticAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

const juce::String ChartchoticAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChartchoticAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChartchoticAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChartchoticAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChartchoticAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChartchoticAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChartchoticAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChartchoticAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ChartchoticAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChartchoticAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void ChartchoticAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
#ifdef DEBUG
    // Don't persist state in debug standalone — always start fresh for testing
    if (wrapperType == wrapperType_Standalone) return;
#endif

    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
    }
}

void ChartchoticAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
#ifdef DEBUG
    // Don't restore state in debug standalone — always use defaults for testing
    if (wrapperType == wrapperType_Standalone) return;
#endif

    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        state = juce::ValueTree::fromXml(*xml);
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChartchoticAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif


juce::VST2ClientExtensions* ChartchoticAudioProcessor::getVST2ClientExtensions()
{
    // Create VST2 extensions instance on demand
    if (!vst2Extensions)
        vst2Extensions = std::make_unique<ChartchoticVST2Extensions>(this);

    return vst2Extensions.get();
}

bool ChartchoticAudioProcessor::attemptReaperConnection()
{
    if (!isReaperHost || !reaperGetFunc)
        return false;

    // Test the connection by getting a simple REAPER function
    auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
    if (GetPlayState)
    {
        DBG("Successfully connected to REAPER API via VST2!");
        return true;
    }

    return false;
}

void* ChartchoticAudioProcessor::getReaperApi(const char* funcname)
{
    if (reaperGetFunc)
        return reaperGetFunc(funcname);
    return nullptr;
}


std::string ChartchoticAudioProcessor::getHostInfo()
{
    // Try to get host information
    juce::String hostName = getPlayHead() ? "Unknown Host" : "No PlayHead";

    // JUCE doesn't provide a direct way to get host name in VST2, but we can infer it
    // from various clues or wait for the REAPER-specific handshake
    return hostName.toStdString();
}


juce::VST3ClientExtensions* ChartchoticAudioProcessor::getVST3ClientExtensions()
{
#if JucePlugin_Build_VST3
    // Create VST3 extensions instance on demand
    if (!vst3Extensions)
        vst3Extensions = std::make_unique<ChartchoticVST3Extensions>(this);

    return vst3Extensions.get();
#else
    return nullptr;
#endif
}

void ChartchoticAudioProcessor::processReaperTimelineMidi(PPQ startPPQ, PPQ endPPQ, double bpm, uint timeSignatureNumerator, uint timeSignatureDenominator)
{
    // If using the new pipeline system, set the display window
    if (midiPipeline)
    {
        midiPipeline->setDisplayWindow(startPPQ, endPPQ);
    }
    else
    {
        // Fallback to old method
        ReaperIntegration::processReaperTimelineMidi(*this, startPPQ, endPPQ, bpm, timeSignatureNumerator, timeSignatureDenominator);
    }
}

void ChartchoticAudioProcessor::requestTimelinePositionChange(PPQ newPosition)
{
    if (isReaperHost && reaperMidiProvider.isReaperApiAvailable())
    {
        // Use REAPER's SetEditCurPos to move the edit cursor
        if (auto* reaper_SetEditCurPos = (void(*)(double, bool, bool))getReaperApi("SetEditCurPos"))
        {
            double timeInSeconds = reaperMidiProvider.ppqToTime(newPosition.toDouble());
            reaper_SetEditCurPos(timeInSeconds, true, false);
        }
    }
    // For non-REAPER hosts, we can't directly control playback position
    // This is a limitation of the plugin API - only the host can set playback position
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChartchoticAudioProcessor();
}

