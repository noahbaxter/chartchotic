#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Labeled value selector: Label  [< value >]
// Supports mouse wheel and click on arrows to step through values.
// The label portion takes ~42% width, value area takes the rest.
class ValueStepper : public juce::Component
{
public:
    ValueStepper(const juce::String& labelText) : name(labelText) {}

    void setDisplayValue(const juce::String& val) { displayValue = val; repaint(); }
    const juce::String& getDisplayValue() const { return displayValue; }

    std::function<void(int delta)> onStep;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        int nameW = (int)(bounds.getWidth() * labelRatio);
        bool hovering = isMouseOver();
        float h = (float)getHeight();
        float scaledFontSize = h * 0.393f;
        float scaledArrowW = h * 0.571f;
        float scaledArrowFont = h * 0.321f;

        // Label
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(scaledFontSize));
        if (labelOnRight)
            g.drawText(name, bounds.getWidth() - nameW, 0, nameW, getHeight(), juce::Justification::centredRight);
        else
            g.drawText(name, 0, 0, nameW, getHeight(), juce::Justification::centredLeft);

        // Value pill
        auto valueRect = (labelOnRight ? bounds.withRight(bounds.getWidth() - nameW)
                                       : bounds.withLeft(nameW)).toFloat().reduced(1.0f);
        auto cornerSize = valueRect.getHeight() * Theme::pillRadius;

        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(valueRect, cornerSize);

        g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.5f)
                             : juce::Colour(Theme::textDim).withAlpha(0.3f));
        g.drawRoundedRectangle(valueRect, cornerSize, 1.0f);

        // Arrows
        g.setColour(juce::Colour(Theme::coral).withAlpha(hovering ? 1.0f : 0.7f));
        g.setFont(juce::Font(scaledArrowFont));

        auto leftArrow = valueRect.withWidth(scaledArrowW);
        auto rightArrow = valueRect.withLeft(valueRect.getRight() - scaledArrowW);
        g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), leftArrow, juce::Justification::centred);
        g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), rightArrow, juce::Justification::centred);

        // Value text
        auto textRect = valueRect.withLeft(valueRect.getX() + scaledArrowW)
                                  .withRight(valueRect.getRight() - scaledArrowW);
        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::getUIFont(scaledFontSize));
        g.drawText(displayValue, textRect, juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition())) return;
        int nameW = (int)(getWidth() * labelRatio);
        float scaledArrowW = (float)getHeight() * 0.571f;

        int pillLeft = labelOnRight ? 0 : nameW;
        int pillRight = labelOnRight ? (getWidth() - nameW) : getWidth();

        if (e.x >= pillLeft && e.x < pillLeft + scaledArrowW)
        {
            if (onStep) onStep(-1);
        }
        else if (e.x > pillRight - scaledArrowW && e.x <= pillRight)
        {
            if (onStep) onStep(1);
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        int delta = (wheel.deltaY > 0) ? 1 : -1;
        if (onStep) onStep(delta);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        repaint();
        if (tooltip.isNotEmpty())
            showTooltip();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        repaint();
        hideTooltip();
    }

    void setLabelRatio(float ratio) { labelRatio = ratio; }
    void setLabelOnRight(bool right) { labelOnRight = right; }
    void setTooltip(const juce::String& text) { tooltip = text; }

private:
    juce::String name;
    juce::String displayValue;
    juce::String tooltip;
    float labelRatio = 0.42f;
    bool labelOnRight = false;

    struct TooltipLabel : public juce::Component
    {
        juce::String text;
        void paint(juce::Graphics& g) override
        {
            float h = (float)getHeight();
            g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.92f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);
            g.setColour(juce::Colour(Theme::textDim));
            g.setFont(Theme::getUIFont(h * 0.6f));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
        }
    };

    std::unique_ptr<TooltipLabel> tooltipLabel;

    void showTooltip()
    {
        auto* topLevel = getTopLevelComponent();
        if (!topLevel) return;

        tooltipLabel = std::make_unique<TooltipLabel>();
        tooltipLabel->text = tooltip;
        tooltipLabel->setAlwaysOnTop(true);

        float h = (float)getHeight();
        int tipH = juce::roundToInt(h * 0.6f);
        int tipW = juce::roundToInt(Theme::getUIFont(tipH * 0.6f).getStringWidthFloat(tooltip)) + tipH;

        auto pos = topLevel->getLocalPoint(this, juce::Point<int>(getWidth() / 2, getHeight()));
        int tipX = pos.x - tipW / 2;
        int tipY = pos.y + 4;
        tipX = juce::jmax(0, juce::jmin(tipX, topLevel->getWidth() - tipW));

        tooltipLabel->setBounds(tipX, tipY, tipW, tipH);
        topLevel->addAndMakeVisible(tooltipLabel.get());
    }

    void hideTooltip()
    {
        tooltipLabel.reset();
    }
};
