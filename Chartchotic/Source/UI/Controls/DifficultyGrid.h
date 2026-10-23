#pragma once

#include <JuceHeader.h>

#include <vector>

#include "InstrumentBadge.h"
#include "../ControlConstants.h"
#include "../Theme.h"

/**
    Every instrument a chart could hold, and how hard each one is.

    Shows all of them rather than only the ones that exist, because what is
    missing is as much a fact about a chart as what is there: a row of greyed
    badges says "no guitar" in a way that an absent guitar cannot.

    A badge is lit when the instrument is part of the export and grey when it
    is not, and clicking one switches it. Instruments with notes start lit.

    The rating is typed on the badge rather than into a field beside it, so
    which number belongs to which instrument is not something to work out.
*/
class DifficultyGrid : public juce::Component
{
public:
    struct Entry
    {
        Part part = Part::GUITAR;

        /** Stamped across the badge: PRO, ELITE, and so on. */
        juce::String variant;

        /** In the export. Off is drawn grey. */
        bool active = false;

        /** There is a track for this instrument in the project. */
        bool present = false;

        /** That track has notes in this song's range. */
        bool hasNotes = false;

        int rating = -1;
    };

    DifficultyGrid() { setWantsKeyboardFocus(true); }

    void setEntries(std::vector<Entry> newEntries)
    {
        entries = std::move(newEntries);
        repaint();
    }

    const std::vector<Entry>& getEntries() const { return entries; }

    /** Which badge was clicked, for the owner to act on. */
    std::function<void(int index)> onToggle;

    /**
        Nudge a rating up or down, by one step per notch.

        A rating is a number between nothing and six, so rolling the wheel
        over the instrument it belongs to is the shortest way to say it. The
        owner decides what a step means and where the number is kept.
    */
    std::function<void(int index, int delta)> onAdjustRating;

    /**
        Set a rating outright, from the list that opens under its badge.

        Scrolling is the quick way when you know roughly where you are going;
        this is the way when you know the answer, and it is the only way to
        get back to unrated.
    */
    std::function<void(int index, int rating)> onSetRating;

    /** What a rating of none is worth, so the list can offer it. */
    static constexpr int unrated = -1;

    /** How wide and tall the badges need, so a caller can size the section. */
    static int heightNeeded() { return badgeSize + gap + ratingHeight + gap + noteHeight; }
    int widthNeeded() const { return (int)entries.size() * (badgeSize + gap); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();

        for (int i = 0; i < (int)entries.size(); ++i)
        {
            const auto& entry = entries[(size_t)i];

            auto column = area.removeFromLeft(badgeSize);
            area.removeFromLeft(gap);

            auto badge = column.removeFromTop(badgeSize);

            // Three states, not two: in the export, available but left out,
            // and not in the project at all.
            const float opacity = entry.active ? 1.0f
                                 : entry.present ? inactiveOpacity
                                                 : absentOpacity;

            if (entry.active)
            {
                g.setColour(juce::Colour(Theme::coral).withAlpha(litBacking));
                g.fillRoundedRectangle(badge.toFloat(), corner);
            }

            InstrumentBadge::draw(g, badge.reduced(badgePad), entry.part, opacity, entry.variant);

            // The rating under its own badge, large enough to read across the
            // window, since it is the number this whole section is about.
            auto rating = column.removeFromTop(ratingHeight + gap);

            if (entry.active)
            {
                const bool unrated = entry.rating < 0;

                g.setColour(juce::Colour(unrated ? Theme::coral : Theme::textWhite));
                g.setFont(Theme::getUIFont(unrated ? (float)ratingHeight * 0.5f
                                                   : (float)ratingHeight));
                g.drawText(unrated ? "?" : juce::String(entry.rating), rating,
                           juce::Justification::centred);
            }

            // Why a lit instrument would still export nothing.
            if (entry.active && !entry.hasNotes)
            {
                g.setColour(juce::Colour(Theme::coral));
                g.setFont(Theme::getUIFont(noteFont));
                g.drawFittedText("no notes", column, juce::Justification::centredTop, 1, squeeze);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int index = columnAt(e.x);
        if (index < 0) return;

        const auto& entry = entries[(size_t)index];

        // An instrument the project does not have cannot be exported, so its
        // badge is a statement rather than a switch.
        if (!entry.present) return;

        // The number under a badge is its own target: clicking the picture
        // says whether the instrument is in, clicking the number says how
        // hard it is, and those are different questions.
        if (e.y >= badgeSize && entry.active)
        {
            showRatingMenu(index);
            return;
        }

        if (onToggle) onToggle(index);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const int index = columnAt(e.x);
        if (index < 0 || onAdjustRating == nullptr) return;
        if (!entries[(size_t)index].active) return;

        // One step per gesture, whatever the trackpad reports. A rating has
        // seven values in it and a flick should not cross all of them.
        const float travel = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
        if (std::abs(travel) < wheelThreshold) return;

        onAdjustRating(index, travel > 0.0f ? 1 : -1);
    }

private:
    /** Every rating there is, under the badge it belongs to. */
    void showRatingMenu(int index)
    {
        if (onSetRating == nullptr) return;

        const int current = entries[(size_t)index].rating;

        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        menu.addItem(1, "unrated", true, current == unrated);
        menu.addSeparator();
        for (int rating = lowest; rating <= highest; ++rating)
            menu.addItem(rating + 2, juce::String(rating), true, rating == current);

        // Anchored under its own badge rather than at the pointer, so the
        // list belongs to the instrument it is about.
        auto badge = juce::Rectangle<int>(index * (badgeSize + gap), 0, badgeSize, badgeSize);

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(this)
                               .withTargetScreenArea(localAreaToGlobal(badge)),
                           [this, index](int choice)
                           {
                               if (choice > 0) onSetRating(index, choice == 1 ? unrated : choice - 2);
                           });
    }

    juce::Rectangle<int> localAreaToGlobal(juce::Rectangle<int> area) const
    {
        return area + getScreenPosition();
    }

    /** Which badge sits under this x, or -1 for the gaps between them. */
    int columnAt(int x) const
    {
        const int index = x / (badgeSize + gap);
        return index >= 0 && index < (int)entries.size() ? index : -1;
    }

    /** What the chart format's ratings run between. */
    static constexpr int lowest  = 0;
    static constexpr int highest = 6;

    static constexpr float wheelThreshold = 0.02f;
    static constexpr int   badgeSize      = 40;
    static constexpr int   badgePad       = 3;
    static constexpr int   gap            = 8;
    static constexpr int   ratingHeight   = 20;
    static constexpr int   noteHeight     = 12;
    static constexpr float noteFont       = 9.5f;
    static constexpr float corner         = 4.0f;
    static constexpr float litBacking     = 0.16f;
    static constexpr float inactiveOpacity = 0.4f;
    static constexpr float absentOpacity   = 0.16f;
    static constexpr float squeeze         = 0.5f;

    std::vector<Entry> entries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DifficultyGrid)
};
