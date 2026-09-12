#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

class ChartchoticLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChartchoticLookAndFeel();

    // The app builds its own menus as panels, so the only PopupMenus left are the ones JUCE
    // raises itself, like a TextEditor's context menu. Palette and face are worth setting;
    // drawing them by hand would be a renderer nothing in the UI actually shows.
    juce::Font getPopupMenuFont() override;

    // ToggleButton (used by debug panels)
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    // Slider
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;

    // TextEditor
    void fillTextEditorBackground(juce::Graphics&, int width, int height,
                                  juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height,
                               juce::TextEditor&) override;

    // TextButton
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

private:
    static constexpr float popupMenuPt = 13.0f;
};
