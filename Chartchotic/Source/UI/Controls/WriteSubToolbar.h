#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../HelpTypes.h"
#include "PillToggle.h"
#include "SegmentedButtons.h"
#include "DropdownButton.h"
#include "WriteModeIcon.h"
#include "../../Editor/InteractionController.h"
#include "../../Visual/Art/GemIconArt.h"

class WriteSubToolbar : public juce::Component
{
public:
    explicit WriteSubToolbar(InteractionController& ic) : interactionController(ic)
    {

        modeButtons.setItems({"Draw", "Edit"});
        cursorIcon = juce::Drawable::createFromImageData(
            BinaryData::icon_cursor_svg, BinaryData::icon_cursor_svgSize);
        modeButtons.setItemIcons({
            [this](juce::Graphics& g, juce::Rectangle<float> r) {
                g.setColour(glyphColour(SubMode::Draw));
                WriteModeIcon::drawPencil(g, squareIn(r, PENCIL_GLYPH_SCALE));
                return true;
            },
            [this](juce::Graphics& g, juce::Rectangle<float> r) {
                if (cursorIcon == nullptr) return false;
                auto tinted = cursorIcon->createCopy();
                tinted->replaceColour(juce::Colours::black, glyphColour(SubMode::Edit));
                tinted->drawWithin(g, squareIn(r, POINTER_GLYPH_SCALE), juce::RectanglePlacement::centred, 1.0f);
                return true;
            },
        });
        addAndMakeVisible(modeButtons);

        barToggle.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(barToggle);

        laneToggle.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(laneToggle);

        snapToggle.setToggleState(ic.snapEnabled(), juce::dontSendNotification);
        snapToggle.setIcon(juce::Drawable::createFromImageData(
            BinaryData::icon_snap_magnet_svg, BinaryData::icon_snap_magnet_svgSize));
        addAndMakeVisible(snapToggle);

        juce::StringArray divisionLabels;
        for (int d : STEP_DIVISIONS) divisionLabels.add("1/" + juce::String(d));
        divisionDropdown.setItems(divisionLabels);
        addAndMakeVisible(divisionDropdown);

        juce::StringArray tupletLabels;
        for (int t : TUPLETS) tupletLabels.add(t == 0 ? "Off" : juce::String(t));
        tupletDropdown.setItems(tupletLabels);
        addAndMakeVisible(tupletDropdown);

        guitarForceButtons.setItems({"Auto", "HOPO", "Strum", "Tap"});
        addChildComponent(guitarForceButtons);


        drumDynamicButtons.setItems({"Ghost", "Normal", "Accent"});
        addChildComponent(drumDynamicButtons);

        cymbalToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(cymbalToggle);

        flamToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(flamToggle);

        addAndMakeVisible(phraseKindButtons);

        for (auto* seg : { &modeButtons, &guitarForceButtons, &drumDynamicButtons,
                           &phraseKindButtons })
            seg->setCornerRadius(3.0f);
        for (auto* pill : { &snapToggle, &barToggle, &laneToggle, &cymbalToggle, &flamToggle })
            pill->setCornerRadius(3.0f);
        divisionDropdown.setCornerRadius(3.0f);
        tupletDropdown.setCornerRadius(3.0f);
        divisionDropdown.setArrowVisible(false);
        tupletDropdown.setArrowVisible(false);

        addHoverZone(modeButtons,        {"Q"},      "draw places notes, edit selects & modifies");
        addHoverZone(phraseKindButtons,  {"A", "S", "D", "F"}, "which phrase a drag writes");
        // Modifier hints run in the same order as the segments they sit under,
        // matching the home-row slots in ModifierSlots.h.
        barHelpZone = addHoverZone(barToggle, {"B"}, "only interact with open notes");
        addHoverZone(laneToggle, {"R"}, "drag writes a roll lane instead of notes");
        addHoverZone(snapToggle,         {"E"},                    "lock note placement to grid");
        addHoverZone(divisionDropdown,   {"[", "]", "⌥scroll"},    "grid spacing (1/4, 1/8, 1/16...)");
        addHoverZone(tupletDropdown,     {"T", "⇧T"},              "tuplet on/off, shift cycles 3/5/7");
        addHoverZone(guitarForceButtons, {"A", "S", "D", "F"},     "force HOPO, strum, or tap (overrides auto)");
        addHoverZone(drumDynamicButtons, {"A", "S", "D"},          "ghost (soft) or accent (loud) hit");
        addHoverZone(cymbalToggle,       {"F"},                    "cymbal placement (PRO drums)");
        addHoverZone(flamToggle,         {"F"},                    "flam: grace note before the hit, kicks place 1x + 2x");
    }

