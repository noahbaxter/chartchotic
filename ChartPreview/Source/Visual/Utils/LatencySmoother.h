/*
    ==============================================================================

        LatencySmoother.h
        Author:  Noah Baxter

        Multi-buffer latency smoothing state machine.
        Smooths PPQ latency changes over a configurable duration to prevent
        jitter during tempo changes.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/PPQ.h"

class LatencySmoother
{
public:
    PPQ smooth(PPQ targetLatency)
    {
        juce::int64 currentTime = juce::Time::getHighResolutionTicks();

        double targetDifference = std::abs((targetLatency - smoothingTargetLatencyPPQ).toDouble());
        bool targetChanged = targetDifference > 0.01;
        bool isFirstFrame = lastSmoothedLatencyPPQ == 0.0;

        if (isFirstFrame || targetChanged)
        {
            if (isFirstFrame)
            {
                lastSmoothedLatencyPPQ = targetLatency;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingStartLatencyPPQ = targetLatency;
                smoothingProgress = 1.0;
                lastSmoothingUpdateTime = currentTime;
                return targetLatency;
            }
            else
            {
                smoothingStartLatencyPPQ = lastSmoothedLatencyPPQ;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingProgress = 0.0;
                lastSmoothingUpdateTime = currentTime;
            }
        }

        double timeElapsedSeconds = (currentTime - lastSmoothingUpdateTime) / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        lastSmoothingUpdateTime = currentTime;

        if (smoothingProgress < 1.0)
        {
            double progressIncrement = timeElapsedSeconds / durationSeconds;
            smoothingProgress = std::min(1.0, smoothingProgress + progressIncrement);

            double totalAdjustment = (smoothingTargetLatencyPPQ - smoothingStartLatencyPPQ).toDouble();
            PPQ currentAdjustment = PPQ(totalAdjustment * smoothingProgress);
            lastSmoothedLatencyPPQ = smoothingStartLatencyPPQ + currentAdjustment;
        }
        else
        {
            lastSmoothedLatencyPPQ = smoothingTargetLatencyPPQ;
        }

        return lastSmoothedLatencyPPQ;
    }

    void reset()
    {
        lastSmoothedLatencyPPQ = 0.0;
        smoothingTargetLatencyPPQ = 0.0;
        smoothingStartLatencyPPQ = 0.0;
        smoothingProgress = 1.0;
        lastSmoothingUpdateTime = 0;
    }

    void setDuration(double seconds) { durationSeconds = seconds; }

private:
    PPQ lastSmoothedLatencyPPQ = 0.0;
    PPQ smoothingTargetLatencyPPQ = 0.0;
    PPQ smoothingStartLatencyPPQ = 0.0;
    double smoothingProgress = 1.0;
    double durationSeconds = 2.0;
    juce::int64 lastSmoothingUpdateTime = 0;
};
