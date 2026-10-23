#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <vector>

#include "InstrumentBadge.h"
#include "../ControlConstants.h"
#include "../Theme.h"

/**
    The instruments something contains, as a row of badges.

    Track names are exact and long, and reading "PART DRUMS, PART ELITE_DRUMS"
    off a line takes longer than seeing two badges does. Pointing at one names
    it, so nothing is lost by not spelling them out.

    Takes track names rather than parts because that is what every caller
    already has: the project reports names, and turning those into instruments
    is this control's job, not each caller's.
*/
class InstrumentBadgeStrip : public juce::Component
{
public:
    InstrumentBadgeStrip()
    {
        setInterceptsMouseClicks(false, true);
    }

    void setTrackNames(const juce::StringArray& names)
    {
        std::vector<Part> found;

        auto add = [&found](Part part)
        {
            if (std::find(found.begin(), found.end(), part) == found.end())
                found.push_back(part);
        };

        // Two tables: what renders and what is only recognised. Both are still
        // instruments in the chart, so both get a badge.
        for (const auto& name : names)
        {
            for (const auto& entry : getImplementedTrackNames())
                if (name.equalsIgnoreCase(entry.name)) add(entry.part);
            for (const auto& entry : getUnimplementedTrackNames())
                if (name.equalsIgnoreCase(entry.name)) add(entry.part);
        }

        std::sort(found.begin(), found.end(), [](Part a, Part b)
                  { return getPartSortOrder(a) < getPartSortOrder(b); });

        if (found == parts) return;

        parts = std::move(found);
        hovered = -1;
        repaint();
    }

    /**
        Whether the drums in here are pro drums.

        The track name cannot say: pro drums is PART DRUMS with a flag set on
        the song, so it is the only variant the strip has to be told about.
    */
    void setProDrums(bool isPro)
    {
        if (proDrums == isPro) return;
        proDrums = isPro;
        repaint();
    }

    bool isEmpty() const { return parts.empty(); }

    /** Room for the badges alone, with nothing set aside for the hovered name. */
    int badgesWidth() const { return (int)parts.size() * (badgeSize + gap); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();

        for (int i = 0; i < (int)parts.size(); ++i)
        {
            auto slot = area.removeFromLeft(badgeSize);
            area.removeFromLeft(gap);

            const auto part = parts[(size_t)i];
            const bool stampPro = proDrums && part == Part::DRUMS;

            InstrumentBadge::draw(g, slot, part, i == hovered ? 1.0f : dimOpacity,
                                  stampPro ? "PRO" : juce::String());
        }

        // The name goes beside the badges rather than in a tooltip window:
        // there is no tooltip window to put it in, and the room is here.
        if (hovered < 0 || hovered >= (int)parts.size()) return;

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(nameFontHeight));
        g.drawText(getPartDisplayName(parts[(size_t)hovered]), area.withTrimmedLeft(gap),
                   juce::Justification::centredLeft, true);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int index = e.x / (badgeSize + gap);
        setHovered(index < (int)parts.size() && e.x < badgesWidth() ? index : -1);
    }

    void mouseExit(const juce::MouseEvent&) override { setHovered(-1); }

private:
    void setHovered(int index)
    {
        if (index == hovered) return;
        hovered = index;
        repaint();
    }

    static constexpr int   badgeSize      = 26;
    static constexpr int   gap            = 4;
    static constexpr float nameFontHeight = 10.5f;
    static constexpr float dimOpacity     = 0.65f;

    std::vector<Part> parts;
    int hovered = -1;
    bool proDrums = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentBadgeStrip)
};