    // Note types, open/kick and cymbal are drawn with the chart's own gem art, so the button
    // looks like the note it places. Until art is loaded they fall back to their words.
    void setAssetManager(AssetManager* am)
    {
        if (am == nullptr) return;
        assetManager = am;
        gemIcons = std::make_unique<GemIconArt>(*am);

        using G = Gem;
        auto guitar = [this](G gem) {
            return [this, gem](juce::Graphics& g, juce::Rectangle<float> r) {
                return gemIcons->drawGuitarGem(g, r, gem);
            };
        };
        auto drum = [this](G gem) {
            return [this, gem](juce::Graphics& g, juce::Rectangle<float> r) {
                return gemIcons->drawDrumGem(g, r, gem);
            };
        };

        // Auto has no gem of its own, and half of one reads as neither, so it is a letter in
        // the segment's own text colour.
        auto autoForce = [](juce::Graphics& g, juce::Rectangle<float> r) {
            g.setFont(Theme::getUIFont(r.getHeight() * AUTO_GLYPH_SCALE).boldened());
            g.drawText("A", r, juce::Justification::centred, false);
            return true;
        };

        guitarForceButtons.setItemIcons({ autoForce, guitar(G::HOPO_GHOST), guitar(G::NOTE),
                                          guitar(G::TAP_ACCENT) });
        drumDynamicButtons.setItemIcons({ drum(G::HOPO_GHOST), drum(G::NOTE), drum(G::TAP_ACCENT) });

        cymbalToggle.setArt([this](juce::Graphics& g, juce::Rectangle<float> r) {
            return !gemIcons->drawImage(g, r, assetManager->getCymYellowImage()).isEmpty();
        });
        flamToggle.setArt([this](juce::Graphics& g, juce::Rectangle<float> r) {
            return gemIcons->drawFlam(g, r);
        });
        laneToggle.setArt([this](juce::Graphics& g, juce::Rectangle<float> r) {
            return gemIcons->drawLane(g, r, juce::Colour(Theme::controlLaneArt));
        });
        barToggle.setArt([this](juce::Graphics& g, juce::Rectangle<float> r) {
            bool drums = isDrumLike(interactionController.focusPart());
            return !gemIcons->drawImage(g, r, drums ? assetManager->getBarKickImage()
                                                    : assetManager->getBarOpenImage()).isEmpty();
        });
    }

    std::function<void(EditorView)> onViewChanged;
    std::function<void(PhraseMath::Kind)> onPhraseKindChanged;
    std::function<void(SubMode)> onSubModeChanged;
    std::function<void(bool)> onSnapChanged;
    // The chosen value, not a delta: the dropdown already knows which one was picked, so the
    // handler no longer has to find the current value in a table before it can move off it.
    std::function<void(bool)> onLaneToolChanged;
    std::function<void(int division)> onStepDivisionChanged;
    std::function<void(int tuplet)> onTupletChanged;
    std::function<void(bool)> onBarModeChanged;
    std::function<void(GuitarForce)> onGuitarForceChanged;
    std::function<void(DrumDynamic)> onDrumDynamicChanged;
    std::function<void(bool)> onCymbalModeChanged;
    std::function<void(bool)> onFlamModeChanged;

    std::function<void(const HelpText&)> onHoverHelp;
    std::function<void()> onHoverHelpClear;

