#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Multi-line info tooltip that appears below a target component on hover.
// Renders a dark rounded card with word-wrapped text.
//
// Usage:
//   InfoTooltip tip;
//   tip.setText("Long description here...");
//   tip.attachTo(myComponent);          // auto show/hide on hover
//   // OR manually: tip.show(component); tip.hide();
class InfoTooltip
{
public:
    /**
        Where the card sits relative to what it explains.

        Left of the target suits a row of controls with room beside it. Below
        suits a checkbox in a line of them, where the card would otherwise
        cover the neighbour you are comparing it against.
    */
    enum class Placement { left, below };

    void setText(const juce::String& text) { tooltipText = text; }
    void setPlacement(Placement where) { placement = where; }

    // Attach to a component so the tooltip auto-shows on mouseEnter/mouseExit.
    // The target must outlive this InfoTooltip.
    void attachTo(juce::Component& target)
    {
        hoverTarget = &target;
        listener = std::make_unique<HoverListener>(*this);
        target.addMouseListener(listener.get(), false);
    }

    void detach()
    {
        if (hoverTarget && listener)
            hoverTarget->removeMouseListener(listener.get());
        listener.reset();
        hoverTarget = nullptr;
        hide();
    }

    void show(juce::Component& relativeTo)
    {
        if (tooltipText.isEmpty()) return;

        auto* topLevel = Theme::getOverlayParent(&relativeTo);
        if (!topLevel) return;

        card = std::make_unique<TooltipCard>();
        card->text = tooltipText;
        card->setAlwaysOnTop(true);
        card->setInterceptsMouseClicks(false, false);

        // Measure text to size the card
        int maxW = juce::jmin(280, topLevel->getWidth() - 20);
        float fontH = juce::jmax(10.0f, Theme::fontSize * 0.85f);
        auto font = Theme::getUIFont(fontH);

        int padX = juce::roundToInt(fontH * 0.8f);
        int padY = juce::roundToInt(fontH * 0.6f);
        int textW = maxW - padX * 2;

        juce::AttributedString as;
        as.append(tooltipText, font, juce::Colour(Theme::textDim));
        as.setWordWrap(juce::AttributedString::WordWrap::byWord);
        juce::TextLayout layout;
        layout.createLayout(as, (float)textW);

        int cardW = textW + padX * 2;
        int cardH = juce::roundToInt(layout.getHeight()) + padY * 2;

        auto pos = topLevel->getLocalPoint(&relativeTo, juce::Point<int>(0, 0));

        // Below: left edges aligned, so the card reads as belonging to the
        // control above it. Left: alongside, top edges aligned.
        int tipX = placement == Placement::below ? pos.x : pos.x - cardW - 4;
        int tipY = placement == Placement::below ? pos.y + relativeTo.getHeight() + 4
                                                 : pos.y;

        // Keep within bounds
        tipX = juce::jmax(4, juce::jmin(tipX, topLevel->getWidth() - cardW - 4));
        tipY = juce::jmin(tipY, topLevel->getHeight() - cardH - 4);

        card->font = font;
        card->padX = padX;
        card->padY = padY;
        card->setBounds(tipX, tipY, cardW, cardH);
        topLevel->addAndMakeVisible(card.get());
    }

    void hide()
    {
        card.reset();
    }

    ~InfoTooltip()
    {
        detach();
    }

private:
    juce::String tooltipText;
    Placement placement = Placement::left;
    juce::Component* hoverTarget = nullptr;

    struct TooltipCard : public juce::Component
    {
        juce::String text;
        juce::Font font;
        int padX = 8;
        int padY = 6;

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.95f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.2f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

            juce::AttributedString as;
            as.append(text, font, juce::Colour(Theme::textDim));
            as.setWordWrap(juce::AttributedString::WordWrap::byWord);
            juce::TextLayout layout;
            layout.createLayout(as, (float)(getWidth() - padX * 2));
            layout.draw(g, juce::Rectangle<float>(
                (float)padX, (float)padY,
                (float)(getWidth() - padX * 2),
                (float)(getHeight() - padY * 2)));
        }
    };

    std::unique_ptr<TooltipCard> card;

    struct HoverListener : public juce::MouseListener, public juce::Timer
    {
        InfoTooltip& owner;
        juce::Component* hoveredComp = nullptr;
        static constexpr int delayMs = 500;

        HoverListener(InfoTooltip& o) : owner(o) {}

        void mouseEnter(const juce::MouseEvent& e) override
        {
            hoveredComp = e.eventComponent;
            startTimer(delayMs);
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            stopTimer();
            hoveredComp = nullptr;
            owner.hide();
        }

        void timerCallback() override
        {
            stopTimer();
            if (hoveredComp)
                owner.show(*hoveredComp);
        }
    };

    std::unique_ptr<HoverListener> listener;
};
