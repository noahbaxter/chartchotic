/*
  ==============================================================================

    ReaperTrackDetector.cpp
    Automatic detection of which track contains the Chartchotic plugin

  ==============================================================================
*/

#include "ReaperTrackDetector.h"
#include <cstring>
#include <algorithm>

int ReaperTrackDetector::detectPluginTrack(ReaperGetFunc reaperGetFunc)
{
    if (!reaperGetFunc)
        return -1;

    // Get REAPER API functions we need
    auto CountTracks = (int(*)(int))reaperGetFunc("CountTracks");
    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto TrackFX_GetCount = (int(*)(void*))reaperGetFunc("TrackFX_GetCount");
    auto TrackFX_GetFXName = (bool(*)(void*, int, char*, int))reaperGetFunc("TrackFX_GetFXName");

    if (!CountTracks || !GetTrack || !TrackFX_GetCount || !TrackFX_GetFXName)
        return -1;

    int trackCount = CountTracks(0);  // 0 = current project

    // Search all tracks for the Chartchotic plugin
    for (int trackIdx = 0; trackIdx < trackCount; trackIdx++)
    {
        void* track = GetTrack(0, trackIdx);  // 0 = current project
        if (!track) continue;

        int fxCount = TrackFX_GetCount(track);

        // Search all FX on this track
        for (int fxIdx = 0; fxIdx < fxCount; fxIdx++)
        {
            char fxName[256] = {0};
            if (TrackFX_GetFXName(track, fxIdx, fxName, sizeof(fxName)))
            {
                // Check if this is our plugin (case-insensitive)
                if (strstr(fxName, "Chartchotic") != nullptr ||
                    strstr(fxName, "Chartchotic") != nullptr)
                {
                    return trackIdx;  // Found it!
                }
            }
        }
    }

    return -1;  // Not found
}

ReaperTrackDetector::TrackContent
ReaperTrackDetector::getTrackContent(ReaperGetFunc reaperGetFunc, int trackIndex)
{
    TrackContent content;
    if (!reaperGetFunc)
        return content;

    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto CountTrackMediaItems = (int(*)(void*))reaperGetFunc("CountTrackMediaItems");
    auto GetTrackMediaItem = (void*(*)(void*, int))reaperGetFunc("GetTrackMediaItem");
    auto GetActiveTake = (void*(*)(void*))reaperGetFunc("GetActiveTake");
    auto TakeIsMIDI = (bool(*)(void*))reaperGetFunc("TakeIsMIDI");

    if (!GetTrack || !CountTrackMediaItems || !GetTrackMediaItem || !GetActiveTake || !TakeIsMIDI)
        return content;

    void* track = GetTrack(0, trackIndex);  // 0 = current project
    if (!track)
        return content;

    int itemCount = CountTrackMediaItems(track);
    for (int i = 0; i < itemCount; ++i)
    {
        void* item = GetTrackMediaItem(track, i);
        if (!item)
            continue;

        void* take = GetActiveTake(item);
        if (!take)
            continue;

        if (TakeIsMIDI(take)) content.midi = true;
        else                  content.audio = true;

        // Both answers found, so the rest of a long track's items say nothing new.
        if (content.midi && content.audio)
            break;
    }

    return content;
}

int ReaperTrackDetector::createTrackNamed(ReaperGetFunc reaperGetFunc, const std::string& name)
{
    if (!reaperGetFunc)
        return -1;

    auto CountTracks = (int(*)(int))reaperGetFunc("CountTracks");
    auto InsertTrackAtIndex = (void(*)(int, bool))reaperGetFunc("InsertTrackAtIndex");
    auto TrackList_AdjustWindows = (void(*)(bool))reaperGetFunc("TrackList_AdjustWindows");
    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto GetSetMediaTrackInfo_String =
        (bool(*)(void*, const char*, char*, bool))reaperGetFunc("GetSetMediaTrackInfo_String");
    auto Undo_BeginBlock2 = (void(*)(void*))reaperGetFunc("Undo_BeginBlock2");
    auto Undo_EndBlock2 = (void(*)(void*, const char*, int))reaperGetFunc("Undo_EndBlock2");

    if (!CountTracks || !InsertTrackAtIndex || !GetTrack || !GetSetMediaTrackInfo_String)
        return -1;

    if (Undo_BeginBlock2) Undo_BeginBlock2(nullptr);   // nullptr = current project

    const int index = CountTracks(0);
    InsertTrackAtIndex(index, true);                   // true = with default envelopes/FX
    if (TrackList_AdjustWindows) TrackList_AdjustWindows(false);

    void* track = GetTrack(0, index);
    if (track)
    {
        std::string buffer = name;
        buffer.resize(std::max<size_t>(buffer.size() + 1, 256));
        GetSetMediaTrackInfo_String(track, "P_NAME", buffer.data(), true);
    }

    if (Undo_EndBlock2) Undo_EndBlock2(nullptr, ("Add track " + name).c_str(), -1);

    return track ? index : -1;
}

bool ReaperTrackDetector::deleteTrack(ReaperGetFunc reaperGetFunc, int trackIndex)
{
    if (!reaperGetFunc)
        return false;

    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto DeleteTrack = (void(*)(void*))reaperGetFunc("DeleteTrack");
    auto TrackList_AdjustWindows = (void(*)(bool))reaperGetFunc("TrackList_AdjustWindows");
    auto Undo_BeginBlock2 = (void(*)(void*))reaperGetFunc("Undo_BeginBlock2");
    auto Undo_EndBlock2 = (void(*)(void*, const char*, int))reaperGetFunc("Undo_EndBlock2");

    if (!GetTrack || !DeleteTrack)
        return false;

    void* track = GetTrack(0, trackIndex);
    if (!track)
        return false;

    if (Undo_BeginBlock2) Undo_BeginBlock2(nullptr);
    DeleteTrack(track);
    if (TrackList_AdjustWindows) TrackList_AdjustWindows(false);
    if (Undo_EndBlock2) Undo_EndBlock2(nullptr, "Delete track", -1);

    return true;
}

std::string ReaperTrackDetector::getTrackName(ReaperGetFunc reaperGetFunc, int trackIndex)
{
    if (!reaperGetFunc)
        return "";

    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto GetTrackName = (bool(*)(void*, char*, int))reaperGetFunc("GetTrackName");

    if (!GetTrack || !GetTrackName)
        return "";

    void* track = GetTrack(0, trackIndex);  // 0 = current project
    if (!track)
        return "";

    char trackName[256] = {0};
    if (GetTrackName(track, trackName, sizeof(trackName)))
    {
        return std::string(trackName);
    }

    return "Track " + std::to_string(trackIndex + 1);  // Default name
}

int ReaperTrackDetector::getTrackCount(ReaperGetFunc reaperGetFunc)
{
    if (!reaperGetFunc)
        return 0;

    auto CountTracks = (int(*)(int))reaperGetFunc("CountTracks");
    if (!CountTracks)
        return 0;

    return CountTracks(0);  // 0 = current project
}