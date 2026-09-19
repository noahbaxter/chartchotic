/*
  ==============================================================================

    ReaperTrackDetector.h
    Automatic detection of which track contains the Chartchotic plugin

  ==============================================================================
*/

#pragma once

#include <functional>
#include <string>

class ReaperTrackDetector
{
public:
    using ReaperGetFunc = std::function<void*(const char*)>;

    // Detect which track contains the Chartchotic plugin
    // Returns -1 if not found or on error
    static int detectPluginTrack(ReaperGetFunc reaperGetFunc);

    // Get the name of a track by index
    static std::string getTrackName(ReaperGetFunc reaperGetFunc, int trackIndex);

    // Get the total number of tracks in the project
    static int getTrackCount(ReaperGetFunc reaperGetFunc);

    // What a track actually holds. Parts need MIDI and the waveform needs audio, so both
    // answers come from one walk of the track's items rather than two.
    struct TrackContent { bool midi = false; bool audio = false; };
    static TrackContent getTrackContent(ReaperGetFunc reaperGetFunc, int trackIndex);

    // Append a named track to the project. Returns its index, or -1 if REAPER would not.
    // Wrapped in an undo block, so it is one step in REAPER's own history rather than a
    // change that appeared from nowhere.
    static int createTrackNamed(ReaperGetFunc reaperGetFunc, const std::string& name);

    // Remove a track and everything on it. Destructive and undoable only through REAPER, so
    // the caller confirms first.
    static bool deleteTrack(ReaperGetFunc reaperGetFunc, int trackIndex);
};