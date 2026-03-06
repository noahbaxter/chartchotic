#pragma once

#include <JuceHeader.h>

// Shared interaction state for all toolbar panels (CircleIconSelector + PopupMenuButton).
// Hovering between any toolbar panel transfers ownership; click-lock persists across transfers.
struct ToolbarPanelGroup
{
    static inline void* activeOwner = nullptr;
    static inline std::function<void()> activeDismiss;
    static inline bool locked = false;

    static bool hasActive() { return activeOwner != nullptr; }
    static bool isOwner(void* o) { return activeOwner == o; }

    // Dismiss current owner (if any) and set new owner as active.
    static void activate(void* owner, std::function<void()> dismiss, bool isLocked)
    {
        if (activeOwner != owner && activeDismiss)
        {
            auto fn = std::move(activeDismiss);
            activeOwner = nullptr;
            activeDismiss = nullptr;
            fn();
        }
        activeOwner = owner;
        activeDismiss = std::move(dismiss);
        locked = isLocked;
    }

    // Clear active state if this owner is current.
    static void deactivate(void* owner)
    {
        if (activeOwner == owner)
        {
            activeOwner = nullptr;
            activeDismiss = nullptr;
            locked = false;
        }
    }
};

namespace Theme
{
    static constexpr juce::uint32 coral         = 0xFFE8513A;
    static constexpr juce::uint32 darkBg        = 0xFF1A1A1A;
    static constexpr juce::uint32 darkBgLighter = 0xFF2A2A2A;
    static constexpr juce::uint32 darkBgHover   = 0xFF333333;
    static constexpr juce::uint32 textWhite     = 0xFFEEEEEE;
    static constexpr juce::uint32 textDim       = 0xFFAAAAAA;
    static constexpr juce::uint32 toolbarBg     = 0xCC111111;

    // Shared accent palette (matches logo dot cluster)
    static constexpr juce::uint32 green  = 0xFF5CB85C;
    static constexpr juce::uint32 red    = coral;
    static constexpr juce::uint32 yellow = 0xFFF0C030;
    static constexpr juce::uint32 blue   = 0xFF5BC0DE;
    static constexpr juce::uint32 orange = 0xFFE8853A;

    static constexpr float fontSize      = 11.0f;
    static constexpr float cornerRadius  = 3.0f;
    static constexpr float pillRadius    = 0.35f; // multiplied by height
    static constexpr float uiFontScale   = 1.2f;  // global scale for all UI text
    static constexpr float panelBgAlpha  = 0.95f;  // popup/expanded panel background opacity

    static inline juce::Typeface::Ptr getUITypeface()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::ChakraPetchMedium_ttf, BinaryData::ChakraPetchMedium_ttfSize);
        return tf;
    }

    static inline juce::Font getUIFont(float height)
    {
        return juce::Font(getUITypeface()).withHeight(height * uiFontScale);
    }
}
