#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../HelpTypes.h"
#include "PillToggle.h"
#include "SegmentedButtons.h"
#include "DropdownButton.h"
#include "ValueStepper.h"
#include "WriteModeIcon.h"
#include "../../Editor/InteractionController.h"
#include "../../Visual/Art/GemIconArt.h"

class WriteSubToolbar : public juce::Component
{
public:
    explicit WriteSubToolbar(InteractionController& ic) : interactionController(ic)
    {
        juce::StringArray viewLabels;
        for (const auto& v : VIEWS) viewLabels.add(v.label);
        viewButtons.setItems(viewLabels);
        addAndMakeVisible(viewButtons);

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

        snapToggle.setToggleState(ic.snapEnabled(), juce::dontSendNotification);
        snapToggle.setIcon(juce::Drawable::createFromImageData(
            BinaryData::icon_snap_magnet_svg, BinaryData::icon_snap_magnet_svgSize));
        addAndMakeVisible(snapToggle);

        divisionStepper.setLabelRatio(0.0f);
        addAndMakeVisible(divisionStepper);

        tupletStepper.setLabelRatio(0.0f);
        addAndMakeVisible(tupletStepper);

        guitarForceButtons.setItems({"Auto", "HOPO", "Strum", "Tap"});
        addChildComponent(guitarForceButtons);

        drumDynamicButtons.setItems({"Ghost", "Normal", "Accent"});
        addChildComponent(drumDynamicButtons);

        cymbalToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(cymbalToggle);

        flamToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(flamToggle);

        viewButtons.setCornerRadius(3.0f);
        for (auto* seg : { &modeButtons, &guitarForceButtons, &drumDynamicButtons })
            seg->setCornerRadius(3.0f);
        for (auto* pill : { &snapToggle, &barToggle, &cymbalToggle, &flamToggle })
            pill->setCornerRadius(3.0f);
        divisionStepper.setCornerRadius(3.0f);
        tupletStepper.setCornerRadius(3.0f);

        addHoverZone(viewButtons,        {},         "what every highway and the strip show, and what a click writes");
        addHoverZone(modeButtons,        {"Q"},      "draw places notes, edit selects & modifies");
        // Modifier hints run in the same order as the segments they sit under,
        // matching the home-row slots in ModifierSlots.h.
        barHelpZone = addHoverZone(barToggle, {"B"}, "only interact with open notes");
        addHoverZone(snapToggle,         {"E"},                    "lock note placement to grid");
        addHoverZone(divisionStepper,    {"[", "]", "⌥scroll"},    "grid spacing (1/4, 1/8, 1/16...)");
        addHoverZone(tupletStepper,      {"T", "⇧T"},              "tuplet on/off, shift cycles 3/5/7");
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
        barToggle.setArt([this](juce::Graphics& g, juce::Rectangle<float> r) {
            bool drums = isDrumLike(interactionController.activePart());
            return !gemIcons->drawImage(g, r, drums ? assetManager->getBarKickImage()
                                                    : assetManager->getBarOpenImage()).isEmpty();
        });
    }

    std::function<void(EditorView)> onViewChanged;
    std::function<void(SubMode)> onSubModeChanged;
    std::function<void(bool)> onSnapChanged;
    std::function<void(int delta)> onStepDivisionStep;
    std::function<void(int delta)> onTupletStep;
    std::function<void(bool)> onBarModeChanged;
    std::function<void(GuitarForce)> onGuitarForceChanged;
    std::function<void(DrumDynamic)> onDrumDynamicChanged;
    std::function<void(bool)> onCymbalModeChanged;
    std::function<void(bool)> onFlamModeChanged;

    std::function<void(const HelpText&)> onHoverHelp;
    std::function<void()> onHoverHelpClear;

