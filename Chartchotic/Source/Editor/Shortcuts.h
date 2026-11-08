#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>
#include "AuthoringTypes.h"

// Every keyboard shortcut the editor owns, with its defaults, overridable from a shortcuts.ini
// beside the palette file. The file is written out with the defaults the first time, so there is
// always one to find and edit, and it is re-read whenever it changes on disk.
//
// Mouse bindings and the keys forwarded to REAPER (space, Cmd+Z, Cmd+S) are not in here: the
// first are the authoring model itself, and the second belong to REAPER's own keymap.
class Shortcuts
{
public:
    struct Chord
    {
        int           keyCode   = 0;
        ModifierFlags modifiers = ModifierFlags::None;
        bool operator==(const Chord& o) const { return keyCode == o.keyCode && modifiers == o.modifiers; }
    };

    struct Command
    {
        const char*  id;
        WriteCommand command;
        const char*  defaultKeys;
        bool         requiresWriteMode;
        const char*  description;
    };

    static const std::vector<Command>& commands();

    // The process-wide table, loaded from disk on first use.
    static Shortcuts& get();

    static juce::File file();

    // First binding the key matches, in table order. A bare-key binding never fires with a
    // modifier held, so Cmd+S and friends still reach REAPER.
    WriteCommand resolve(bool writeModeActive, const juce::KeyPress& key);

    // The keys a command is bound to, as written in the file, for tooltips and the like.
    juce::String describe(WriteCommand command) const;

    // Replaces the bindings with the defaults, then applies `text` over them. Lines that fail to
    // parse are skipped and reported in `errors`; a command the text leaves out keeps its default.
    void loadFromText(const juce::String& text, juce::StringArray* errors = nullptr);
    static juce::String defaultFileText();

    // "Shift+T", "Delete", "[" and so on. Exact: an unknown name is an error, not a guess.
    static std::optional<Chord> parseChord(const juce::String& text);
    static juce::String chordText(const Chord& chord);

    static Chord chordOf(const juce::KeyPress& key);

private:
    Shortcuts();

    void reloadIfChanged();

    struct Bound { WriteCommand command; bool requiresWriteMode; Chord chord; };
    std::vector<Bound> bound;
    juce::Time loadedModTime;
};
