#pragma once

#include "ChartTextEvent.h"

/**
 * Tracks disco flip regions parsed from MIDI text events on the drums track.
 *
 * Disco flip swaps red <-> yellow lanes during marked sections.
 * Text event format: [mix <diff> drums<N>[d|dnoflip]]
 *   - diff 0-3 = Easy through Expert
 *   - 'd' suffix = flip ON
 *   - no suffix or 'dnoflip' = flip OFF
 *
 * Regions are stored per-difficulty since charts can have different
 * flip sections per difficulty level.
 */
class DiscoFlipState
{
public:
    static constexpr int NUM_DIFFICULTIES = 4; // 0=Easy, 1=Medium, 2=Hard, 3=Expert

    struct FlipRegion {
        PPQ start;
        PPQ end;
    };

    DiscoFlipState() = default;

    void buildFromTextEvents(const TrackTextEvents& events)
    {
        for (auto& r : perDiffRegions)
            r.clear();

        PPQ flipStart[NUM_DIFFICULTIES] = {};
        bool inFlip[NUM_DIFFICULTIES] = {};

        for (const auto& evt : events)
        {
            juce::String t = evt.text.trim();
            if (!t.startsWith("[mix ") || !t.endsWith("]"))
                continue;

            // Strip brackets: "mix 3 drums0d"
            juce::String inner = t.substring(1, t.length() - 1);

            // Tokenize: "mix", "3", "drums0d"
            juce::StringArray tokens;
            tokens.addTokens(inner, " ", "");
            if (tokens.size() < 3 || tokens[0] != "mix")
                continue;

            int diff = tokens[1].getIntValue();
            if (diff < 0 || diff >= NUM_DIFFICULTIES)
                continue;

            juce::String drumsToken = tokens[2]; // e.g. "drums0d", "drums0", "drums0dnoflip"
            if (!drumsToken.startsWith("drums"))
                continue;

            // Check for dnoflip — no visual flip
            if (drumsToken.endsWith("dnoflip"))
            {
                if (inFlip[diff])
                {
                    perDiffRegions[diff].push_back({flipStart[diff], evt.position});
                    inFlip[diff] = false;
                }
                continue;
            }

            bool flipOn = drumsToken.endsWith("d");

            if (flipOn && !inFlip[diff])
            {
                flipStart[diff] = evt.position;
                inFlip[diff] = true;
            }
            else if (!flipOn && inFlip[diff])
            {
                perDiffRegions[diff].push_back({flipStart[diff], evt.position});
                inFlip[diff] = false;
            }
        }

        // If still in flip at end of chart, extend to max
        for (int d = 0; d < NUM_DIFFICULTIES; ++d)
            if (inFlip[d])
                perDiffRegions[d].push_back({flipStart[d], PPQ(999999.0)});
    }

    /** Check if position is in a flip region for the given MIDI difficulty (0-3). */
    bool isFlipped(PPQ position, int midiDiff) const
    {
        if (midiDiff < 0 || midiDiff >= NUM_DIFFICULTIES)
            return false;

        for (const auto& r : perDiffRegions[midiDiff])
        {
            if (position >= r.start && position < r.end)
                return true;
            if (position < r.start)
                break; // Regions are sorted by start, no more matches possible
        }
        return false;
    }

    bool hasRegions() const
    {
        for (const auto& r : perDiffRegions)
            if (!r.empty()) return true;
        return false;
    }

    const std::vector<FlipRegion>& getRegions(int midiDiff) const
    {
        if (midiDiff >= 0 && midiDiff < NUM_DIFFICULTIES)
            return perDiffRegions[midiDiff];
        static const std::vector<FlipRegion> empty;
        return empty;
    }

private:
    std::vector<FlipRegion> perDiffRegions[NUM_DIFFICULTIES];
};
