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
    std::function<void()> onFolderClick;

    void setFolderIconVisible(bool show) { folderIconVisible = show; repaint(); }
    void setValueClickable(bool clickable) { valueClickOpensFolder = clickable; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        float h = (float)getHeight();

        int folderW = folderIconVisible ? (int)(h * 0.85f) : 0;
        int nameW = (int)(bounds.getWidth() * labelRatio);

        bool hovering = isMouseOver() && !folderHoverZone;

        // Label
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::controlFont);
        if (labelOnRight)
            g.drawText(name, bounds.getWidth() - nameW, 0, nameW, getHeight(), juce::Justification::centredRight);
        else
            g.drawText(name, 0, 0, nameW - folderW, getHeight(), juce::Justification::centredLeft);

        // Folder icon — at right edge of label area, just left of pill
        if (folderIconVisible)
        {
            bool folderHover = isMouseOver() && folderHoverZone;
            int iconX = labelOnRight ? (bounds.getWidth() - nameW - folderW) : (nameW - folderW);
            auto iconArea = juce::Rectangle<float>((float)iconX, 0.0f, (float)folderW, h).reduced(h * 0.2f);
            g.setColour(juce::Colour(Theme::coral).withAlpha(folderHover ? 1.0f : 0.45f));

            float ix = iconArea.getX(), iy = iconArea.getY();
            float iw = iconArea.getWidth(), ih = iconArea.getHeight();
            float tabW = iw * 0.4f, tabH = ih * 0.2f;
            juce::Path folder;
            folder.startNewSubPath(ix, iy + tabH);
            folder.lineTo(ix, iy + ih);
            folder.lineTo(ix + iw, iy + ih);
            folder.lineTo(ix + iw, iy + tabH);
            folder.lineTo(ix + tabW + tabH, iy + tabH);
            folder.lineTo(ix + tabW, iy);
            folder.lineTo(ix, iy);
            folder.closeSubPath();
            g.strokePath(folder, juce::PathStrokeType(1.2f));
        }

        // Value pill — same position as non-folder steppers
        auto valueRect = (labelOnRight ? bounds.withRight(bounds.getWidth() - nameW)
                                       : bounds.withLeft(nameW)).toFloat().reduced(1.0f);

        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(valueRect, Theme::pillCorner);

        g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.5f)
                             : juce::Colour(Theme::textDim).withAlpha(0.3f));
        g.drawRoundedRectangle(valueRect, Theme::pillCorner, 1.0f);

        // Arrows
        g.setColour(juce::Colour(Theme::coral).withAlpha(hovering ? 1.0f : 0.7f));
        g.setFont(juce::Font(Theme::arrowFontSize));

        auto leftArrow = valueRect.withWidth(Theme::arrowZone);
        auto rightArrow = valueRect.withLeft(valueRect.getRight() - Theme::arrowZone);
        g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), leftArrow, juce::Justification::centred);
        g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), rightArrow, juce::Justification::centred);

        // Value text
        auto textRect = valueRect.withLeft(valueRect.getX() + Theme::arrowZone)
                                  .withRight(valueRect.getRight() - Theme::arrowZone);
        g.setColour(valueClickOpensFolder ? juce::Colour(Theme::coral).withAlpha(0.7f)
                                          : juce::Colour(Theme::textWhite));
        g.setFont(Theme::controlFont);
        g.drawText(displayValue, textRect, juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition())) return;
        float h = (float)getHeight();
        int folderW = folderIconVisible ? (int)(h * 0.85f) : 0;
        int nameW = (int)(getWidth() * labelRatio);
        int folderLeft = labelOnRight ? (getWidth() - nameW - folderW) : (nameW - folderW);

        // Folder icon click
        if (folderIconVisible && e.x >= folderLeft && e.x < folderLeft + folderW)
        {
            if (onFolderClick) onFolderClick();
            return;
        }

        int pillLeft = labelOnRight ? 0 : nameW;
        int pillRight = labelOnRight ? (getWidth() - nameW) : getWidth();

        if (e.x >= pillLeft && e.x < pillLeft + Theme::arrowZone)
        {
            if (onStep) onStep(-1);
        }
        else if (e.x > pillRight - Theme::arrowZone && e.x <= pillRight)
        {
            if (onStep) onStep(1);
        }
        else if (valueClickOpensFolder && onFolderClick)
        {
            onFolderClick();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        int delta = (wheel.deltaY > 0) ? 1 : -1;
        if (onStep) onStep(delta);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (folderIconVisible)
        {
            float h = (float)getHeight();
            int folderW = (int)(h * 0.85f);
            int nameW = (int)(getWidth() * labelRatio);
            int folderLeft = labelOnRight ? (getWidth() - nameW - folderW) : (nameW - folderW);
            bool inFolder = e.x >= folderLeft && e.x < folderLeft + folderW;
            if (inFolder != folderHoverZone)
            {
                folderHoverZone = inFolder;
                repaint();
            }
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        repaint();
        if (tooltip.isNotEmpty())
            showTooltip();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        folderHoverZone = false;
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
    bool folderIconVisible = false;
    bool folderHoverZone = false;
    bool valueClickOpensFolder = false;

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
