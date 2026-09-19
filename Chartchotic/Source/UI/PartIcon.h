#pragma once

#include <JuceHeader.h>
#include <map>
#include "ControlConstants.h"
#include "Theme.h"

// A part's icon with its variant badge baked in. Five images stand in for fourteen parts, so
// the badge is what tells Rhythm from Guitar and Elite from Drums. Everywhere a part is shown
// as an icon draws it through here, so the toolbar and the parts menu cannot drift apart.
namespace PartIcon
{
    inline constexpr float badgeScale = 0.42f;   // of the icon box

    inline juce::Image render(Part part, int sizePx)
    {
        // A part with no art returns nothing rather than a blank square, so a caller can tell
        // "no icon" from "transparent icon" and draw whatever it wants instead. Returning the
        // blank made every isValid() check pass and hid the fact that EVENTS had no icon.
        const auto icon = getPartIcon(part);
        if (icon.data == nullptr) return {};

        auto base = juce::ImageCache::getFromMemory(icon.data, icon.size);
        if (!base.isValid()) return {};

        juce::Image out(juce::Image::ARGB, sizePx, sizePx, true);

        juce::Graphics g(out);
        const auto box = juce::Rectangle<int>(0, 0, sizePx, sizePx);
        g.drawImage(base, box.toFloat(), juce::RectanglePlacement::centred);

        const auto badge = getPartBadge(part);
        if (badge.isEmpty()) return out;

        // Bottom right, on its own disc so it stays readable over whatever the art does
        // underneath it.
        const int side = juce::roundToInt((float)sizePx * badgeScale);
        const auto bBox = juce::Rectangle<int>(sizePx - side, sizePx - side, side, side);

        g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.95f));
        g.fillEllipse(bBox.toFloat());
        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(juce::Font(juce::FontOptions().withHeight((float)side * 0.8f).withStyle("Bold")));
        g.drawText(badge, bBox, juce::Justification::centred, false);

        return out;
    }

    // Cached by part and size. Rendering is cheap but it happens per paint, and the set of
    // (part, size) pairs on screen is tiny and stable.
    inline juce::Image get(Part part, int sizePx)
    {
        if (sizePx <= 0) return {};

        struct Key { Part part; int size; };
        static std::map<std::pair<int, int>, juce::Image> cache;

        const auto key = std::make_pair((int)part, sizePx);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        // The working set is one entry per part per drawn size, so this only grows when the
        // window is resized through a lot of sizes. Clearing wholesale is fine at that point.
        if (cache.size() > 128) cache.clear();

        auto image = render(part, sizePx);
        cache[key] = image;
        return image;
    }
}