    void wireCallbacks()
    {
        // Picking what to write is what chooses the view. Both sets of types sit on the bar at
        // once, so reaching for a phrase kind says "phrase" as plainly as a view control would,
        // and there is no state where the thing you clicked is not the thing you are writing.
        phraseKindButtons.onSelectionChanged = [this](int idx) {
            auto kinds = PhraseMath::kindsFor(interactionController.focusPart());
            if (onPhraseKindChanged && idx >= 0 && idx < (int)kinds.size())
                onPhraseKindChanged(kinds[(size_t)idx]);
            if (onLaneToolChanged) onLaneToolChanged(false);
            selectView(EditorView::Phrase);
        };

        modeButtons.onSelectionChanged = [this](int idx) {
            if (onSubModeChanged)
                onSubModeChanged(idx == 0 ? SubMode::Draw : SubMode::Edit);
        };

        snapToggle.onClick = [this]() {
            if (onSnapChanged) onSnapChanged(snapToggle.getToggleState());
        };

        divisionDropdown.onSelectionChanged = [this](int idx) {
            if (onStepDivisionChanged && idx >= 0 && idx < (int)std::size(STEP_DIVISIONS))
                onStepDivisionChanged(STEP_DIVISIONS[idx]);
        };

        tupletDropdown.onSelectionChanged = [this](int idx) {
            if (onTupletChanged && idx >= 0 && idx < (int)std::size(TUPLETS))
                onTupletChanged(TUPLETS[idx]);
        };

        barToggle.onClick = [this]() {
            if (onBarModeChanged) onBarModeChanged(barToggle.getToggleState());
        };

        // Picks lanes rather than toggling them: it is one of the exclusive choices, so it stays
        // on until a note type or phrase kind takes the turn, the same as those do to each other.
        laneToggle.onClick = [this]() {
            if (onLaneToolChanged) onLaneToolChanged(true);
            selectView(EditorView::Notes);
        };

        guitarForceButtons.onSelectionChanged = [this](int idx) {
            static const GuitarForce forces[] = { GuitarForce::None, GuitarForce::Hopo,
                                                   GuitarForce::Strum, GuitarForce::Tap };
            if (onGuitarForceChanged) onGuitarForceChanged(forces[idx]);
            if (onLaneToolChanged) onLaneToolChanged(false);
            selectView(EditorView::Notes);
        };

        drumDynamicButtons.onSelectionChanged = [this](int idx) {
            if (onDrumDynamicChanged) onDrumDynamicChanged(DYNAMIC_ORDER[idx]);
            if (onLaneToolChanged) onLaneToolChanged(false);
            selectView(EditorView::Notes);
        };

        cymbalToggle.onClick = [this]() {
            if (onCymbalModeChanged) onCymbalModeChanged(cymbalToggle.getToggleState());
        };

        flamToggle.onClick = [this]() {
            if (onFlamModeChanged) onFlamModeChanged(flamToggle.getToggleState());
        };

        for (auto& hz : hoverZones)
        {
            hz->onHelp = &onHoverHelp;
            hz->onClear = &onHoverHelpClear;
        }
    }

