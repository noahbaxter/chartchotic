#include "ChartPreviewLookAndFeel.h"

ChartPreviewLookAndFeel::ChartPreviewLookAndFeel()
{
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBg));
    setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::coral));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::coral));
    setColour(juce::TextEditor::highlightColourId, juce::Colour(Theme::coral).withAlpha(0.4f));

    setColour(juce::Label::textColourId, juce::Colour(Theme::textWhite));

    setColour(juce::Slider::thumbColourId, juce::Colour(Theme::coral));
    setColour(juce::Slider::trackColourId, juce::Colour(Theme::darkBgLighter));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(Theme::textWhite));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(Theme::darkBg));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textWhite));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

//==============================================================================
// ToggleButton (used by debug panels)

void ChartPreviewLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto boxSize = juce::jmin(14.0f, bounds.getHeight() - 2.0f);
    auto boxBounds = juce::Rectangle<float>(bounds.getX() + 2.0f,
                                             bounds.getCentreY() - boxSize * 0.5f,
                                             boxSize, boxSize);

    if (button.getToggleState())
    {
        g.setColour(juce::Colour(Theme::coral));
        g.fillRoundedRectangle(boxBounds, 2.0f);

        g.setColour(juce::Colours::white);
        juce::Path tick;
        auto b = boxBounds.reduced(3.0f);
        tick.startNewSubPath(b.getX(), b.getCentreY());
        tick.lineTo(b.getCentreX() - 1.0f, b.getBottom());
        tick.lineTo(b.getRight(), b.getY());
        g.strokePath(tick, juce::PathStrokeType(1.5f));
    }
    else
    {
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(Theme::coral)
                                                   : juce::Colour(Theme::textDim));
        g.drawRoundedRectangle(boxBounds, 2.0f, 1.0f);
    }

    g.setColour(juce::Colour(Theme::textWhite));
    auto textBounds = bounds.withLeft(boxBounds.getRight() + 6.0f);
    g.drawFittedText(button.getButtonText(), textBounds.toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

//==============================================================================
// Slider

void ChartPreviewLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                                int width, int height,
                                                float sliderPos,
                                                float /*minSliderPos*/, float /*maxSliderPos*/,
                                                juce::Slider::SliderStyle style,
                                                juce::Slider& slider)
{
    if (style == juce::Slider::LinearVertical)
    {
        auto trackWidth = 4.0f;
        auto trackX = (float)x + (float)width * 0.5f - trackWidth * 0.5f;

        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRoundedRectangle(trackX, (float)y, trackWidth, (float)height, 2.0f);

        auto thumbRadius = 8.0f;
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse((float)x + (float)width * 0.5f - thumbRadius,
                      sliderPos - thumbRadius,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);
    }
    else if (style == juce::Slider::LinearHorizontal)
    {
        auto trackH = 3.0f;
        auto trackY = (float)y + (float)height * 0.5f - trackH * 0.5f;

        // Track background
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRoundedRectangle((float)x, trackY, (float)width, trackH, 1.5f);

        // Filled portion
        g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(0.3f));
        g.fillRoundedRectangle((float)x, trackY, sliderPos - (float)x, trackH, 1.5f);

        // Thumb
        auto thumbR = 6.0f;
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(sliderPos - thumbR,
                      (float)y + (float)height * 0.5f - thumbR,
                      thumbR * 2.0f, thumbR * 2.0f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                          0.0f, 0.0f, style, slider);
    }
}

juce::Label* ChartPreviewLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::textColourId, juce::Colour(Theme::textWhite));
    label->setColour(juce::Label::backgroundColourId, juce::Colour(Theme::darkBg));
    label->setColour(juce::Label::outlineColourId, juce::Colour(Theme::coral).withAlpha(0.3f));
    label->setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBg));
    label->setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
    label->setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::coral).withAlpha(0.5f));
    label->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::coral));
    label->setFont(Theme::getUIFont(Theme::fontSize));
    label->setJustificationType(juce::Justification::centred);
    return label;
}

//==============================================================================
// TextEditor

void ChartPreviewLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                        juce::TextEditor& editor)
{
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, Theme::cornerRadius);
}

void ChartPreviewLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                     juce::TextEditor& editor)
{
    auto colour = editor.hasKeyboardFocus(true)
        ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
        : editor.findColour(juce::TextEditor::outlineColourId);
    g.setColour(colour);
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f,
                           Theme::cornerRadius, 1.0f);
}

//==============================================================================
// TextButton

void ChartPreviewLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    bool toggled = button.getToggleState();

    // Suppress hover highlight on toolbar buttons when another panel is click-locked
    bool suppressHover = ToolbarPanelGroup::isMember(&button)
        && ToolbarPanelGroup::locked
        && ToolbarPanelGroup::hasActive()
        && !ToolbarPanelGroup::isOwner(&button);
    bool highlighted = (shouldDrawButtonAsHighlighted || toggled) && !suppressHover;

    if (highlighted)
    {
        // Hover or open — coral fill + full border
        g.setColour(juce::Colour(Theme::coral).withAlpha(0.25f));
        g.fillRoundedRectangle(bounds, Theme::cornerRadius);

        g.setColour(juce::Colour(Theme::coral));
        g.drawRoundedRectangle(bounds, Theme::cornerRadius, 1.0f);
    }
    else
    {
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(bounds, Theme::cornerRadius);

        g.setColour(juce::Colour(Theme::coral).withAlpha(0.6f));
        g.drawRoundedRectangle(bounds, Theme::cornerRadius, 1.0f);
    }
}

juce::Font ChartPreviewLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return Theme::getUIFont((float)buttonHeight * 0.46f);
}
