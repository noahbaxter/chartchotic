#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "PopupMenuButton.h"

// One-of-many picker that shows only the current choice, for a row too tight for every option
// to sit side by side. Click opens the app's own panel below it, the wheel steps through the
// items in place.
class DropdownButton : public juce::Component
{
public:
    ~DropdownButton() override { closePanel(); }

    void setItems(const juce::StringArray& labels) { items = labels; repaint(); }

    void setSelectedIndex(int index)
    {
        if (selectedIndex == index) return;
        selectedIndex = index;
        repaint();
    }

    int getSelectedIndex() const { return selectedIndex; }

    std::function<void(int index)> onSelectionChanged;

    // A dropdown you click or scroll on does not need to advertise the fact with a chevron, and
    // on a short value the arrow column costs more than the value it sits next to.
    void setArrowVisible(bool v) { arrowVisible = v; repaint(); }

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void setCornerRadius(float r) { cornerRadius = r; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto cornerSize = (cornerRadius >= 0.0f) ? cornerRadius : Theme::pillCorner;
        bool lit = isMouseOver() || panel != nullptr;

        g.setColour(juce::Colour(lit ? Theme::darkBgHover : Theme::darkBg));
        g.fillRoundedRectangle(bounds, cornerSize);
        g.setColour(accent.withAlpha(lit ? 0.8f : 0.5f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        const float arrowW = arrowVisible ? bounds.getHeight() * ARROW_W_RATIO : 0.0f;
        auto textArea = bounds.withTrimmedRight(arrowW).toNearestInt();

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::controlFont);
        if (juce::isPositiveAndBelow(selectedIndex, items.size()))
            g.drawText(items[selectedIndex], textArea, juce::Justification::centred);

        if (arrowVisible)
        {
            const float cx = bounds.getRight() - arrowW * 0.5f;
            const float cy = bounds.getCentreY();
            const float s = arrowW * ARROW_SIZE_RATIO;
            juce::Path arrow;
            arrow.addTriangle(cx - s, cy - s * 0.5f, cx + s, cy - s * 0.5f, cx, cy + s * 0.5f);
            g.setColour(juce::Colour(Theme::textDim));
            g.fillPath(arrow);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition()) || items.isEmpty()) return;
        if (panel != nullptr) closePanel();
        else                  openPanel();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (items.isEmpty()) return;
        int delta = (wheel.deltaY > 0.0f) ? 1 : (wheel.deltaY < 0.0f) ? -1 : 0;
        if (delta != 0) choose(juce::jlimit(0, items.size() - 1, selectedIndex + delta));
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { repaint(); }

    // Public so a caller sizing a dropdown reserves exactly the column the paint trims off. Held
    // separately, the two drifted and the text ellipsised inside a button wide enough for it.
    static constexpr float ARROW_W_RATIO = 0.5f;     // of the height

private:
    bool arrowVisible = true;
    static constexpr float ARROW_SIZE_RATIO = 0.25f; // of the arrow column
    static constexpr float PANEL_PAD_RATIO = 0.18f;  // of a row, around the list
    static constexpr int PANEL_GAP = 4;              // px between button and panel

    class Row : public juce::Component
    {
    public:
        Row(DropdownButton& o, int i) : owner(o), index(i) {}

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            bool selected = index == owner.selectedIndex;

            if (selected || isMouseOver())
            {
                g.setColour(selected ? owner.accent : juce::Colour(Theme::darkBgHover));
                g.fillRoundedRectangle(r, Theme::pillCorner);
            }

            g.setColour(juce::Colour(selected || isMouseOver() ? Theme::textWhite : Theme::textDim));
            g.setFont(Theme::controlFont);
            g.drawText(owner.items[index], r.toNearestInt(), juce::Justification::centred);
        }

        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { repaint(); }

        // Deferred, because choosing closes the panel and that deletes this row.
        void mouseUp(const juce::MouseEvent& e) override
        {
            if (!getLocalBounds().contains(e.getPosition())) return;
            juce::Component::SafePointer<DropdownButton> safe(&owner);
            int chosen = index;
            juce::MessageManager::callAsync([safe, chosen] {
                if (safe == nullptr) return;
                safe->closePanel();
                safe->choose(chosen);
            });
        }

    private:
        DropdownButton& owner;
        int index;
    };

    void openPanel()
    {
        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        panel = std::make_unique<PopupPanel>();
        panel->setOutlined(false);

        const int rowH = getHeight();
        const int pad = juce::roundToInt(rowH * PANEL_PAD_RATIO);
        int textW = 0;
        for (const auto& item : items)
            textW = juce::jmax(textW, juce::roundToInt(Theme::controlFont.getStringWidthFloat(item)));

        const int w = juce::jmax(getWidth(), textW + rowH + pad * 2);
        panel->setSize(w, items.size() * rowH + pad * 2);

        for (int i = 0; i < items.size(); ++i)
        {
            auto row = std::make_unique<Row>(*this, i);
            row->setBounds(pad, pad + i * rowH, w - pad * 2, rowH);
            panel->getContent().addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }

        // Under the button, held inside the window.
        auto below = topLevel->getLocalPoint(this, juce::Point<int>(0, getHeight() + PANEL_GAP));
        int x = juce::jlimit(0, juce::jmax(0, topLevel->getWidth() - w), below.x);
        panel->setTopLeftPosition(x, below.y);

        topLevel->addAndMakeVisible(panel.get());
        panel->onContentLaidOut();

        panel->onOutsideMouseDown = [this](const juce::MouseEvent& e) {
            // The button's own click toggles it shut on mouse up.
            if (e.eventComponent == this) return;
            juce::Component::SafePointer<DropdownButton> safe(this);
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->closePanel(); });
        };
        overlayParent = topLevel;
        overlayParent->addMouseListener(panel.get(), true);
        repaint();
    }

    void closePanel()
    {
        if (panel == nullptr) return;
        if (overlayParent != nullptr) overlayParent->removeMouseListener(panel.get());
        overlayParent = nullptr;
        rows.clear();
        panel.reset();
        repaint();
    }

    void choose(int index)
    {
        if (index == selectedIndex) return;
        selectedIndex = index;
        repaint();
        if (onSelectionChanged) onSelectionChanged(selectedIndex);
    }

    juce::StringArray items;
    int selectedIndex = 0;
    juce::Colour accent { Theme::coral };
    float cornerRadius = -1.0f;

    std::unique_ptr<PopupPanel> panel;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Component::SafePointer<juce::Component> overlayParent;
};