    void refreshFromController()
    {
        bool drums = isDrumLike(interactionController.focusPart());
        bool drawMode = interactionController.subMode() == SubMode::Draw;
        bool editMode = !drawMode;
        bool hasSel = editMode && interactionController.hasEditSelection();
        bool notesView = interactionController.view() == EditorView::Notes;
        const bool phraseView = interactionController.view() == EditorView::Phrase;
        // Lanes are written on the note highway, so the view alone cannot say whether a note type
        // or the lane tool holds the turn.
        const bool laneMode = interactionController.laneTool();
        const bool noteTypeActive = notesView && !laneMode;
        // One neutral for both sub-modes. Draw and Edit are told apart by which segment is lit,
        // not by the bar changing colour under you.
        juce::Colour modeAccent = juce::Colour(Theme::controlSelected);

        // Open/kick filtering only means anything while gems are what a click writes, so it greys
        // in a phrase view rather than vanishing: the bar keeps one shape whatever is selected.

        if (barHelpZone)
            barHelpZone->setDescription({"B"}, drums ? "only interact with kick notes"
                                                     : "only interact with open notes");

        modeButtons.setAccentColour(modeAccent);
        snapToggle.setAccentColour(modeAccent);
        divisionDropdown.setAccentColour(modeAccent);
        tupletDropdown.setAccentColour(modeAccent);
        barToggle.setAccentColour(modeAccent);
        laneToggle.setAccentColour(modeAccent);
        guitarForceButtons.setAccentColour(modeAccent);
        drumDynamicButtons.setAccentColour(modeAccent);
        cymbalToggle.setAccentColour(modeAccent);
        flamToggle.setAccentColour(modeAccent);

        int modeIdx = drawMode ? 0 : 1;
        modeButtons.setSelectedIndex(modeIdx);

        bool snapOn = interactionController.snapEnabled();
        snapToggle.setToggleState(snapOn);

        // The wheel and the [ ] keys can land on a division that is not in the table, so nothing
        // is selected rather than the nearest being shown as exact.
        int div = interactionController.stepDivision();
        divisionDropdown.setSelectedIndex(indexOf(STEP_DIVISIONS, div));
        divisionDropdown.setAlpha(snapOn ? 1.0f : 0.35f);
        divisionDropdown.setInterceptsMouseClicks(snapOn, snapOn);

        int tuplet = interactionController.tuplet();
        tupletDropdown.setSelectedIndex(indexOf(TUPLETS, tuplet));
        tupletDropdown.setAlpha(snapOn ? 1.0f : 0.35f);
        tupletDropdown.setInterceptsMouseClicks(snapOn, snapOn);

        barToggle.setToggleState(interactionController.barMode());
        barToggle.setButtonText(drums ? "KICK" : "OPEN");

        // Over a selection LANE describes the notes, not the tool: lit when they already sit
        // under a lane, so clicking it again is visibly the way to take it off.
        const bool laneOverSelection = editMode && hasSel;
        laneToggle.setToggleState(laneOverSelection ? interactionController.selectionHasLane()
                                                    : interactionController.laneTool());

        // Cym and Flam share one pill slot; no kit has both.
        const auto* cfg = getAuthoringConfig(interactionController.focusPart());
        bool showCym  = drums && cfg && cfg->hasCymbalToggle;
        bool showFlam = drums && cfg && cfg->hasFlamToggle;

        // The Phrase view writes phrases, so its slot picks which one rather than a note type.
        auto kinds = PhraseMath::kindsFor(interactionController.focusPart());
        juce::StringArray kindLabels;
        for (auto k : kinds) kindLabels.add(PhraseMath::phraseShortLabel(k));
        phraseKindButtons.setItems(kindLabels);
        phraseKindButtons.setAccentColour(modeAccent);
        // Note types and phrase kinds are one exclusive choice spread over two groups: whichever
        // is selected is what a drag writes, and the view follows it. The other group clears its
        // selection rather than greying out, so both stay one click away.
        if (phraseView)
        {
            for (int i = 0; i < (int)kinds.size(); ++i)
                if (kinds[(size_t)i] == interactionController.phraseKind())
                    phraseKindButtons.setSelectedIndex(i);
        }
        else
        {
            phraseKindButtons.setSelectedIndex(-1);
        }

        // Both kinds of type sit on the bar at once: clicking one is what picks the view, so
        // hiding the other would leave no way back to it.
        phraseKindButtons.setVisible(true);
        guitarForceButtons.setVisible(!drums);
        drumDynamicButtons.setVisible(drums);
        cymbalToggle.setVisible(showCym);
        flamToggle.setVisible(showFlam);

        // Selectable whatever the view is: clicking one is how you get back to the note view,
        // so they stay live and simply show nothing selected while a phrase kind holds the turn.
        bool modifiersEnabled = drawMode || hasSel;

        if (drums)
        {
            drumDynamicButtons.setAlpha(modifiersEnabled ? 1.0f : 0.35f);
            drumDynamicButtons.setInterceptsMouseClicks(modifiersEnabled, modifiersEnabled);
            // Cymbal is only drawable on a plain drum note, so it greys anywhere a click writes
            // something else. Without this it would be settable in a mode that drops it again.
            const bool plainNotes = noteTypeActive && !interactionController.barMode();
            cymbalToggle.setDisabled(!modifiersEnabled || !plainNotes);
            flamToggle.setDisabled(!modifiersEnabled);

            if (!noteTypeActive) drumDynamicButtons.setSelectedIndex(-1);
            else if (modifiersEnabled)
            {
                auto dyn = interactionController.drumDynamic();
                for (int i = 0; i < (int)std::size(DYNAMIC_ORDER); ++i)
                    if (DYNAMIC_ORDER[i] == dyn) drumDynamicButtons.setSelectedIndex(i);
                cymbalToggle.setToggleState(interactionController.cymbalMode());
                flamToggle.setToggleState(interactionController.flamMode());
            }
        }
        else
        {
            guitarForceButtons.setAlpha(modifiersEnabled ? 1.0f : 0.35f);
            guitarForceButtons.setInterceptsMouseClicks(modifiersEnabled, modifiersEnabled);

            if (!noteTypeActive) guitarForceButtons.setSelectedIndex(-1);
            else if (modifiersEnabled)
            {
                auto force = interactionController.guitarForce();
                int forceIdx = 0;
                if (force == GuitarForce::Hopo) forceIdx = 1;
                else if (force == GuitarForce::Strum) forceIdx = 2;
                else if (force == GuitarForce::Tap) forceIdx = 3;
                guitarForceButtons.setSelectedIndex(forceIdx);
            }
        }

        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Neutral like the strip's ground: the selected buttons already say which mode is on,
        // and a tinted bar just competes with them.
        g.setColour(juce::Colour(Theme::panelNeutralBg));
        g.fillRect(getLocalBounds());

        g.setColour(juce::Colour(Theme::panelNeutralLine));
        g.drawHorizontalLine(0, 0.0f, (float)getWidth());

        if (SUBTOOLBAR_SHOW_LABELS && labelH > 0)
        {
            float fs = juce::jmax(7.0f, (float)labelH * 0.55f);
            juce::Font labelFont = Theme::getUIFont(fs)
                .withHorizontalScale(1.25f)
                .withExtraKerningFactor(0.12f);
            g.setFont(labelFont);
            g.setColour(juce::Colours::white.withAlpha(0.25f));

            auto draw = [&](const char* text, juce::Rectangle<int> r) {
                g.drawText(text, r.getX(), 0, r.getWidth(), labelH,
                           juce::Justification::centred, false);
            };
            draw("MODE",      labelGroupMode);
            draw("NOTE TYPE", labelGroupNote);
            draw("PHRASE",    labelGroupPhrase);
            draw("GRID",      labelGroupGrid);
            // No VIEW label: the stepper's value is the view's name, so a label over it would
            // only say the same word twice.
        }
    }

