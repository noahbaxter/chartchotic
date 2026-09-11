#pragma once

#include "ChartTextEvent.h"
#include <algorithm>

/**
 * Sections and [coda] from the EVENTS track. Song-scoped: no part, no difficulty.
 *
 * Three spellings are all valid and all appear in the wild: "[section name]",
 * "[section_name]" and "[prc_name]". Official Harmonix charts use both the section and
 * prc forms depending on era, so the parser accepts all three and stores the bare name.
 */
class SongSections
{
public:
    struct Section
    {
        PPQ position;
        juce::String name;
    };

    SongSections() = default;

    void buildFromTextEvents(const TrackTextEvents& events)
    {
        sections.clear();
        endMarkers.clear();
        codaPPQ = PPQ(NEVER);
        endPPQ = PPQ(0.0);

        for (const auto& evt : events)
        {
            auto text = evt.text.trim();
            if (text == "[coda]")
            {
                if (evt.position < codaPPQ) codaPPQ = evt.position;
                continue;
            }
            if (text == "[end]" || text == "[music_end]")
            {
                endMarkers.push_back(evt.position);
                if (evt.position > endPPQ) endPPQ = evt.position;
                continue;
            }

            juce::String name;
            if (parseSectionName(text, name))
                sections.push_back({ evt.position, name });
        }

        std::sort(sections.begin(), sections.end(),
                  [](const Section& a, const Section& b) { return a.position < b.position; });
        std::sort(endMarkers.begin(), endMarkers.end());
    }

    const std::vector<Section>& getSections() const { return sections; }
    bool hasSections() const { return !sections.empty(); }

    // A fill after the coda is a Big Rock Ending rather than a drum fill.
    // Last [end] / [music_end], or 0 if the chart has neither. The map zoom needs a span.
    PPQ getEndPPQ() const { return endPPQ; }

    // Last end marker strictly inside (from, to), or `from` when there is none. Several
    // charts can share one timeline, each with its own [music_end] and [end], so these are
    // clip terminators rather than one song end: what stops a clip's last section from
    // running across the gap into the next chart. The last marker in the span wins, so the
    // pair collapses to [end] rather than cutting at [music_end] a bar or two earlier.
    PPQ endMarkerBetween(PPQ from, PPQ to) const
    {
        PPQ found = from;
        for (auto e : endMarkers)
        {
            if (e <= from) continue;
            if (e >= to) break;
            found = e;
        }
        return found;
    }

    bool hasCoda() const { return codaPPQ < PPQ(NEVER); }
    PPQ getCodaPPQ() const { return codaPPQ; }
    bool isAfterCoda(PPQ position) const { return hasCoda() && position >= codaPPQ; }

    // The section covering `position`, or nullptr before the first one.
    const Section* sectionAt(PPQ position) const
    {
        const Section* found = nullptr;
        for (const auto& s : sections)
        {
            if (s.position > position) break;
            found = &s;
        }
        return found;
    }

    static bool parseSectionName(const juce::String& text, juce::String& outName)
    {
        if (!text.startsWith("[") || !text.endsWith("]")) return false;
        auto body = text.substring(1, text.length() - 1);

        if (body.startsWith("prc_"))          outName = body.substring(4);
        else if (body.startsWith("section "))  outName = body.substring(8);
        else if (body.startsWith("section_"))  outName = body.substring(8);
        else return false;

        outName = outName.trim();
        return outName.isNotEmpty();
    }

private:
    static constexpr double NEVER = 1e12;
    std::vector<Section> sections;
    std::vector<PPQ> endMarkers;
    PPQ codaPPQ { NEVER };
    PPQ endPPQ { 0.0 };
};
