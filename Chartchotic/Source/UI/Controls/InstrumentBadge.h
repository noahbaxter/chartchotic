#pragma once

#include <JuceHeader.h>

#include "../ControlConstants.h"
#include "../Theme.h"

/**
    One instrument, drawn as the badge the games already taught everyone.

    There is one icon per instrument and several parts share each of them: pro
    guitar and guitar are the same picture, and so are elite drums and drums.
    Rock Band settles that by stamping a word across the bottom of the badge,
    so this does the same rather than inventing a mark nobody has seen.

    The word is composed here rather than by whoever is showing the badge.
    Every caller drawing its own strap is how two of them end up disagreeing
    about what elite drums look like.
*/
class InstrumentBadge : public juce::Component
{
public:
    explicit InstrumentBadge(Part instrument = Part::GUITAR) : part(instrument) {}

    void setPart(Part instrument)
    {
        if (part == instrument) return;
        part = instrument;
        repaint();
    }

    /** Quiet unless it is the one being pointed at. */
    void setDimmed(bool shouldDim)
    {
        if (dimmed == shouldDim) return;
        dimmed = shouldDim;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        draw(g, getLocalBounds(), part, dimmed ? dimOpacity : 1.0f);
    }

    /**
        Draw one badge into any rectangle.

        Offered as well as the component because a row of these is a row of
        pictures, not a row of things to click, and a child component each
        buys nothing.
    */
    static void draw(juce::Graphics& g, juce::Rectangle<int> bounds, Part part, float opacity,
                     const juce::String& variant = {})
    {
        const auto icon = getPartIcon(part);
        if (icon.data == nullptr) return;

        auto image = juce::ImageCache::getFromMemory(icon.data, icon.size);
        if (!image.isValid()) return;

        g.setOpacity(opacity);
        g.drawImage(image, bounds.toFloat(), juce::RectanglePlacement::centred);
        g.setOpacity(1.0f);

        // A caller can say which variant this is when the part alone does not
        // know: pro drums is PART DRUMS with a flag set on the song.
        const auto label = variant.isNotEmpty() ? variant : variantLabel(part);
        if (label.isEmpty()) return;

        // Over a bar of its own: the icons are white line art, and white on
        // white does not read at the size these are drawn.
        auto strap = bounds.removeFromBottom(juce::roundToInt((float)bounds.getHeight() * strapShare))
                         .toFloat();

        g.setColour(juce::Colour(Theme::darkBg).withAlpha(strapAlpha * opacity));
        g.fillRoundedRectangle(strap, strapCorner);

        // Measured down to fit rather than squeezed by the text renderer,
        // which will happily overhang the badge it is stamped on. PRO is
        // three characters and ELITE is five, on a badge twenty pixels wide.
        auto font = Theme::getUIFont(strap.getHeight() * strapFontShare);

        const float natural = font.getStringWidthFloat(label);
        if (natural > strap.getWidth())
            font = Theme::getUIFont(font.getHeight() * (strap.getWidth() / natural));

        g.setColour(juce::Colour(Theme::textWhite).withAlpha(opacity));
        g.setFont(font);
        g.drawText(label, strap.toNearestInt(), juce::Justification::centred, false);
    }

    /** The word stamped across a variant, and nothing for a plain instrument. */
    static juce::String variantLabel(Part part)
    {
        switch (part)
        {
            case Part::PRO_GUITAR:
            case Part::PRO_BASS:
            case Part::PRO_KEYS:    return "PRO";
            case Part::ELITE_DRUMS: return "ELITE";
            case Part::GHL_GUITAR:
            case Part::GHL_BASS:    return "GHL";
            case Part::HARMONIES:   return "HARM";
            default:                return {};
        }
    }

private:
    /** How much of the badge the strap takes, and how much of that is text. */
    static constexpr float strapShare     = 0.34f;
    static constexpr float strapFontShare = 0.86f;
    static constexpr float strapCorner    = 1.0f;
    static constexpr float dimOpacity     = 0.65f;

    /** Enough to sit the word on, without blacking out the picture under it. */
    static constexpr float strapAlpha = 0.85f;

    Part part;
    bool dimmed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentBadge)
};
