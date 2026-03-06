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
        g.setFont(juce::Font(scaledFontSize));
        g.drawText(name, 0, 0, nameW, getHeight(), juce::Justification::centredLeft);

        // Value pill
        auto valueRect = bounds.withLeft(nameW).toFloat().reduced(1.0f);
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
        g.setFont(juce::Font(scaledFontSize));
        g.drawText(displayValue, textRect, juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition())) return;
        int nameW = (int)(getWidth() * labelRatio);
        float scaledArrowW = (float)getHeight() * 0.571f;

        if (e.x >= nameW && e.x < nameW + scaledArrowW)
        {
            if (onStep) onStep(-1);
        }
        else if (e.x > getWidth() - scaledArrowW)
        {
            if (onStep) onStep(1);
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        int delta = (wheel.deltaY > 0) ? 1 : -1;
        if (onStep) onStep(delta);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void setLabelRatio(float ratio) { labelRatio = ratio; }

private:
    juce::String name;
    juce::String displayValue;
    float labelRatio = 0.42f;
};
