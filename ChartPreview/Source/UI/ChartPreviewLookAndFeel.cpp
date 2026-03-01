#include "ChartPreviewLookAndFeel.h"

ChartPreviewLookAndFeel::ChartPreviewLookAndFeel()
{
    // Base colour scheme
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(darkBg));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(coralAccent));
    setColour(juce::PopupMenu::textColourId, juce::Colour(textWhite));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(darkBg));
    setColour(juce::ComboBox::textColourId, juce::Colour(textWhite));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(coralAccent));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(coralAccent));

    setColour(juce::TextEditor::backgroundColourId, juce::Colour(darkBg));
    setColour(juce::TextEditor::textColourId, juce::Colour(textWhite));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(coralAccent));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(coralAccent));
    setColour(juce::TextEditor::highlightColourId, juce::Colour(coralAccent).withAlpha(0.4f));

    setColour(juce::Label::textColourId, juce::Colour(textWhite));

    setColour(juce::Slider::thumbColourId, juce::Colour(coralAccent));
    setColour(juce::Slider::trackColourId, juce::Colour(darkBgLighter));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(textWhite));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(darkBg));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour(juce::TextButton::buttonColourId, juce::Colour(darkBgLighter));
    setColour(juce::TextButton::textColourOffId, juce::Colour(textWhite));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

//==============================================================================
// ComboBox

void ChartPreviewLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                            bool /*isButtonDown*/,
                                            int /*buttonX*/, int /*buttonY*/,
                                            int /*buttonW*/, int /*buttonH*/,
                                            juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    auto cornerSize = 3.0f;

    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);

    // Draw arrow
    auto arrowZone = juce::Rectangle<float>(width - 20.0f, 0.0f, 16.0f, (float)height);
    juce::Path arrow;
    arrow.addTriangle(arrowZone.getX() + 2.0f, arrowZone.getCentreY() - 3.0f,
                      arrowZone.getRight() - 2.0f, arrowZone.getCentreY() - 3.0f,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void ChartPreviewLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, box.findColour(juce::ComboBox::textColourId));
}

juce::Font ChartPreviewLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(juce::jmin(13.0f, (float)box.getHeight() * 0.8f));
}

//==============================================================================
// PopupMenu

void ChartPreviewLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(juce::Colour(darkBg));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 3.0f);

    g.setColour(juce::Colour(coralAccent).withAlpha(0.5f));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, 3.0f, 1.0f);
}

void ChartPreviewLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                                 const juce::Rectangle<int>& area,
                                                 bool isSeparator, bool isActive,
                                                 bool isHighlighted, bool isTicked,
                                                 bool /*hasSubMenu*/,
                                                 const juce::String& text,
                                                 const juce::String& /*shortcutKeyText*/,
                                                 const juce::Drawable* /*icon*/,
                                                 const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        auto r = area.reduced(4, 0);
        r.removeFromTop(juce::roundToInt(((float)r.getHeight() * 0.5f) - 0.5f));
        g.setColour(juce::Colour(darkBgLighter));
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto r = area.reduced(1);

    if (isHighlighted && isActive)
    {
        g.setColour(juce::Colour(coralAccent));
        g.fillRoundedRectangle(r.toFloat(), 2.0f);
        g.setColour(juce::Colours::white);
    }
    else
    {
        g.setColour(isActive ? juce::Colour(textWhite) : juce::Colour(textDim));
    }

    auto textArea = r.reduced(8, 0);

    if (isTicked)
    {
        auto tickArea = textArea.removeFromLeft(16);
        juce::Path tick;
        tick.addLineSegment(juce::Line<float>(tickArea.getX() + 2.0f, tickArea.getCentreY(),
                                               tickArea.getCentreX(), tickArea.getBottom() - 4.0f), 1.5f);
        tick.addLineSegment(juce::Line<float>(tickArea.getCentreX(), tickArea.getBottom() - 4.0f,
                                               tickArea.getRight() - 2.0f, tickArea.getY() + 4.0f), 1.5f);
        g.strokePath(tick, juce::PathStrokeType(1.5f));
    }

    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);
}

//==============================================================================
// ToggleButton

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
        g.setColour(juce::Colour(coralAccent));
        g.fillRoundedRectangle(boxBounds, 2.0f);

        // Draw checkmark
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
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(coralAccent) : juce::Colour(textDim));
        g.drawRoundedRectangle(boxBounds, 2.0f, 1.0f);
    }

    g.setColour(juce::Colour(textWhite));
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

        // Track background
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRoundedRectangle(trackX, (float)y, trackWidth, (float)height, 2.0f);

        // Thumb
        auto thumbRadius = 8.0f;
        auto thumbY = sliderPos;
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse((float)x + (float)width * 0.5f - thumbRadius,
                      thumbY - thumbRadius,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);
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
    label->setColour(juce::Label::textColourId, juce::Colour(textWhite));
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setJustificationType(juce::Justification::centred);
    return label;
}

//==============================================================================
// TextEditor

void ChartPreviewLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                        juce::TextEditor& editor)
{
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 3.0f);
}

void ChartPreviewLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                     juce::TextEditor& editor)
{
    auto colour = editor.hasKeyboardFocus(true)
        ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
        : editor.findColour(juce::TextEditor::outlineColourId);
    g.setColour(colour);
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, 3.0f, 1.0f);
}

//==============================================================================
// TextButton

void ChartPreviewLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto cornerSize = 3.0f;

    auto baseColour = backgroundColour;
    if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.1f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds.reduced(0.5f), cornerSize);

    g.setColour(juce::Colour(coralAccent).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
}

juce::Font ChartPreviewLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::jmin(13.0f, (float)buttonHeight * 0.8f));
}
