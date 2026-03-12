#pragma once

#include "ChartTextEvent.h"

/**
 * Tracks disco flip regions parsed from MIDI text events on the drums track.
 *
 * Disco flip swaps red <-> yellow lanes during marked sections.
 * Text event format: [mix <diff> drums<N>[d|dnoflip]]
 *   - diff 3 = Expert
 *   - 'd' suffix = flip ON
 *   - no suffix or 'dnoflip' = flip OFF
 */
class DiscoFlipState
{
public:
    struct FlipRegion {
        PPQ start;
        PPQ end;
    };

    DiscoFlipState() = default;

    void buildFromTextEvents(const TrackTextEvents& events)
    {
        regions.clear();

        PPQ flipStart;
        bool inFlip = false;

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
            if (diff != 3) // Only Expert
                continue;

            juce::String drumsToken = tokens[2]; // e.g. "drums0d", "drums0", "drums0dnoflip"
            if (!drumsToken.startsWith("drums"))
                continue;

            // Check for dnoflip — no visual flip
            if (drumsToken.endsWith("dnoflip"))
            {
                if (inFlip)
                {
                    regions.push_back({flipStart, evt.position});
                    inFlip = false;
                }
                continue;
            }

            bool flipOn = drumsToken.endsWith("d");

            if (flipOn && !inFlip)
            {
                flipStart = evt.position;
                inFlip = true;
            }
            else if (!flipOn && inFlip)
            {
                regions.push_back({flipStart, evt.position});
                inFlip = false;
            }
        }

        // If still in flip at end of chart, extend to max
        if (inFlip)
            regions.push_back({flipStart, PPQ(999999.0)});
    }

    bool isFlipped(PPQ position) const
    {
        for (const auto& r : regions)
        {
            if (position >= r.start && position < r.end)
                return true;
            if (position < r.start)
                break; // Regions are sorted by start, no more matches possible
        }
        return false;
    }

    bool hasRegions() const { return !regions.empty(); }

private:
    std::vector<FlipRegion> regions;
};