    void resized() override
    {
        int h = getHeight();
        if (h <= 0) return;

        bool drums = isDrumLike(interactionController.focusPart());

        labelH = SUBTOOLBAR_SHOW_LABELS ? juce::jmax(10, juce::roundToInt(h * 0.3f)) : 0;
        int controlArea = h - labelH;
        int padTop = juce::roundToInt(controlArea * SUBTOOLBAR_PAD_TOP);
        int padBot = juce::roundToInt(controlArea * SUBTOOLBAR_PAD_BOTTOM);
        int available = controlArea - padTop - padBot;

        // Every button is a square of one size, so the row reads as a set rather than a pile
        // of widths. Cym and Flam share a square (no kit has both), open/kick takes two for
        // its bar art. Steppers and the view picker hold text, so they are the exceptions.
        // The note-type slot is the same width on every instrument: guitar's four gems, or
        // drums' three plus Cym. Switching parts swaps what is inside it and nothing else
        // on the row moves.
        const int noteSquares = juce::jmax((int)guitarForceItems, (int)drumDynamicItems + 1);
        // BAR keeps its two squares for the bar art; LANE is a single square beside it.
        const float squares = MODE_SQUARES + (float)noteSquares + SUBTOOLBAR_BAR_SQUARES + 1.0f
                            + SNAP_SQUARES;
        const float gapSquares = SUBTOOLBAR_GROUP_GAP * 3.0f + SUBTOOLBAR_ITEM_GAP * 4.0f
                               + SUBTOOLBAR_LEFT_MARGIN;

        // A stepper is two arrow zones that scale with the squares plus room for its value in
        // pixels, so the value text comes off the width first and the squares shrink into what
        // is left. A tall row in a narrow window would otherwise run off both ends.
        // Text-sized controls take their pixels off the width first, then the squares divide what
        // is left. One chevron per dropdown, not the two arrow zones a stepper needed.
        const int phraseW = PHRASE_MAX_KINDS * phraseItemWidth();
        // No chevron on these: they are clicked or scrolled, so the whole width is the value.
        const float chevronSquares = 0.0f;
        const float fixedPx = (float)(divisionTextWidth() + tupletTextWidth() + phraseW);
        const float fitSide = ((float)getWidth() * SUBTOOLBAR_FIT_FRACTION - fixedPx)
                            / (squares + gapSquares + chevronSquares);
        const int side = juce::jmax(8, juce::jmin(juce::roundToInt(available * SUBTOOLBAR_HEIGHT_SCALE),
                                                  (int)fitSide));

        const int divisionW = divisionTextWidth();
        const int tupletW   = tupletTextWidth();

        const int groupGap = juce::roundToInt(side * SUBTOOLBAR_GROUP_GAP);
        const int itemGap  = juce::jmax(2, juce::roundToInt(side * SUBTOOLBAR_ITEM_GAP));
        const int barW  = juce::roundToInt(side * SUBTOOLBAR_BAR_SQUARES);

        const int modeW = side * MODE_SQUARES;
        const int noteSlotW = side * noteSquares + itemGap;
        const int noteW = noteSlotW + itemGap * 2 + barW + side;
        const int gridW = side * SNAP_SQUARES + itemGap * 2 + divisionW + tupletW;
        const int totalW = modeW + noteW + phraseW + gridW + groupGap * 3;

        const int controlY = labelH + padTop + (available - side) / 2;
        // Left-aligned, but never off the end: a narrow window falls back to centring what fits.
        int x = juce::roundToInt(side * SUBTOOLBAR_LEFT_MARGIN);
        if (x + totalW > getWidth()) x = juce::jmax(0, (getWidth() - totalW) / 2);

        auto place = [&](juce::Component& c, int w) {
            c.setBounds(x, controlY, w, side);
            x += w;
        };

        labelGroupMode = { x, 0, modeW, labelH };
        place(modeButtons, modeW);
        x += groupGap;

        labelGroupNote = { x, 0, noteW, labelH };
        const int noteSlotEnd = x + noteSlotW;
        if (drums)
        {
            place(drumDynamicButtons, side * (int)drumDynamicItems);
            x += itemGap;
            cymbalToggle.setBounds(x, controlY, side, side);
            place(flamToggle, side);
        }
        else
        {
            // Centred in the slot, so the spare gap splits either side of the gems.
            x += (noteSlotW - side * (int)guitarForceItems) / 2;
            place(guitarForceButtons, side * (int)guitarForceItems);
        }
        x = noteSlotEnd + itemGap;
        // LANE is one square: its art is a single lane up one column, not a board-wide bar, so
        // the two squares BAR needs for its bar art left it swimming in empty space.
        place(laneToggle, side);
        x += itemGap;
        place(barToggle, barW);
        x += groupGap;

        // Centred in a slot sized for the most kinds any instrument has, so the segments keep one
        // width everywhere and only the spare gap either side changes with the part.
        labelGroupPhrase = { x, 0, phraseW, labelH };
        const int phraseSlotEnd = x + phraseW;
        const int phraseItems = (int)PhraseMath::kindsFor(interactionController.focusPart()).size();
        x += (phraseW - phraseItems * phraseItemWidth()) / 2;
        place(phraseKindButtons, phraseItems * phraseItemWidth());
        x = phraseSlotEnd + groupGap;

        labelGroupGrid = { x, 0, gridW, labelH };
        place(snapToggle, side * SNAP_SQUARES);
        x += itemGap;
        place(divisionDropdown, divisionW);
        x += itemGap;
        place(tupletDropdown, tupletW);
        x += groupGap;

    }

private:
    struct HoverHelpZone : public juce::MouseListener
    {
        juce::Component& target;
        HelpText helpText;
        std::function<void(const HelpText&)>* onHelp = nullptr;
        std::function<void()>* onClear = nullptr;

