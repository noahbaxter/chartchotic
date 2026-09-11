/*
    ==============================================================================

        StripCommandMapper.h
        Author: Noah Baxter

        Gesture -> command table for the utility strip, the same shape as
        CommandMapper is for the highway. Every action the strip can perform is a
        named command; which gesture triggers it is data, so the mapping can be
        rebound without touching behaviour. Eventually user-configurable.

        The strip has no Draw/Edit split: its objects are sparse and large enough
        that the target disambiguates a click on its own, which is the only reason
        the highway needs modes. Write off navigates, write on edits.

    ==============================================================================
*/

#pragma once

#include "AuthoringTypes.h"
#include <vector>

enum class StripMode { Play, Write };

enum class StripCommand
{
    None,

    // Navigation, available with write off.
    SeekToBar,
    JumpToSection,
    BeginTimeSelection,
    UpdateTimeSelection,
    CommitTimeSelection,

    // Selection.
    SelectAt,
    AddToSelection,
    BeginMarquee,
    UpdateMarquee,
    CommitMarquee,

    // Mutation.
    BeginMove,
    UpdateMove,
    CommitMove,
    BeginResize,
    UpdateResize,
    CommitResize,
    ToggleAt,        // delete what is under the cursor, or create where nothing is
    DeleteAt,
    BeginDeleteSpan,
    UpdateDeleteSpan,
    CommitDeleteSpan,
    Rename,
};

class StripCommandMapper
{
public:
    struct Binding
    {
        StripMode    mode;
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

    StripCommand resolve(StripMode mode, EventType event, MouseButton button,
                         ModifierFlags mods, bool overObject) const
    {
        for (const auto& b : bindings)
        {
            if (b.mode != mode || b.event != event || b.button != button) continue;
            if (b.objectSensitive && b.overObject != overObject) continue;
            if (b.modifiers != ModifierFlags::None && !(mods & b.modifiers)) continue;
            if (b.modifiers == ModifierFlags::None && mods != ModifierFlags::None) continue;
            return b.command;
        }
        return StripCommand::None;
    }

    // The starting set: click selects, double click creates or deletes, drag moves or
    // marquees, right deletes. Ableton-shaped, and the same grammar gem edit mode uses.
    static std::vector<Binding> defaultBindings()
    {
        const auto none = ModifierFlags::None;
        const bool anyTarget = false, needsTarget = true;

        return {
            // --- Write off: navigate, and set a loop range ---
            { StripMode::Play, EventType::Down, MouseButton::Left, none, true,  needsTarget, StripCommand::JumpToSection },
            { StripMode::Play, EventType::Down, MouseButton::Left, none, false, needsTarget, StripCommand::SeekToBar },

            { StripMode::Play, EventType::Down, MouseButton::Right, none, false, anyTarget, StripCommand::BeginTimeSelection },
            { StripMode::Play, EventType::Drag, MouseButton::Right, none, false, anyTarget, StripCommand::UpdateTimeSelection },
            { StripMode::Play, EventType::Up,   MouseButton::Right, none, false, anyTarget, StripCommand::CommitTimeSelection },

            // --- Write on ---
            { StripMode::Write, EventType::Down, MouseButton::Left, ModifierFlags::Shift, true, needsTarget, StripCommand::AddToSelection },

            { StripMode::Write, EventType::Down, MouseButton::Left, none, true,  needsTarget, StripCommand::SelectAt },
            { StripMode::Write, EventType::Drag, MouseButton::Left, none, true,  needsTarget, StripCommand::UpdateMove },
            { StripMode::Write, EventType::Up,   MouseButton::Left, none, true,  needsTarget, StripCommand::CommitMove },

            { StripMode::Write, EventType::Down, MouseButton::Left, none, false, needsTarget, StripCommand::BeginMarquee },
            { StripMode::Write, EventType::Drag, MouseButton::Left, none, false, needsTarget, StripCommand::UpdateMarquee },
            { StripMode::Write, EventType::Up,   MouseButton::Left, none, false, needsTarget, StripCommand::CommitMarquee },

            { StripMode::Write, EventType::DoubleClick, MouseButton::Left, none, false, anyTarget, StripCommand::ToggleAt },

            { StripMode::Write, EventType::Down, MouseButton::Right, none, true,  needsTarget, StripCommand::DeleteAt },
            { StripMode::Write, EventType::Down, MouseButton::Right, none, false, needsTarget, StripCommand::BeginDeleteSpan },
            { StripMode::Write, EventType::Drag, MouseButton::Right, none, false, needsTarget, StripCommand::UpdateDeleteSpan },
            { StripMode::Write, EventType::Up,   MouseButton::Right, none, false, needsTarget, StripCommand::CommitDeleteSpan },
        };
    }

private:
    std::vector<Binding> bindings;
};
