#pragma once

#include <JuceHeader.h>

class ChartPreviewLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Colour palette
    static constexpr juce::uint32 coralAccent    = 0xFFE85D45;
    static constexpr juce::uint32 darkBg         = 0xFF1A1A1A;
    static constexpr juce::uint32 darkBgLighter  = 0xFF2A2A2A;
    static constexpr juce::uint32 darkBgHover    = 0xFF333333;
    static constexpr juce::uint32 textWhite      = 0xFFEEEEEE;
    static constexpr juce::uint32 textDim        = 0xFFAAAAAA;
    static constexpr juce::uint32 toolbarBg      = 0xCC111111;

    ChartPreviewLookAndFeel();

    // ComboBox
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    // PopupMenu
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    // ToggleButton
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
};