    void wireCallbacks()
    {
        viewButtons.onSelectionChanged = [this](int idx) {
            if (onViewChanged && idx >= 0 && idx < (int)std::size(VIEWS))
                onViewChanged(VIEWS[idx].view);
        };

        modeButtons.onSelectionChanged = [this](int idx) {
            if (onSubModeChanged)
                onSubModeChanged(idx == 0 ? SubMode::Draw : SubMode::Edit);
        };

        snapToggle.onClick = [this]() {
            if (onSnapChanged) onSnapChanged(snapToggle.getToggleState());
        };

        divisionStepper.onStep = [this](int delta) {
            if (onStepDivisionStep) onStepDivisionStep(delta);
        };

        tupletStepper.onStep = [this](int delta) {
            if (onTupletStep) onTupletStep(delta);
        };

        barToggle.onClick = [this]() {
            if (onBarModeChanged) onBarModeChanged(barToggle.getToggleState());
        };

        guitarForceButtons.onSelectionChanged = [this](int idx) {
            static const GuitarForce forces[] = { GuitarForce::None, GuitarForce::Hopo,
                                                   GuitarForce::Strum, GuitarForce::Tap };
            if (onGuitarForceChanged) onGuitarForceChanged(forces[idx]);
        };

        drumDynamicButtons.onSelectionChanged = [this](int idx) {
            if (onDrumDynamicChanged) onDrumDynamicChanged(DYNAMIC_ORDER[idx]);
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
        bool drums = isDrumLike(interactionController.activePart());
        bool drawMode = interactionController.subMode() == SubMode::Draw;
        bool editMode = !drawMode;
        bool hasSel = editMode && interactionController.hasEditSelection();
        bool notesView = interactionController.view() == EditorView::Notes;
        juce::Colour modeAccent = drawMode ? juce::Colour(Theme::purple) : juce::Colour(Theme::blue);

        viewButtons.setAccentColour(modeAccent);
        for (int i = 0; i < (int)std::size(VIEWS); ++i)
            if (VIEWS[i].view == interactionController.view())
                viewButtons.setSelectedIndex(i);

        // Open/kick filtering only means anything while gems are what a click writes.
        barToggle.setDisabled(!notesView);

        if (barHelpZone)
            barHelpZone->setDescription({"B"}, drums ? "only interact with kick notes"
                                                     : "only interact with open notes");

        modeButtons.setAccentColour(modeAccent);
        snapToggle.setAccentColour(modeAccent);
        divisionStepper.setAccentColour(modeAccent);
        tupletStepper.setAccentColour(modeAccent);
        barToggle.setAccentColour(modeAccent);
        guitarForceButtons.setAccentColour(modeAccent);
        drumDynamicButtons.setAccentColour(modeAccent);
        cymbalToggle.setAccentColour(modeAccent);
        flamToggle.setAccentColour(modeAccent);

        int modeIdx = drawMode ? 0 : 1;
        modeButtons.setSelectedIndex(modeIdx);

        bool snapOn = interactionController.snapEnabled();
        snapToggle.setToggleState(snapOn);

        int div = interactionController.stepDivision();
        divisionStepper.setDisplayValue("1/" + juce::String(div));
        divisionStepper.setAtMin(div <= 1);
        divisionStepper.setAtMax(div >= 64);
        divisionStepper.setAlpha(snapOn ? 1.0f : 0.35f);
        divisionStepper.setInterceptsMouseClicks(snapOn, snapOn);

        int tuplet = interactionController.tuplet();
        tupletStepper.setDisplayValue(tuplet == 0 ? "Off" : juce::String(tuplet));
        tupletStepper.setAtMin(tuplet == 0);
        tupletStepper.setAtMax(tuplet == 7);
        tupletStepper.setAlpha(snapOn ? 1.0f : 0.35f);
        tupletStepper.setInterceptsMouseClicks(snapOn, snapOn);

        barToggle.setToggleState(interactionController.barMode());
        barToggle.setButtonText(drums ? "KICK" : "OPEN");

        // Cym and Flam share one pill slot; no kit has both.
        const auto* cfg = getAuthoringConfig(interactionController.activePart());
        bool showCym  = drums && cfg && cfg->hasCymbalToggle;
        bool showFlam = drums && cfg && cfg->hasFlamToggle;

        guitarForceButtons.setVisible(!drums);
        drumDynamicButtons.setVisible(drums);
        cymbalToggle.setVisible(showCym);
        flamToggle.setVisible(showFlam);

        bool modifiersEnabled = notesView && (drawMode || hasSel);

        if (drums)
        {
            drumDynamicButtons.setAlpha(modifiersEnabled ? 1.0f : 0.35f);
            drumDynamicButtons.setInterceptsMouseClicks(modifiersEnabled, modifiersEnabled);
            cymbalToggle.setDisabled(!modifiersEnabled);
            flamToggle.setDisabled(!modifiersEnabled);

            if (modifiersEnabled)
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

            if (modifiersEnabled)
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
            draw("GRID",      labelGroupGrid);
            draw("VIEW",      labelGroupView);
        }
    }

    void resized() override
    {
        int h = getHeight();
        if (h <= 0) return;

        bool drums = isDrumLike(interactionController.activePart());

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
        const float squares = MODE_SQUARES + (float)noteSquares + SUBTOOLBAR_BAR_SQUARES
                            + SNAP_SQUARES + SUBTOOLBAR_VIEW_SQUARES;
        const float gapSquares = SUBTOOLBAR_GROUP_GAP * 3.0f + SUBTOOLBAR_ITEM_GAP * 4.0f;

        // A stepper is two arrow zones that scale with the squares plus room for its value in
        // pixels, so the value text comes off the width first and the squares shrink into what
        // is left. A tall row in a narrow window would otherwise run off both ends.
        const float stepperSquares = STEPPER_ARROW_SQUARES * 2.0f;
        const float fitSide = ((float)getWidth() * SUBTOOLBAR_FIT_FRACTION - stepperTextWidth() * 2.0f)
                            / (squares + gapSquares + stepperSquares * 2.0f);
        const int side = juce::jmax(8, juce::jmin(juce::roundToInt(available * SUBTOOLBAR_HEIGHT_SCALE),
                                                  (int)fitSide));

        const float arrowZone = side * STEPPER_ARROW_SQUARES;
        divisionStepper.setArrowZone(arrowZone);
        tupletStepper.setArrowZone(arrowZone);
        const int stepW = juce::roundToInt(arrowZone * 2.0f) + stepperTextWidth();

        const int groupGap = juce::roundToInt(side * SUBTOOLBAR_GROUP_GAP);
        const int itemGap  = juce::jmax(2, juce::roundToInt(side * SUBTOOLBAR_ITEM_GAP));
        const int barW  = juce::roundToInt(side * SUBTOOLBAR_BAR_SQUARES);
        const int viewW = juce::roundToInt(side * SUBTOOLBAR_VIEW_SQUARES);

        const int modeW = side * MODE_SQUARES;
        const int noteSlotW = side * noteSquares + itemGap;
        const int noteW = noteSlotW + itemGap + barW;
        const int gridW = side * SNAP_SQUARES + itemGap * 2 + stepW * 2;
        const int totalW = modeW + noteW + gridW + viewW + groupGap * 3;

        const int controlY = labelH + padTop + (available - side) / 2;
        int x = (getWidth() - totalW) / 2;

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
        place(barToggle, barW);
        x += groupGap;

        labelGroupGrid = { x, 0, gridW, labelH };
        place(snapToggle, side * SNAP_SQUARES);
        x += itemGap;
        place(divisionStepper, stepW);
        x += itemGap;
        place(tupletStepper, stepW);
        x += groupGap;

        labelGroupView = { x, 0, viewW, labelH };
        place(viewButtons, viewW);
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

    struct ViewItem { EditorView view; const char* label; };
    static constexpr ViewItem VIEWS[] = {
        { EditorView::Notes,     "Notes" },
        { EditorView::StarPower, "SP" },
        { EditorView::Phrase,    "Phrase" },
        { EditorView::Sections,  "Sections" },
        { EditorView::Tempo,     "Tempo" },
    };

    DropdownButton   viewButtons;
    SegmentedButtons modeButtons;
    PillToggle       barToggle      {"BAR"};

    PillToggle       snapToggle     {"Snap"};
    ValueStepper     divisionStepper{"", ""};
    ValueStepper     tupletStepper  {"", ""};

    SegmentedButtons guitarForceButtons;
    SegmentedButtons drumDynamicButtons;
    PillToggle       cymbalToggle   {"Cym"};
    PillToggle       flamToggle     {"Flam"};

    std::vector<std::unique_ptr<HoverHelpZone>> hoverZones;
    HoverHelpZone* barHelpZone = nullptr;

    int labelH = 0;
    juce::Rectangle<int> labelGroupMode, labelGroupNote, labelGroupGrid, labelGroupView;

    // Soft to loud, the same order as the home-row slots in ModifierSlots.h.
    static constexpr DrumDynamic DYNAMIC_ORDER[] = { DrumDynamic::Ghost, DrumDynamic::Normal,
                                                     DrumDynamic::Accent };
    static constexpr float AUTO_GLYPH_SCALE = 0.62f;   // of the segment height

    static constexpr int MODE_SQUARES = 2;
    static constexpr int SNAP_SQUARES = 1;
    static constexpr size_t guitarForceItems = 4;
    static constexpr size_t drumDynamicItems = 3;

    // Room for the widest division ("1/64") between the arrows, whatever is showing now, so
    // the stepper does not change width as you step.
    static int stepperTextWidth()
    {
        return juce::roundToInt(Theme::controlFont.getStringWidthFloat("1/64")
                                + Theme::controlFont.getHeight() * STEPPER_TEXT_PAD);
    }
    static constexpr float STEPPER_TEXT_PAD = 0.3f;        // of the font height
    static constexpr float STEPPER_ARROW_SQUARES = 0.45f;  // each arrow zone, in squares

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
