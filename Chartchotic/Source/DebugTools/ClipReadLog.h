/*
    ==============================================================================

        ClipReadLog.h
        Author: Noah Baxter

        One log for everything read off REAPER's timeline: MIDI items and their
        notes, audio items and their samples. What was asked for, what came back.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

/*
    Reading the timeline is the part of this plugin with the least to show for itself when it
    goes wrong. An empty highway and a flat waveform look the same whether the track was never
    resolved, the items were never found, the read returned nothing, or the data arrived fine
    and the renderer asked for the wrong range. Four causes, one symptom, no way to tell them
    apart from the outside.

    So both readers report through here, in one file, in order: the track they resolved, the
    items they found, how much came back, and what the view then asked for. A mismatch between
    what was read and what was asked for is the failure that is otherwise invisible, and side
    by side it is obvious.

    Marker file, like Perf, so a normal session pays nothing and there is no switch to forget:

        enable:  touch ~/Library/Application Support/Chartchotic/clips
        read:    ~/Library/Logs/Chartchotic/clips.log

    Message thread only. Both readers already assert they are on it.
*/
namespace ClipReadLog
{
    enum class Source { Midi, Audio };

    inline bool enabled()
    {
        struct State { bool armed = false; bool on = false; };
        static State s;

        if (!s.armed)
        {
            s.armed = true;
            s.on = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Application Support/Chartchotic/clips")
                       .existsAsFile();
        }
        return s.on;
    }

    inline void write(Source source, const juce::String& message)
    {
        if (!enabled()) return;

        static std::unique_ptr<juce::FileLogger> logger = []
        {
            auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("Logs/Chartchotic/clips.log");
            f.getParentDirectory().createDirectory();
            return std::make_unique<juce::FileLogger>(f, "Chartchotic clip reads", 0);
        }();

        if (logger != nullptr)
            logger->logMessage(juce::String(source == Source::Midi ? "[MIDI]  " : "[AUDIO] ")
                               + message);
    }

    inline void midi(const juce::String& message)  { write(Source::Midi, message); }
    inline void audio(const juce::String& message) { write(Source::Audio, message); }

    // True at most once per intervalMs, for the lines that would otherwise arrive every frame.
    // The caller owns the timestamp so each site throttles on its own clock.
    inline bool due(double& lastMs, double intervalMs)
    {
        if (!enabled()) return false;

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastMs < intervalMs) return false;

        lastMs = now;
        return true;
    }

    // Only log a value when it actually moves. Most of these lines describe state that holds
    // for minutes at a time, and repeating them every frame buries the one that changed.
    template <typename T>
    inline bool changed(T& remembered, T current)
    {
        if (!enabled() || remembered == current) return false;

        remembered = current;
        return true;
    }

    inline juce::String secs(double t)  { return juce::String(t, 3) + "s"; }
    inline juce::String range(double from, double to)
    {
        return secs(from) + " .. " + secs(to);
    }

    // A track index as the rest of the app spells it, with "no track" said out loud rather
    // than printed as a negative number nobody can place.
    inline juce::String track(int sourceTrackIndex)
    {
        return sourceTrackIndex < 0 ? juce::String("(no track)")
                                    : ("track " + juce::String(sourceTrackIndex));
    }
}
