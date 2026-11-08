#include "Shortcuts.h"

namespace
{
    struct NamedKey { const char* name; int code; };

    // The first name for a code is the one written back out.
    const NamedKey kNamedKeys[] = {
        { "Space",     juce::KeyPress::spaceKey },
        { "Delete",    juce::KeyPress::deleteKey },
        { "Backspace", juce::KeyPress::backspaceKey },
        { "Escape",    juce::KeyPress::escapeKey },
        { "Esc",       juce::KeyPress::escapeKey },
        { "Return",    juce::KeyPress::returnKey },
        { "Enter",     juce::KeyPress::returnKey },
        { "Tab",       juce::KeyPress::tabKey },
        { "Left",      juce::KeyPress::leftKey },
        { "Right",     juce::KeyPress::rightKey },
        { "Up",        juce::KeyPress::upKey },
        { "Down",      juce::KeyPress::downKey },
        { "Home",      juce::KeyPress::homeKey },
        { "End",       juce::KeyPress::endKey },
        { "PageUp",    juce::KeyPress::pageUpKey },
        { "PageDown",  juce::KeyPress::pageDownKey },
    };

    struct NamedModifier { const char* name; ModifierFlags flag; };

    const NamedModifier kNamedModifiers[] = {
        { "Shift",   ModifierFlags::Shift },
        { "Ctrl",    ModifierFlags::Ctrl },
        { "Control", ModifierFlags::Ctrl },
        { "Alt",     ModifierFlags::Alt },
        { "Option",  ModifierFlags::Alt },
        { "Cmd",     ModifierFlags::Cmd },
        { "Command", ModifierFlags::Cmd },
    };

    // Comma separates keys but is also a key, so it only separates once there is a key before
    // it: "," and "Shift+," are the comma key, "Delete, ," is Delete and the comma key.
    juce::StringArray splitKeys(const juce::String& keys)
    {
        juce::StringArray out;
        juce::String current;
        for (auto c : keys)
        {
            const auto sofar = current.trim();
            if (c == ',' && sofar.isNotEmpty() && !sofar.endsWith("+"))
            {
                out.add(sofar);
                current.clear();
                continue;
            }
            current += c;
        }
        if (current.trim().isNotEmpty()) out.add(current.trim());
        return out;
    }
}

const std::vector<Shortcuts::Command>& Shortcuts::commands()
{
    // The authoring keys sit in the left-hand block, because the right hand is on the mouse the
    // whole time you are charting.
    static const std::vector<Command> table = {
        { "toggle-write-mode",    WriteCommand::ToggleWriteMode,    "W",                 false, "Write mode on and off" },
        { "toggle-edit-mode",     WriteCommand::ToggleSubMode,      "Q",                 true,  "Switch between draw and edit" },
        { "toggle-lane-tool",     WriteCommand::ToggleLaneTool,     "R",                 true,  "Lane tool, or put a lane on the selection" },
        { "toggle-snap",          WriteCommand::ToggleSnap,         "E",                 true,  "Snap to grid on and off" },
        { "toggle-tuplet",        WriteCommand::ToggleTuplet,       "T",                 true,  "Tuplet grid on and off" },
        { "cycle-tuplet",         WriteCommand::CycleTuplet,        "Shift+T",           true,  "Cycle the tuplet through 3, 5 and 7" },
        { "note-type-1",          WriteCommand::ModifierSlot1,      "A",                 true,  "First note type on the write bar" },
        { "note-type-2",          WriteCommand::ModifierSlot2,      "S",                 true,  "Second note type" },
        { "note-type-3",          WriteCommand::ModifierSlot3,      "D",                 true,  "Third note type" },
        { "note-type-4",          WriteCommand::ModifierSlot4,      "F",                 true,  "Fourth note type" },
        { "note-type-5",          WriteCommand::ModifierSlot5,      "G",                 true,  "Fifth note type" },
        { "toggle-bar-mode",      WriteCommand::ToggleBarMode,      "B",                 true,  "Write bar (kick and open) notes" },
        { "capture-stamp",        WriteCommand::CaptureStamp,       "C",                 true,  "Copy a stamp from the hovered notes or the selection" },
        { "recall-stamp",         WriteCommand::RecallStamp,        "V",                 true,  "Bring back the last stamp copied" },
        { "grid-coarser",         WriteCommand::StepDown,           "[",                 true,  "Halve the grid division" },
        { "grid-finer",           WriteCommand::StepUp,             "]",                 true,  "Double the grid division" },
        { "delete-selection",     WriteCommand::DeleteSelection,    "Delete, Backspace", true,  "Delete the selected notes" },
        { "deselect",             WriteCommand::DeselectAll,        "Escape",            true,  "Clear the selection or the stamp" },
        { "toggle-utility-strip", WriteCommand::ToggleUtilityStrip, "U",                 false, "Show and hide the utility strip" },
        { "toggle-strip-zoom",    WriteCommand::ToggleStripZoom,    "M",                 false, "Zoom the utility strip" },
        { "previous-chart",       WriteCommand::PreviousChart,      ",",                 false, "Jump to the previous song" },
        { "next-chart",           WriteCommand::NextChart,          ".",                 false, "Jump to the next song" },
        { "toggle-metronome",     WriteCommand::ToggleMonitorClick, "K",                 false, "Metronome click on and off" },
        { "toggle-note-sounds",   WriteCommand::ToggleMonitorNotes, "N",                 false, "Note samples on and off" },
    };
    return table;
}

