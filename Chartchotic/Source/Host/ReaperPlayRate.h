#pragma once

#include <JuceHeader.h>
#include "PlayRateMath.h"
#include "../Midi/Providers/REAPER/ReaperApiHelpers.h"

// REAPER's master playrate: the transport's own speed, which slows or speeds playback without
// touching the tempo map. The control drives REAPER's value rather than keeping one of its own,
// so REAPER's slider and Chartchotic's always agree.
//
// Preserving pitch is REAPER's per-project option on that same slider. It has no API call,
// only a native toggle action, so the action is found once by its name in the main section's
// action list and then read and flipped by its command ID.
class ReaperPlayRate
{
public:
    explicit ReaperPlayRate(const ReaperAPIs& apis) : apis(apis) {}

    bool available() const { return apis.Master_GetPlayRate && apis.CSurf_OnPlayRateChange; }

    double get() const { return available() ? apis.Master_GetPlayRate(nullptr) : 1.0; }

    void set(double rate) const
    {
        if (available()) apis.CSurf_OnPlayRateChange(PlayRateMath::snap(rate));
    }

    bool hasPitchToggle() { return pitchCommand() > 0 && apis.GetToggleCommandState && apis.Main_OnCommand; }

    bool preservesPitch()
    {
        return hasPitchToggle() && apis.GetToggleCommandState(pitchCommand()) == 1;
    }

    void togglePreservePitch()
    {
        if (hasPitchToggle()) apis.Main_OnCommand(pitchCommand(), 0);
    }

private:
    // 0 = the main section, where transport actions live.
    int pitchCommand()
    {
        if (pitchLookupDone) return pitchCommandId;
        pitchLookupDone = true;

        if (!apis.SectionFromUniqueID || !apis.kbd_enumerateActions) return pitchCommandId;
        void* mainSection = apis.SectionFromUniqueID(0);
        if (mainSection == nullptr) return pitchCommandId;

        // "Transport: Toggle preserve pitch in audio items when changing master playrate".
        // Matched on its two distinctive phrases rather than the whole string, so a reworded
        // prefix in a later REAPER still finds it.
        for (int i = 0;; ++i)
        {
            const char* name = nullptr;
            const int id = apis.kbd_enumerateActions(mainSection, i, &name);
            if (id == 0) break;
            const juce::String n(name != nullptr ? name : "");
            if (n.containsIgnoreCase("preserve pitch") && n.containsIgnoreCase("master playrate"))
            {
                pitchCommandId = id;
                break;
            }
        }
        return pitchCommandId;
    }

    const ReaperAPIs& apis;
    bool pitchLookupDone = false;
    int pitchCommandId = -1;
};
