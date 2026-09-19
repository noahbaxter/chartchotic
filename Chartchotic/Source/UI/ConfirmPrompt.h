#pragma once

#include <JuceHeader.h>
#include "Theme.h"

// A yes/no the app asks in its own voice. JUCE's AlertWindow draws host-native chrome that
// looks nothing like the rest of the plugin and, in a DAW, reads as the DAW asking rather
// than us. Drawn here instead: same panel ground, same rounded chrome, same palette.
//
// Its own window rather than an overlay inside the editor. A panel drawn into the plugin
// window inherits the host's UI scale, so the question came out tiny in exactly the small
// window where it matters most. A window of its own is the one thing here at a fixed pixel
// size, whatever the plugin is scaled to.
class ConfirmPrompt : public juce::Component
{
public:
    ConfirmPrompt()
    {
        setWantsKeyboardFocus(true);
        setVisible(false);
        setSize(panelW, panelH);
    }

    ~ConfirmPrompt() override { removeFromDesktop(); }

    // `destructive` paints the confirm button in the warning colour. Everything this is
    // currently used for is, but a plain confirmation should not look like a warning.
    // `owner` is only there to centre on: the prompt is not its child.
    void ask(const juce::String& promptTitle,
             const juce::String& promptBody,
             const juce::String& confirmLabel,
             bool destructive,
             std::function<void(bool)> onResult,
             juce::Component* owner)
    {
        title = promptTitle;
        body = promptBody;
        confirmText = confirmLabel;
        isDestructive = destructive;
        result = std::move(onResult);

        if (!isOnDesktop())
            addToDesktop(juce::ComponentPeer::windowIsTemporary
                         | juce::ComponentPeer::windowHasDropShadow);

        // Screen coordinates now, so it lands over the plugin window rather than at whatever
        // the last parent-relative bounds happened to be.
        const auto over = owner != nullptr
                        ? owner->getScreenBounds()
                        : juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setBounds(juce::Rectangle<int>(panelW, body.isEmpty() ? bodylessPanelH : panelH)
                      .withCentre(over.getCentre()));

        setAlwaysOnTop(true);
        setVisible(true);
        toFront(true);
        grabKeyboardFocus();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto panel = panelBounds().toFloat();
        g.setColour(juce::Colour(Theme::panelNeutralBg));
        g.fillRoundedRectangle(panel, corner);
        g.setColour(juce::Colour(isDestructive ? Theme::coral : Theme::textDim)
                        .withAlpha(outlineAlpha));
        g.drawRoundedRectangle(panel, corner, 1.0f);

        auto inner = panelBounds().reduced(pad * 2);

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(juce::Font(juce::FontOptions().withHeight(titleFontH).withStyle("Bold")));

        // A question with nothing under it sits in the middle of its own space rather than
        // pinned to the top with a hole below it.
        if (body.isEmpty())
        {
            // Fitted, not drawn flat: the question carries a track name, and a long one has
            // to wrap rather than be squeezed to nothing.
            g.drawFittedText(title, inner.withTrimmedBottom(buttonH + pad),
                             juce::Justification::centredLeft, 2);
            paintButton(g, cancelBounds(), "Cancel", false, hovered == Button::Cancel);
            paintButton(g, confirmBounds(), confirmText, true, hovered == Button::Confirm);
            return;
        }

        g.drawText(title, inner.removeFromTop(titleLineH), juce::Justification::centredLeft, false);

        inner.removeFromTop(pad);
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(juce::FontOptions().withHeight(bodyFontH));
        g.drawFittedText(body, inner.removeFromTop(inner.getHeight() - buttonH - pad),
                         juce::Justification::topLeft, 4);

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

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)  { finish(false); return true; }
        if (key == juce::KeyPress::returnKey)  { finish(true);  return true; }
        return false;
    }

private:
    enum class Button { None, Cancel, Confirm };

    // Fixed pixels, on purpose. Nothing here scales with the plugin: the window is the same
    // size whether the editor is a strip at the bottom of REAPER or filling a screen.
    static constexpr int panelW = 520;
    static constexpr int panelH = 240;
    static constexpr int bodylessPanelH = 150;   // just the question and the two buttons
    static constexpr int pad = 14;
    static constexpr int buttonH = 38;
    static constexpr int buttonW = 128;
    static constexpr int titleLineH = 34;
    static constexpr float titleFontH = 23.0f;
    static constexpr float bodyFontH = 18.0f;
    static constexpr float buttonFontH = 17.0f;
    static constexpr float corner = 6.0f;
    static constexpr float outlineAlpha = 0.5f;

    juce::String title, body, confirmText;
    bool isDestructive = true;
    Button hovered = Button::None;
    std::function<void(bool)> result;

    // The window is the panel now, pulled in by a pixel so the outline is not clipped.
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
        const auto accent = juce::Colour(isDestructive ? Theme::coral : Theme::purple);

        if (primary)
        {
            g.setColour(accent.withAlpha(isHovered ? 1.0f : 0.85f));
            g.fillRoundedRectangle(box.toFloat(), corner);
            g.setColour(juce::Colour(Theme::textWhite));
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
        setVisible(false);
        removeFromDesktop();   // no stray window left behind once it is answered
        hovered = Button::None;

        // Moved out first: the callback can raise this prompt again, and it would otherwise
        // be overwritten mid-call.
        auto callback = std::move(result);
        result = nullptr;
        if (callback) callback(confirmed);
    }
};