Shortcuts& Shortcuts::get()
{
    static Shortcuts instance;
    return instance;
}

Shortcuts::Shortcuts()
{
    loadFromText({});
}

juce::File Shortcuts::file()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    return dir.getChildFile("Application Support/Chartchotic/shortcuts.ini");
#else
    return dir.getChildFile("Chartchotic/shortcuts.ini");
#endif
}

void Shortcuts::reloadIfChanged()
{
    auto f = file();
    if (!f.existsAsFile())
    {
        // Written out so there is always a file to find and edit.
        f.getParentDirectory().createDirectory();
        f.replaceWithText(defaultFileText());
    }

    const auto modTime = f.getLastModificationTime();
    if (modTime == loadedModTime) return;
    loadedModTime = modTime;

    juce::StringArray errors;
    loadFromText(f.loadFileAsString(), &errors);
    for (const auto& e : errors) DBG("shortcuts.ini: " + e);
}

WriteCommand Shortcuts::resolve(bool writeModeActive, const juce::KeyPress& key)
{
    reloadIfChanged();

    const auto chord = chordOf(key);
    for (const auto& b : bound)
    {
        if (b.requiresWriteMode && !writeModeActive) continue;
        if (b.chord == chord) return b.command;
    }
    return WriteCommand::None;
}

juce::String Shortcuts::describe(WriteCommand command) const
{
    juce::StringArray keys;
    for (const auto& b : bound)
        if (b.command == command) keys.add(chordText(b.chord));
    return keys.joinIntoString(", ");
}

void Shortcuts::loadFromText(const juce::String& text, juce::StringArray* errors)
{
    std::vector<std::pair<const Command*, juce::String>> keysFor;
    for (const auto& c : commands())
        keysFor.push_back({ &c, c.defaultKeys });

    int lineNumber = 0;
    for (auto line : juce::StringArray::fromLines(text))
    {
        ++lineNumber;
        line = line.upToFirstOccurrenceOf(";", false, false).trim();
        if (line.isEmpty()) continue;

        auto fail = [&](const juce::String& why)
        {
            if (errors) errors->add("line " + juce::String(lineNumber) + ": " + why);
        };

        if (!line.containsChar('=')) { fail("expected command = key"); continue; }

        const auto id = line.upToFirstOccurrenceOf("=", false, false).trim();
        const auto keys = line.fromFirstOccurrenceOf("=", false, false).trim();

        auto it = std::find_if(keysFor.begin(), keysFor.end(),
                               [&id](const auto& e) { return id.equalsIgnoreCase(e.first->id); });
        if (it == keysFor.end()) { fail("unknown command '" + id + "'"); continue; }

        // Checked whole before it replaces the default, so a typo never unbinds a command.
        bool ok = true;
        for (const auto& k : splitKeys(keys))
            if (!parseChord(k)) { fail("unknown key '" + k + "'"); ok = false; }
        if (ok) it->second = keys;
    }

    bound.clear();
    for (const auto& [command, keys] : keysFor)
        for (const auto& k : splitKeys(keys))
            if (auto chord = parseChord(k))
                bound.push_back({ command->command, command->requiresWriteMode, *chord });
}

