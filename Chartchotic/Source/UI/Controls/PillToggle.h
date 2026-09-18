#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Compact pill-shaped on/off toggle.
// Filled coral when ON, outlined when OFF. Hover highlights.
class PillToggle : public juce::Component
{
public:
    PillToggle(const juce::String& text) : label(text) {}

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (on == shouldBeOn) return;
        on = shouldBeOn;
        repaint();
        if (notification != juce::dontSendNotification && onClick)
            onClick();
    }

    bool getToggleState() const { return on; }
    void setButtonText(const juce::String& text) { label = text; repaint(); }
    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void setCornerRadius(float r) { cornerRadius = r; repaint(); }

    std::function<void()> onClick;
    std::function<void(bool modifierHeld)> onClickWithModifier;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto cornerSize = (cornerRadius >= 0.0f) ? cornerRadius : Theme::pillCorner;
        bool hovering = !disabled && isMouseOver();

        if (disabled)
        {
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.2f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.25f));
        }
        else if (on)
        {
            g.setColour(hovering ? accent.brighter(0.15f) : accent);
            g.fillRoundedRectangle(bounds, cornerSize);
            g.setColour(juce::Colour(Theme::textWhite));
        }
        else
        {
            g.setColour(hovering ? accent.withAlpha(0.7f)
                                 : juce::Colour(Theme::textDim).withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            g.setColour(hovering ? juce::Colour(Theme::textWhite)
                                 : juce::Colour(Theme::textDim));
        }

        bool drewArt = false;
        if (iconDrawer)
        {
            juce::Graphics::ScopedSaveState save(g);
            if (disabled) g.setOpacity(DISABLED_ART_ALPHA);
            drewArt = iconDrawer(g, bounds.reduced(bounds.getHeight() * ART_INSET));
        }

        if (drewArt) return;

        if (icon)
        {
            auto iconBounds = getLocalBounds().toFloat().reduced(getHeight() * SVG_ICON_INSET);
            juce::Colour tint = disabled ? juce::Colour(Theme::textDim).withAlpha(0.25f)
                               : on      ? juce::Colour(Theme::textWhite)
                               : isMouseOver() ? juce::Colour(Theme::textWhite)
                                               : juce::Colour(Theme::textDim);
            auto tinted = icon->createCopy();
            tinted->replaceColour(juce::Colours::black, tint);
            tinted->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            g.setFont(Theme::smallFont);
            g.drawText(label, getLocalBounds(), juce::Justification::centred);
        }
    }

    void setIcon(std::unique_ptr<juce::Drawable> d) { icon = std::move(d); repaint(); }

    // Full-colour art in place of the label, for things the chart itself has a picture of.
    // Answers false when there is nothing to draw yet, and the label is drawn instead.
    using ArtDrawer = std::function<bool(juce::Graphics&, juce::Rectangle<float>)>;
    void setArt(ArtDrawer d) { iconDrawer = std::move(d); repaint(); }

    void setDisabled(bool shouldBeDisabled)
    {
        if (disabled == shouldBeDisabled) return;
        disabled = shouldBeDisabled;
        repaint();
    }
    bool isDisabled() const { return disabled; }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (disabled) return;
        if (getLocalBounds().contains(e.getPosition()))
        {
            on = !on;
            repaint();
            if (onClickWithModifier)
            {
                bool mod = juce::ModifierKeys::currentModifiers.isShiftDown()
                        || juce::ModifierKeys::currentModifiers.isAltDown();
                onClickWithModifier(mod);
            }
            else if (onClick)
            {
                onClick();
            }
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    static constexpr float ART_INSET = 0.05f;          // of the height, each side
    static constexpr float SVG_ICON_INSET = 0.19f;     // line icons need a margin to breathe
    static constexpr float DISABLED_ART_ALPHA = 0.3f;

    juce::String label;
    std::unique_ptr<juce::Drawable> icon;
    ArtDrawer iconDrawer;
    bool on = true;
    bool disabled = false;
    juce::Colour accent { Theme::coral };
    float cornerRadius = -1.0f;
};
