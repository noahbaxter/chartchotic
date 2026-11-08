#pragma once

#include "AuthoringTypes.h"
#include <vector>

struct Binding
{
    SubMode       mode;
    EventType     event;
    MouseButton   button;
    ModifierFlags modifiers;
    WriteCommand  command;
};

class CommandMapper
{
public:
    CommandMapper();

    WriteCommand resolve(SubMode mode, EventType event,
                         const AuthoringContext& ctx) const;

    // Keys come from the user-editable table in Shortcuts.
    WriteCommand resolveKey(bool writeModeActive,
                            const juce::KeyPress& key) const;

private:
    std::vector<Binding> bindings;

    static MouseButton   buttonFromContext(const AuthoringContext& ctx);
    static ModifierFlags modifiersFromContext(const AuthoringContext& ctx);
};