juce::String Shortcuts::defaultFileText()
{
    juce::String out;
    out << "; Chartchotic keyboard shortcuts\n"
        << ";\n"
        << "; command = key, another key\n"
        << ";\n"
        << "; Modifiers are Shift, Ctrl, Alt and Cmd, joined with +, as in Shift+T. Named keys are\n"
        << "; Space, Delete, Backspace, Escape, Return, Tab, Left, Right, Up, Down, Home, End,\n"
        << "; PageUp, PageDown and F1 to F12. Anything else is the character itself.\n"
        << "; Leave the right side empty to unbind a command. A command missing from this file\n"
        << "; keeps its default. Changes apply the next time you press a key.\n";

    for (const auto& c : commands())
        out << "\n; " << c.description << (c.requiresWriteMode ? " (write mode)" : "") << "\n"
            << c.id << " = " << c.defaultKeys << "\n";
    return out;
}

std::optional<Shortcuts::Chord> Shortcuts::parseChord(const juce::String& text)
{
    auto t = text.trim();
    if (t.isEmpty()) return std::nullopt;

    // "+" alone, or a trailing "Shift++", is the plus key rather than a separator.
    juce::StringArray parts;
    if (t.endsWith("+") && (t.length() == 1 || t.dropLastCharacters(1).endsWith("+")))
    {
        parts = juce::StringArray::fromTokens(t.dropLastCharacters(t.length() == 1 ? 1 : 2), "+", {});
        parts.add("+");
    }
    else
    {
        parts = juce::StringArray::fromTokens(t, "+", {});
    }
    parts.trim();
    parts.removeEmptyStrings();
    if (parts.isEmpty()) return std::nullopt;

    Chord chord;
    for (int i = 0; i < parts.size() - 1; ++i)
    {
        auto m = std::find_if(std::begin(kNamedModifiers), std::end(kNamedModifiers),
                              [&](const NamedModifier& n) { return parts[i].equalsIgnoreCase(n.name); });
        if (m == std::end(kNamedModifiers)) return std::nullopt;
#if JUCE_MAC
        chord.modifiers = chord.modifiers | m->flag;
#else
        // A Mac-written file still works: Cmd there is Ctrl here.
        chord.modifiers = chord.modifiers | (m->flag == ModifierFlags::Cmd ? ModifierFlags::Ctrl : m->flag);
#endif
    }

    const auto key = parts[parts.size() - 1];
    for (const auto& n : kNamedKeys)
        if (key.equalsIgnoreCase(n.name)) { chord.keyCode = n.code; return chord; }

    if (key.length() >= 2 && (key[0] == 'F' || key[0] == 'f') && key.substring(1).containsOnly("0123456789"))
    {
        const int n = key.substring(1).getIntValue();
        if (n < 1 || n > 12) return std::nullopt;
        chord.keyCode = juce::KeyPress::F1Key + n - 1;
        return chord;
    }

    if (key.length() != 1) return std::nullopt;
    chord.keyCode = (int)juce::CharacterFunctions::toUpperCase(key[0]);
    return chord;
}

juce::String Shortcuts::chordText(const Chord& chord)
{
    juce::String out;
    if (chord.modifiers & ModifierFlags::Shift) out << "Shift+";
    if (chord.modifiers & ModifierFlags::Ctrl)  out << "Ctrl+";
    if (chord.modifiers & ModifierFlags::Alt)   out << "Alt+";
    if (chord.modifiers & ModifierFlags::Cmd)   out << "Cmd+";

    for (const auto& n : kNamedKeys)
        if (n.code == chord.keyCode) return out + n.name;
    if (chord.keyCode >= juce::KeyPress::F1Key && chord.keyCode <= juce::KeyPress::F1Key + 11)
        return out + "F" + juce::String(chord.keyCode - juce::KeyPress::F1Key + 1);
    return out + juce::String::charToString((juce::juce_wchar)chord.keyCode);
}

Shortcuts::Chord Shortcuts::chordOf(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    auto flags = ModifierFlags::None;
    if (mods.isShiftDown())   flags = flags | ModifierFlags::Shift;
    if (mods.isCtrlDown())    flags = flags | ModifierFlags::Ctrl;
    if (mods.isAltDown())     flags = flags | ModifierFlags::Alt;
#if JUCE_MAC
    // Off the Mac, JUCE reports Ctrl as Command too, so Cmd only means something here.
    if (mods.isCommandDown()) flags = flags | ModifierFlags::Cmd;
#endif

    int code = key.getKeyCode();
    if (code < 128) code = (int)juce::CharacterFunctions::toUpperCase((juce::juce_wchar)code);
    return { code, flags };
}
