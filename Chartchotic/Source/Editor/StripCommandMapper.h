/*
    ==============================================================================

        StripCommandMapper.h
        Author: Noah Baxter

        Gesture -> command table for the utility strip, the same shape as
        CommandMapper is for the highway. Every action the strip can perform is a
        named command; which gesture triggers it is data, so the mapping can be
        rebound without touching behaviour. Eventually user-configurable.

        The strip only navigates. Authoring happens on the highways, whatever view
        the editor is in.

    ==============================================================================
*/

#pragma once

#include "AuthoringTypes.h"
#include <vector>

enum class StripCommand
{
    None,
    SeekToBar,
    JumpToSection,
    BeginTimeSelection,
    UpdateTimeSelection,
    CommitTimeSelection,
};

class StripCommandMapper
{
public:
    struct Binding
    {
        EventType    event;
        MouseButton  button;
        ModifierFlags modifiers;
        bool         overObject;   // requires the pointer to be over an object
        bool         objectSensitive;   // when false, overObject is ignored
        StripCommand command;
    };

    StripCommandMapper() { bindings = defaultBindings(); }

    void setBindings(std::vector<Binding> b) { bindings = std::move(b); }
    const std::vector<Binding>& getBindings() const { return bindings; }

    StripCommand resolve(EventType event, MouseButton button, ModifierFlags mods,
                         bool overObject) const
    {
        for (const auto& b : bindings)
        {
            if (b.event != event || b.button != button) continue;
            if (b.objectSensitive && b.overObject != overObject) continue;
            if (b.modifiers != ModifierFlags::None && !(mods & b.modifiers)) continue;
            if (b.modifiers == ModifierFlags::None && mods != ModifierFlags::None) continue;
            return b.command;
        }
        return StripCommand::None;
    }

    // Click a section to jump to it, click anywhere else to seek to that bar, right drag to
    // set a loop range.
    static std::vector<Binding> defaultBindings()
    {
        const auto none = ModifierFlags::None;
        const bool anyTarget = false, needsTarget = true;

        return {
            { EventType::Down, MouseButton::Left, none, true,  needsTarget, StripCommand::JumpToSection },
            { EventType::Down, MouseButton::Left, none, false, needsTarget, StripCommand::SeekToBar },

            { EventType::Down, MouseButton::Right, none, false, anyTarget, StripCommand::BeginTimeSelection },
            { EventType::Drag, MouseButton::Right, none, false, anyTarget, StripCommand::UpdateTimeSelection },
            { EventType::Up,   MouseButton::Right, none, false, anyTarget, StripCommand::CommitTimeSelection },
        };
    }

private:
    std::vector<Binding> bindings;
};
