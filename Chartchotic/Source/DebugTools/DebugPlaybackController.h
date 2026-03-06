#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../Utils/PPQ.h"

class DebugPlaybackController
{
public:
    bool isPlaying() const { return playActive; }
    bool isNotesActive() const { return chartIndex > 0; }
    PPQ getCurrentPPQ() const { return PPQ(playPPQ); }
    double getBPM() const { return bpm; }

    int getChartIndex() const { return chartIndex; }
    void setChartIndex(int index) { chartIndex = index; }

    void setPlaying(bool active)
    {
        playActive = active;
        if (active)
            lastTick = juce::Time::getHighResolutionTicks();
    }

    void togglePlay()
    {
        setPlaying(!playActive);
    }

    void advancePlayhead()
    {
        if (!playActive) return;

        juce::int64 now = juce::Time::getHighResolutionTicks();
        double deltaSeconds = (now - lastTick) / (double)juce::Time::getHighResolutionTicksPerSecond();
        lastTick = now;

        playPPQ += deltaSeconds * (bpm / 60.0);
    }

    void setBPM(double newBpm) { bpm = newBpm; }

    void nudgePlayhead(double beats)
    {
        playPPQ = std::max(0.0, playPPQ + beats);
    }

private:
    bool playActive = false;
    int chartIndex = 1;
    double playPPQ = 0.0;
    juce::int64 lastTick = 0;
    double bpm = 150.0;
};

#endif
