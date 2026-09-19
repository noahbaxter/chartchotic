#pragma once

#include <JuceHeader.h>
#include <optional>
#include "Theme.h"
#include "ControlConstants.h"
#include "LookAndFeel/ChartchoticLookAndFeel.h"
#include "../Visual/Utils/PaletteLanes.h"
#include "../Visual/Utils/PartPalettes.h"

// One part's colours. A row of swatches for the lanes that part actually has, a colour wheel
// editing whichever swatch is selected, and the presets as one-click answers.
//
// Its own window at a fixed size, for the same reason the confirm prompt is: a colour wheel
// squeezed into a plugin scaled down inside a DAW is not a colour wheel.
class PalettePicker : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener
{
public:
    // Called after every change, so the highway repaints while the wheel is being dragged
    // rather than when the window shuts.
    std::function<void()> onChanged;

    PalettePicker()
    {
        setWantsKeyboardFocus(true);
        setVisible(false);
        setSize(panelW, panelH);

        selector = std::make_unique<juce::ColourSelector>(
            juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders);

        // JUCE's own chrome inside our panel reads as a different application's dialog that
        // happened to open here. The app's look and feel is already written; the selector
        // just was not being told to use it.
        selector->setLookAndFeel(&lookAndFeel);
        selector->setColour(juce::ColourSelector::backgroundColourId, juce::Colour(Theme::panelNeutralBg));
        selector->setColour(juce::ColourSelector::labelTextColourId, juce::Colour(Theme::textDim));

        selector->addChangeListener(this);
        addAndMakeVisible(*selector);
    }

    ~PalettePicker() override
    {
        // Every child pointed at the look and feel has to let go before it dies with us.
        if (selector != nullptr) selector->setLookAndFeel(nullptr);
        for (auto& box : hexBoxes) box->setLookAndFeel(nullptr);
        paletteBox.setLookAndFeel(nullptr);
        removeFromDesktop();
    }

    void open(Part p, juce::Component* owner)
    {
        part = p;
        lanes = PaletteLanes::forPart(part);
        if (lanes.empty()) return;

        working = PartPalettes::global().forPart(part);
        selected = 0;

        if (!isOnDesktop())
            addToDesktop(juce::ComponentPeer::windowIsTemporary
                         | juce::ComponentPeer::windowHasDropShadow);

        const auto over = owner != nullptr
                        ? owner->getScreenBounds()
                        : juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setBounds(juce::Rectangle<int>(panelW, panelH).withCentre(over.getCentre()));

        setAlwaysOnTop(true);
        setVisible(true);
        toFront(true);
        grabKeyboardFocus();

        rebuildHexBoxes();
        pushSelectedToSelector();
        refreshHexBoxes();
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto panel = getLocalBounds().reduced(1).toFloat();
        g.setColour(juce::Colour(Theme::panelNeutralBg));
        g.fillRoundedRectangle(panel, corner);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(outlineAlpha));
        g.drawRoundedRectangle(panel, corner, 1.0f);

