#pragma once

// Centralized tooltip text for all InfoTooltip instances.
// Keep descriptions user-facing: what it does, not how it works.

namespace TooltipStrings
{

inline constexpr const char* trackDiscovery =
    "Controls how the plugin finds MIDI to display.\n\n"
    "On: scans the project for tracks named PART GUITAR, "
    "PART DRUMS, etc. and populates the instrument selector.\n\n"
    "Off: reads MIDI from whatever track the plugin is placed on. "
    "You pick guitar or drums.";

inline constexpr const char* calibration =
    "Fine-tune the visual sync to match your audio interface latency. "
    "Most setups won't need to change this.";

inline constexpr const char* latency =
    "How far ahead the plugin requests MIDI from the DAW. "
    "Higher values give more visible scroll time at the cost of "
    "added latency. Your note speed and highway length affect "
    "how much lookahead you need.\n\n"
    "Try 500ms to start.";

inline constexpr const char* discoFlip =
    "Swap red and yellow cymbal positions during disco flip sections. "
    "Matches how Rock Band charts encode hand-swap patterns for pro drums.";

inline constexpr const char* hopoThreshold =
    "Max note distance for auto hammer-on/pull-off detection. "
    "Default (170 ticks) matches Guitar Hero, Rock Band, Clone Hero, "
    "and YARG. You almost never need to change this.\n\n"
    "Tight: 120 ticks (1/16th note). Fewer auto HOPOs.\n"
    "Default: 170 ticks. The standard across all major games.\n"
    "Loose: 240 ticks (1/8th note). For charts using eighthnote_hopo.";

} // namespace TooltipStrings
