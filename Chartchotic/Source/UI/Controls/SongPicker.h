#pragma once

#include <JuceHeader.h>
#include "../../Midi/SongList.h"

// The song the strip is confined to, and a menu to change it. Session comes first and means
// unconfined, which is where a project with fewer than two EVENTS items stays.
//
// Colours come from the caller rather than the theme, so the picker reads as part of the
// strip it sits on rather than as chrome dropped onto it.
class SongPicker : public juce::Component
{
public:
    struct Colours
    {
        juce::Colour background, text, dimText, border;
    };

    SongPicker(Colours c) : colours(c) {}

    void setSongs(const SongList* s) { songs = s; repaint(); }

    void setSelected(int index)
    {
        if (selected == index) return;
        selected = index;
        repaint();
    }

    std::function<void(int)> onSelect;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour(colours.background);
        g.fillRect(bounds);
        g.setColour(colours.border);
        g.drawLine(0.0f, (float)getHeight() - 0.5f,
                   (float)getWidth(), (float)getHeight() - 0.5f, 1.0f);

        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(selected == SongList::SESSION ? colours.dimText : colours.text);
        g.drawText(currentName(), bounds.reduced(TEXT_INSET, 0).withTrimmedRight(CHEVRON_W),
                   juce::Justification::centredLeft, true);

        drawChevron(g);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (songs == nullptr) return;

        juce::PopupMenu menu;
        menu.addItem(SESSION_ITEM_ID, SESSION_LABEL, true, selected == SongList::SESSION);
        menu.addSeparator();

        const auto& list = songs->getSongs();
        for (int i = 0; i < (int)list.size(); ++i)
            menu.addItem(i + 1, list[(size_t)i].name, true, selected == i);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [this](int result)
                           {
                               if (result == 0) return;   // dismissed
                               if (onSelect)
                                   onSelect(result == SESSION_ITEM_ID ? SongList::SESSION
                                                                      : result - 1);
                           });
    }

private:
    // JUCE reads item id 0 as "dismissed", so Session cannot be 0 and the songs are 1-based.
    static constexpr int SESSION_ITEM_ID = 1000000;
    static constexpr const char* SESSION_LABEL = "Session";
    static constexpr int TEXT_INSET = 6;
    static constexpr int CHEVRON_W = 14;
    static constexpr float CHEVRON_H = 4.0f;

    juce::String currentName() const
    {
        const auto* song = songs != nullptr ? songs->at(selected) : nullptr;
        return song != nullptr ? song->name : juce::String(SESSION_LABEL);
    }

    void drawChevron(juce::Graphics& g) const
    {
        float cx = (float)getWidth() - CHEVRON_W;
        float cy = (float)getHeight() * 0.5f;

        juce::Path chevron;
        chevron.addTriangle(cx - CHEVRON_H, cy - CHEVRON_H * 0.5f,
                            cx + CHEVRON_H, cy - CHEVRON_H * 0.5f,
                            cx, cy + CHEVRON_H * 0.5f);

        g.setColour(colours.dimText);
        g.fillPath(chevron);
    }

    Colours colours;
    const SongList* songs = nullptr;
    int selected = SongList::SESSION;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongPicker)
};
