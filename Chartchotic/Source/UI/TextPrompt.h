#pragma once

#include <JuceHeader.h>
#include "Theme.h"

// A one-line text question, in its own window for the same reason ConfirmPrompt is: it stays a
// fixed pixel size whatever the plugin is scaled to.
class TextPrompt : public juce::Component
{
public:
    TextPrompt()
    {
        setVisible(false);
        setSize(panelW, panelH);

        input.setFont(juce::FontOptions().withHeight(inputFontH));
        input.setSelectAllWhenFocused(true);
        input.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBg));
        input.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
        input.setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::textDim).withAlpha(0.4f));
        input.setColour(juce::TextEditor::focusedOutlineColourId,
                        juce::Colour(Theme::textWhite).withAlpha(0.6f));
        input.onReturnKey = [this] { finish(true); };
        input.onEscapeKey = [this] { finish(false); };
        addAndMakeVisible(input);
    }

    ~TextPrompt() override { removeFromDesktop(); }

    // `owner` is only there to centre on: the prompt is not its child. An empty answer counts
    // as a cancel, so a section is never written with no name.
    void ask(const juce::String& promptTitle,
             const juce::String& initialText,
             const juce::String& confirmLabel,
             std::function<void(const juce::String&)> onResult,
             juce::Component* owner)
    {
        title = promptTitle;
        confirmText = confirmLabel;
        result = std::move(onResult);
        input.setText(initialText, false);

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
        input.grabKeyboardFocus();
        repaint();
    }

    void resized() override
    {
        input.setBounds(panelBounds().reduced(pad * 2)
                            .withTrimmedTop(titleLineH + pad)
                            .withHeight(inputH));
    }

    void paint(juce::Graphics& g) override
    {
        const auto panel = panelBounds().toFloat();
        g.setColour(juce::Colour(Theme::panelNeutralBg));
        g.fillRoundedRectangle(panel, corner);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(outlineAlpha));
        g.drawRoundedRectangle(panel, corner, 1.0f);

        auto inner = panelBounds().reduced(pad * 2);
        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(juce::Font(juce::FontOptions().withHeight(titleFontH).withStyle("Bold")));
        g.drawText(title, inner.removeFromTop(titleLineH), juce::Justification::centredLeft, false);

        paintButton(g, cancelBounds(), "Cancel", false, hovered == Button::Cancel);
        paintButton(g, confirmBounds(), confirmText, true, hovered == Button::Confirm);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        auto now = Button::None;
        if (cancelBounds().contains(e.getPosition()))       now = Button::Cancel;
        else if (confirmBounds().contains(e.getPosition())) now = Button::Confirm;

        if (now != hovered) { hovered = now; repaint(); }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (confirmBounds().contains(e.getPosition()))     { finish(true);  return; }
        if (cancelBounds().contains(e.getPosition()))        finish(false);
    }

private:
    enum class Button { None, Cancel, Confirm };

    static constexpr int panelW = 520;
    static constexpr int panelH = 170;
    static constexpr int pad = 14;
    static constexpr int buttonH = 38;
    static constexpr int buttonW = 128;
    static constexpr int titleLineH = 34;
    static constexpr int inputH = 36;
    static constexpr float titleFontH = 23.0f;
    static constexpr float inputFontH = 20.0f;
    static constexpr float buttonFontH = 17.0f;
    static constexpr float corner = 6.0f;
    static constexpr float outlineAlpha = 0.5f;

    juce::TextEditor input;
    juce::String title, confirmText;
    Button hovered = Button::None;
    std::function<void(const juce::String&)> result;

    juce::Rectangle<int> panelBounds() const { return getLocalBounds().reduced(1); }

    juce::Rectangle<int> confirmBounds() const
    {
        const auto panel = panelBounds();
        return { panel.getRight() - pad * 2 - buttonW, panel.getBottom() - pad * 2 - buttonH,
                 buttonW, buttonH };
    }

    juce::Rectangle<int> cancelBounds() const
    { return confirmBounds().translated(-(buttonW + pad), 0); }

    void paintButton(juce::Graphics& g, juce::Rectangle<int> box, const juce::String& label,
                     bool primary, bool isHovered)
    {
        if (primary)
        {
            g.setColour(juce::Colour(Theme::textWhite).withAlpha(isHovered ? 1.0f : 0.85f));
            g.fillRoundedRectangle(box.toFloat(), corner);
            g.setColour(juce::Colour(Theme::darkBg));
        }
        else
        {
            g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(isHovered ? 1.0f : 0.6f));
            g.fillRoundedRectangle(box.toFloat(), corner);
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.4f));
            g.drawRoundedRectangle(box.toFloat(), corner, 1.0f);
            g.setColour(juce::Colour(isHovered ? Theme::textWhite : Theme::textDim));
        }

        g.setFont(juce::FontOptions().withHeight(buttonFontH));
        g.drawText(label, box, juce::Justification::centred, false);
    }

    void finish(bool confirmed)
    {
        const auto text = input.getText().trim();
        setVisible(false);
        removeFromDesktop();
        hovered = Button::None;

        // Moved out first: the callback can raise this prompt again.
        auto callback = std::move(result);
        result = nullptr;
        if (callback && confirmed && text.isNotEmpty()) callback(text);
    }
};
