#pragma once

#include <JuceHeader.h>

// Shared interaction state for all toolbar panels (CircleIconSelector + PopupMenuButton).
// Hovering between any toolbar panel transfers ownership; click-lock persists across transfers.
struct ToolbarPanelGroup
{
    static inline void* activeOwner = nullptr;
    static inline std::function<void()> activeDismiss;
    static inline bool locked = false;

    // Toolbar member registry — only these components participate in hover/lock behavior
    static constexpr int maxMembers = 8;
    static inline void* members[maxMembers] = {};
    static inline int memberCount = 0;

    static void registerMember(void* m)
    {
        if (memberCount < maxMembers)
            members[memberCount++] = m;
    }

    static void unregisterMember(void* m)
    {
        for (int i = 0; i < memberCount; ++i)
        {
            if (members[i] == m)
            {
                members[i] = members[--memberCount];
                return;
            }
        }
    }

    static bool isMember(void* m)
    {
        for (int i = 0; i < memberCount; ++i)
            if (members[i] == m) return true;
        return false;
    }

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
