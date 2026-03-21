/*
  ==============================================================================

    TrackInfoListener.cpp
    VST3 Standard Track Information Listener

  ==============================================================================
*/

#include "TrackInfoListener.h"
#include "../PluginProcessor.h"

#if JucePlugin_Build_VST3
using namespace Steinberg;

TrackInfoListener::TrackInfoListener(ChartchoticAudioProcessor* proc)
    : processor(proc)
{
}

tresult TrackInfoListener::setChannelContextInfos(Vst::IAttributeList* list)
{
    if (!list || !processor)
        return kInvalidArgument;

    // Try to get channel index (1-based)
    Steinberg::int64 channelIndex = -1;
    if (list->getInt(Vst::ChannelContext::kChannelIndexKey, channelIndex) == kResultTrue)
    {
        int trackNum = static_cast<int>(channelIndex);

        // Only update if track number actually changed
        if (processor->detectedTrackNumber.load() != trackNum)
        {
            processor->detectedTrackNumber = trackNum;

            // Auto-apply track change (convert from 1-based to 0-based for MIDI channel)
            processor->applyTrackNumberChange(trackNum - 1);
        }
    }

    return kResultTrue;
}

tresult TrackInfoListener::queryInterface(const TUID _iid, void** obj)
{
    // Compare against IInfoListener IID bytes directly to avoid needing DEF_CLASS_IID
    // (which conflicts with vstinitiids.cpp on the VST3 target).
    // From ivstchannelcontextinfo.h: 0x0F194781, 0x8D984ADA, 0xBBA0C1EF, 0xC011D8D0
    static const TUID infoListenerIID = INLINE_UID(0x0F194781, 0x8D984ADA, 0xBBA0C1EF, 0xC011D8D0);
    if (std::memcmp(_iid, infoListenerIID, sizeof(TUID)) == 0)
    {
        ++refCount;
        *obj = this;
        return kResultOk;
    }

    *obj = nullptr;
    return kNoInterface;
}

uint32 TrackInfoListener::addRef()
{
    return ++refCount;
}

uint32 TrackInfoListener::release()
{
    uint32 newCount = --refCount;
    if (newCount == 0)
        delete this;
    return newCount;
}

#endif // JucePlugin_Build_VST3