        // A bar to grab. The window has no host chrome of its own, so without one there is
        // nothing to move it by, and a colour picker you cannot move covers the thing you are
        // picking colours for.
        const auto bar = titleBarBounds();
        g.setColour(juce::Colour(Theme::darkBgLighter));
        g.fillRect(bar);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.25f));
        g.drawLine((float)bar.getX(), (float)bar.getBottom(),
                   (float)bar.getRight(), (float)bar.getBottom(), 1.0f);

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(juce::Font(juce::FontOptions().withHeight(titleFontH).withStyle("Bold")));
        g.drawText(getPartDisplayName(part) + " colours", bar.reduced(pad, 0),
                   juce::Justification::centredLeft, false);

        paintCloseButton(g);

        paintSwatches(g);
        paintButton(g, resetBounds(), "Reset", hovered == Hit::Reset);
        paintButton(g, doneBounds(),  "Done",  hovered == Hit::Done);

        // Says the drop will land, and says it in the app's own accent rather than leaving the
        // OS cursor to be the only sign anything is happening.
        if (dropTarget)
        {
            g.setColour(juce::Colour(Theme::coral));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), corner, 2.0f);
        }
    }

    void resized() override
    {
        if (selector != nullptr) selector->setBounds(wheelBounds());

        for (int i = 0; i < (int)hexBoxes.size(); ++i)
        {
            const auto swatch = swatchBounds(i);
            hexBoxes[(size_t)i]->setBounds(swatch.getX(), swatch.getBottom() + labelH,
                                           swatch.getWidth(), hexH);
        }

        paletteBox.setBounds(paletteBoxBounds());
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto now = hitTest(e.getPosition());
        if (now.kind != hovered || now.index != hoveredIndex)
        {
            hovered = now.kind;
            hoveredIndex = now.index;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (closeBounds().contains(e.getPosition())) { close(); return; }

        if (titleBarBounds().contains(e.getPosition()))
        {
            dragger.startDraggingComponent(this, e);
            dragging = true;
            return;
        }

        const auto hit = hitTest(e.getPosition());

        switch (hit.kind)
        {
        case Hit::Swatch:
            selected = hit.index;
            pushSelectedToSelector();
            repaint();
            break;

        case Hit::Reset:
            working = LaneColours::defaultPalette();
            commit();
            pushSelectedToSelector();
            refreshHexBoxes();
            repaint();
            break;

        case Hit::Done:
            close();
            break;

        default: break;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragging) dragger.dragComponent(this, e, nullptr);
    }

    // Someone else's colours, dropped in. The palette is one small file in the app's settings
    // folder, so sharing a scheme is sending that file and dropping it on this window.
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (f.endsWithIgnoreCase(".xml")) return true;
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override { dropTarget = true;  repaint(); }
    void fileDragExit (const juce::StringArray&)                   override { dropTarget = false; repaint(); }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dropTarget = false;

        for (const auto& f : files)
        {
            if (!f.endsWithIgnoreCase(".xml")) continue;
            if (!PartPalettes::global().loadFrom(juce::File(f))) continue;

            PartPalettes::global().save();   // adopted, so it is now this machine's palette
            working = PartPalettes::global().forPart(part);
            pushSelectedToSelector();
            refreshHexBoxes();
            if (onChanged) onChanged();
            break;
        }

        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { close(); return true; }
        return false;
    }