        HoverHelpZone(juce::Component& t, std::initializer_list<juce::String> keys, const juce::String& desc)
            : target(t)
        {
            for (const auto& k : keys)
                helpText.push_back(key(k));
            helpText.push_back(dim(desc));
            target.addMouseListener(this, false);
        }

        ~HoverHelpZone() override { target.removeMouseListener(this); }

        void setDescription(std::initializer_list<juce::String> newKeys, const juce::String& desc)
        {
            helpText.clear();
            for (const auto& k : newKeys)
                helpText.push_back(key(k));
            helpText.push_back(dim(desc));
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            if (onHelp && *onHelp) (*onHelp)(helpText);
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            if (onClear && *onClear) (*onClear)();
        }
    };

    HoverHelpZone* addHoverZone(juce::Component& target, std::initializer_list<juce::String> keys, const juce::String& desc)
    {
        auto zone = std::make_unique<HoverHelpZone>(target, keys, desc);
        auto* ptr = zone.get();
        hoverZones.push_back(std::move(zone));

        // The same words the help strip gets, as a tooltip on the control itself. With the group
        // headers gone the strip is the only thing naming these, and it only reads if you know
        // to look at it.
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&target))
        {
            juce::StringArray shortcuts;
            for (const auto& k : keys) shortcuts.add(k);
            client->setTooltip(shortcuts.isEmpty() ? desc
                                                   : desc + "  (" + shortcuts.joinIntoString(", ") + ")");
        }
        return ptr;
    }

    InteractionController& interactionController;
    AssetManager* assetManager = nullptr;
    std::unique_ptr<GemIconArt> gemIcons;
    std::unique_ptr<juce::Drawable> cursorIcon;

    // The pencil and pointer are line glyphs, so they read at a smaller size than the gem art
    // and would look oversized filling their squares.
    static juce::Rectangle<float> squareIn(juce::Rectangle<float> r, float scale)
    {
        float side = juce::jmin(r.getWidth(), r.getHeight()) * scale;
        return juce::Rectangle<float>(side, side).withCentre(r.getCentre());
    }
    // The pointer's art fills its box edge to edge where the pencil runs diagonally inside
    // its own, so it gets the smaller box to read the same weight.
    static constexpr float PENCIL_GLYPH_SCALE = 0.68f;
    static constexpr float POINTER_GLYPH_SCALE = 0.5f;

    // White on the selected segment, dim on the other, like the words were.
    juce::Colour glyphColour(SubMode forMode) const
    {
        return interactionController.subMode() == forMode ? juce::Colour(Theme::textWhite)
                                                          : juce::Colour(Theme::textDim);
    }

    // No separate SP view: Phrase shows and writes star power, solo and fill as three lanes, so
    // an SP-only view was one more place to be in for no extra reach.
    // Order is the step order. enabledByDefault is what the stepper walks: a view listed but off
    // still holds its place here, so turning it on is this flag rather than working out where in
    // the order it belonged. Sections and Tempo are both listed and both waiting on their views.
    struct ViewItem { EditorView view; const char* label; bool enabledByDefault; };
    static constexpr ViewItem VIEWS[] = {
        { EditorView::Notes,     "Notes",    true  },
        { EditorView::Phrase,    "Phrase",   true  },
        { EditorView::Sections,  "Sections", false },
        { EditorView::Tempo,     "Tempo",    false },
    };

    std::array<bool, std::size(VIEWS)> viewEnabled = [] {
        std::array<bool, std::size(VIEWS)> a{};
        for (size_t i = 0; i < std::size(VIEWS); ++i) a[i] = VIEWS[i].enabledByDefault;
        return a;
    }();

    // Index of the step `delta` away from the current view, skipping the disabled ones and
    // stopping at the ends rather than wrapping: with two views enabled, wrapping would make
    // both arrows do the same thing.
    // Switch views, but only when that view is enabled and is not already the one showing.
    void selectView(EditorView v)
    {
        for (size_t i = 0; i < std::size(VIEWS); ++i)
            if (VIEWS[i].view == v && !viewEnabled[i]) return;

        if (onViewChanged && v != interactionController.view())
            onViewChanged(v);
    }

    SegmentedButtons modeButtons;
    SegmentedButtons phraseKindButtons;
    PillToggle       barToggle      {"BAR"};
    // Drawing lanes is a third thing a drag can write, alongside notes and phrases, so it sits
    // with the note types rather than among the phrase kinds.
    PillToggle       laneToggle     {"LANE"};

    PillToggle       snapToggle     {"Snap"};
    DropdownButton   divisionDropdown;
    DropdownButton   tupletDropdown;

    // Values and labels from one table each, so a dropdown's index cannot mean one division here
    // and another where it is applied. These used to live in ToolbarComponent's handlers, which
    // is why stepping had to search for the current value before it could move off it.
    static constexpr int STEP_DIVISIONS[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };
    static constexpr int TUPLETS[] = { 0, 3, 5, 7 };

    template <size_t N>
    static int indexOf(const int (&table)[N], int value)
    {
        for (size_t i = 0; i < N; ++i) if (table[i] == value) return (int)i;
        return -1;
    }

    SegmentedButtons guitarForceButtons;
    SegmentedButtons drumDynamicButtons;
    PillToggle       cymbalToggle   {"Cym"};
    PillToggle       flamToggle     {"Flam"};

    std::vector<std::unique_ptr<HoverHelpZone>> hoverZones;
    HoverHelpZone* barHelpZone = nullptr;

    int labelH = 0;
    juce::Rectangle<int> labelGroupMode, labelGroupNote, labelGroupPhrase, labelGroupGrid;

    // Soft to loud, the same order as the home-row slots in ModifierSlots.h.
    static constexpr DrumDynamic DYNAMIC_ORDER[] = { DrumDynamic::Ghost, DrumDynamic::Normal,
                                                     DrumDynamic::Accent };
    static constexpr float AUTO_GLYPH_SCALE = 0.62f;   // of the segment height

    static constexpr int MODE_SQUARES = 2;
    static constexpr int SNAP_SQUARES = 1;
    static constexpr size_t guitarForceItems = 4;
    static constexpr size_t drumDynamicItems = 3;

    // Room for the widest value a control can show, whatever is showing now, so it does not
    // change width as the value changes.
    static int textWidthFor(const juce::StringArray& values)
    {
        float w = 0.0f;
        for (const auto& s : values) w = juce::jmax(w, Theme::controlFont.getStringWidthFloat(s));
        return juce::roundToInt(w + Theme::controlFont.getHeight() * STEPPER_TEXT_PAD);
    }

    static int divisionTextWidth()
    {
        juce::StringArray v;
        for (int d : STEP_DIVISIONS) v.add("1/" + juce::String(d));
        return textWidthFor(v);
    }

    static int tupletTextWidth()
    {
        juce::StringArray v;
        for (int t : TUPLETS) v.add(t == 0 ? "Off" : juce::String(t));
        return textWidthFor(v);
    }

    // One phrase segment, wide enough for the longest label any instrument uses. Measured across
    // every kind rather than the current part's, so a segment is the same width on all of them:
    // sizing to the part meant the labels squeezed and the whole group's width moved with it.
    static int phraseItemWidth()
    {
        juce::StringArray v;
        for (auto k : { PhraseMath::Kind::StarPower, PhraseMath::Kind::Solo,
                        PhraseMath::Kind::Fill, PhraseMath::Kind::Bre })
            v.add(PhraseMath::phraseShortLabel(k));
        return textWidthFor(v);
    }

    // The phrase slot holds the most kinds any instrument has, so switching parts changes which
    // segments are in it and nothing after it on the row moves. Same trick the note slot uses.
    static constexpr int PHRASE_MAX_KINDS = 4;
    static constexpr float STEPPER_TEXT_PAD = 0.3f;        // of the font height
    static constexpr float STEPPER_ARROW_SQUARES = 0.6f;   // each arrow zone, in squares

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