private:
    enum class Hit { None, Swatch, Reset, Done, Close };
    struct HitResult { Hit kind = Hit::None; int index = -1; };

    static constexpr int panelW = 560;
    static constexpr int panelH = 560;
    static constexpr int pad = 16;
    static constexpr int titleH = 34;
    static constexpr int swatchH = 54;
    static constexpr int labelH = 16;
    static constexpr int hexH = 20;
    static constexpr int buttonH = 38;
    static constexpr int buttonW = 110;
    static constexpr float titleFontH = 21.0f;
    static constexpr float labelFontH = 11.0f;
    static constexpr float bodyFontH = 14.0f;
    static constexpr float corner = 6.0f;
    static constexpr float outlineAlpha = 0.5f;
    // How far the pad gradient's top sits under the colour that was picked. The stock pairs
    // run about this far apart, so a picked colour gets a gradient of the same character
    // rather than a flat slab.
    static constexpr float darkBrightness = 0.62f;

    juce::Rectangle<int> titleBarBounds() const
    { return getLocalBounds().removeFromTop(titleH); }

    juce::Rectangle<int> closeBounds() const
    {
        const auto bar = titleBarBounds();
        return juce::Rectangle<int>(bar.getRight() - titleH, bar.getY(), titleH, titleH).reduced(titleH / 4);
    }

    void paintCloseButton(juce::Graphics& g)
    {
        const auto x = closeBounds().toFloat().reduced(3.0f);
        g.setColour(juce::Colour(hovered == Hit::Close ? Theme::coral : Theme::textDim));
        g.drawLine(x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.4f);
        g.drawLine(x.getRight(), x.getY(), x.getX(), x.getBottom(), 1.4f);
    }

    juce::Rectangle<int> swatchRow() const
    {
        auto r = getLocalBounds().reduced(pad * 2, pad);
        r.removeFromTop(titleH + pad / 2);
        return r.removeFromTop(swatchH + labelH);
    }

    juce::Rectangle<int> swatchBounds(int index) const
    {
        const auto row = swatchRow();
        const int n = juce::jmax(1, (int)lanes.size());
        const int w = row.getWidth() / n;
        return { row.getX() + index * w, row.getY(), w - 4, swatchH };
    }

    juce::Rectangle<int> paletteBoxBounds() const
    {
        auto r = getLocalBounds().reduced(pad * 2, pad);
        r.removeFromBottom(buttonH + pad);
        return r.removeFromBottom(hexH);
    }

    juce::Rectangle<int> wheelBounds() const
    {
        auto r = getLocalBounds().reduced(pad * 2, pad);
        r.removeFromTop(titleH + pad / 2 + swatchH + labelH + hexH + pad);
        r.removeFromBottom(buttonH + pad + hexH + pad);
        return r;
    }

    juce::Rectangle<int> doneBounds() const
    {
        auto r = getLocalBounds().reduced(pad * 2, pad);
        return { r.getRight() - buttonW, r.getBottom() - buttonH, buttonW, buttonH };
    }

    juce::Rectangle<int> resetBounds() const { return doneBounds().translated(-(buttonW + pad), 0); }

    HitResult hitTest(juce::Point<int> p) const
    {
        for (int i = 0; i < (int)lanes.size(); ++i)
            if (swatchBounds(i).contains(p)) return { Hit::Swatch, i };

        if (closeBounds().contains(p)) return { Hit::Close, -1 };
        if (resetBounds().contains(p)) return { Hit::Reset, -1 };
        if (doneBounds().contains(p))  return { Hit::Done, -1 };
        return {};
    }

    void paintSwatches(juce::Graphics& g)
    {
        for (int i = 0; i < (int)lanes.size(); ++i)
        {
            auto box = swatchBounds(i);
            const auto lane = PaletteLanes::colourOf(working, lanes[(size_t)i]);

            g.setColour(LaneColours::bright(lane));
            g.fillRoundedRectangle(box.toFloat(), corner);

            // The pad gradient's dark end as a strip along the bottom, so the swatch shows
            // what will actually be drawn rather than one flat colour.
            g.setColour(LaneColours::dark(lane));
            g.fillRect(box.removeFromBottom(box.getHeight() / 4).reduced(2, 0));

            const auto outline = swatchBounds(i).toFloat();
            const bool live = i == selected;
            g.setColour(juce::Colour(live ? Theme::coral : Theme::textDim)
                            .withAlpha(live ? 1.0f : (i == hoveredIndex && hovered == Hit::Swatch ? 0.8f : 0.3f)));
            g.drawRoundedRectangle(outline, corner, live ? 2.0f : 1.0f);

            g.setColour(juce::Colour(Theme::textDim));
            g.setFont(juce::FontOptions().withHeight(labelFontH));
            g.drawText(lanes[(size_t)i].label,
                       juce::Rectangle<int>(outline.getX(), outline.getBottom(), outline.getWidth(), labelH)
                           .toNearestInt(),
                       juce::Justification::centred, false);
        }
    }

    void paintButton(juce::Graphics& g, juce::Rectangle<int> box, const juce::String& label, bool isHovered)
    {
        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(isHovered ? 1.0f : 0.6f));
        g.fillRoundedRectangle(box.toFloat(), corner);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.4f));
        g.drawRoundedRectangle(box.toFloat(), corner, 1.0f);

        g.setColour(juce::Colour(isHovered ? Theme::textWhite : Theme::textDim));
        g.setFont(juce::FontOptions().withHeight(bodyFontH));
        g.drawText(label, box, juce::Justification::centred, false);
    }

    // One box per lane holding that lane's colour as hex, and one holding the whole part as
    // a comma-separated run of them. Both are plain text on purpose: a colour you can select
    // and copy is a colour you can paste into another part, or into a message to someone.
    void rebuildHexBoxes()
    {
        for (auto& box : hexBoxes) removeChildComponent(box.get());
        hexBoxes.clear();

        for (int i = 0; i < (int)lanes.size(); ++i)
        {
            auto box = std::make_unique<juce::TextEditor>();
            styleBox(*box);
            box->onReturnKey = [this, i] { applyHexBox(i); };
            box->onFocusLost = [this, i] { applyHexBox(i); };
            addAndMakeVisible(*box);
            hexBoxes.push_back(std::move(box));
        }

        addAndMakeVisible(paletteBox);
        styleBox(paletteBox);
        paletteBox.onReturnKey = [this] { applyPaletteBox(); };
        paletteBox.onFocusLost = [this] { applyPaletteBox(); };
    }

    void styleBox(juce::TextEditor& box)
    {
        box.setLookAndFeel(&lookAndFeel);
        box.setFont(juce::FontOptions().withHeight(labelFontH + 1.0f));
        box.setJustification(juce::Justification::centred);
        // One click takes the whole value. These boxes exist to be copied out of and pasted
        // into, and a hex that needs a double click or a drag first is a hex nobody copies.
        box.setSelectAllWhenFocused(true);
        box.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBg));
        box.setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::textDim).withAlpha(0.3f));
    }

    static juce::String hexOf(const LaneColours::Lane& lane)
    {
        return LaneColours::bright(lane).toDisplayString(false).toUpperCase();
    }

    void refreshHexBoxes()
    {
        juce::StringArray all;
        for (int i = 0; i < (int)lanes.size(); ++i)
        {
            const auto text = hexOf(PaletteLanes::colourOf(working, lanes[(size_t)i]));
            all.add(text);
            if (i < (int)hexBoxes.size())
                hexBoxes[(size_t)i]->setText(text, juce::dontSendNotification);
        }
        paletteBox.setText(all.joinIntoString(" "), juce::dontSendNotification);
    }

    // A lane takes any of the shapes a colour gets pasted in: bare hex, #-prefixed, with or
    // without an alpha byte. Anything else is rejected by putting the old value back, which
    // says "not understood" without a dialog about it.
    void applyHexBox(int index)
    {
        if (index < 0 || index >= (int)lanes.size()) return;

        if (auto colour = parseHex(hexBoxes[(size_t)index]->getText()))
        {
            setLaneColour(index, *colour);
            commit();
            repaint();
        }
        refreshHexBoxes();
        pushSelectedToSelector();
    }

    // The whole part in one field, so a scheme can be copied from one part and pasted into
    // another. Short runs are allowed: they fill from the left and leave the rest alone.
    void applyPaletteBox()
    {
        const auto tokens = juce::StringArray::fromTokens(paletteBox.getText(), " ,;\t\n", "");
        int lane = 0;

        for (const auto& token : tokens)
        {
            if (token.trim().isEmpty()) continue;
            if (lane >= (int)lanes.size()) break;
            if (auto colour = parseHex(token)) setLaneColour(lane, *colour);
            ++lane;
        }

        commit();
        refreshHexBoxes();
        pushSelectedToSelector();
        repaint();
    }

    static std::optional<juce::Colour> parseHex(const juce::String& text)
    {
        auto digits = text.trim().removeCharacters("#").toUpperCase();
        if (digits.length() == 8) digits = digits.substring(2);       // drop a pasted alpha byte
        if (digits.length() != 6) return {};
        if (!digits.containsOnly("0123456789ABCDEF")) return {};

        return juce::Colour((juce::uint32)(0xFF000000u | (juce::uint32)digits.getHexValue32()));
    }

    void setLaneColour(int index, juce::Colour bright)
    {
        PaletteLanes::setColourOf(working, lanes[(size_t)index],
                                  { bright.withMultipliedBrightness(darkBrightness).getARGB(),
                                    bright.getARGB() });
    }

    void pushSelectedToSelector()
    {
        if (selector == nullptr || selected < 0 || selected >= (int)lanes.size()) return;

        // Setting the selector fires its own change message, which would write straight back
        // over the palette with what we just handed it. Harmless in value but it would mark a
        // part customised for merely being looked at.
        juce::ScopedValueSetter<bool> guard(loading, true);
        selector->setCurrentColour(LaneColours::bright(PaletteLanes::colourOf(working, lanes[(size_t)selected])),
                                   juce::dontSendNotification);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (loading || selector == nullptr || selected < 0 || selected >= (int)lanes.size()) return;

        setLaneColour(selected, selector->getCurrentColour());
        commit();
        refreshHexBoxes();
        repaint();
    }

    // Saved on every change rather than on Done: there is no Cancel here, and a colour scheme
    // people spent ten minutes on should not depend on how they closed the window.
    void commit()
    {
        PartPalettes::global().set(part, working);
        PartPalettes::global().save();
        if (onChanged) onChanged();
    }

    void close()
    {
        setVisible(false);
        removeFromDesktop();
        hovered = Hit::None;
        hoveredIndex = -1;
    }

    Part part = Part::GUITAR;
    std::vector<PaletteLanes::Lane> lanes;
    std::vector<std::unique_ptr<juce::TextEditor>> hexBoxes;
    juce::TextEditor paletteBox;
    ChartchoticLookAndFeel lookAndFeel;
    juce::ComponentDragger dragger;
    bool dragging = false;
    LaneColours::Palette working = LaneColours::defaultPalette();
    int selected = 0;
    Hit hovered = Hit::None;
    int hoveredIndex = -1;
    bool loading = false;
    bool dropTarget = false;   // a palette file is hovering over the window

    std::unique_ptr<juce::ColourSelector> selector;
};
