#include "test_helpers.h"
#include "Editor/Shortcuts.h"

namespace
{
    // Built on a fresh table each time so the tests never touch the real file.
    struct Table
    {
        Shortcuts& s = Shortcuts::get();
        explicit Table(const juce::String& text = {}) { s.loadFromText(text); }
        ~Table() { s.loadFromText({}); }
    };

    juce::KeyPress key(int code, int mods = 0) { return juce::KeyPress(code, juce::ModifierKeys(mods), 0); }
}

TEST_CASE("Shortcuts - chords parse exactly", "[shortcuts]")
{
    auto t = Shortcuts::parseChord("Shift+T");
    REQUIRE(t.has_value());
    REQUIRE(t->keyCode == 'T');
    REQUIRE(t->modifiers == ModifierFlags::Shift);

    REQUIRE(Shortcuts::parseChord("w")->keyCode == 'W');
    REQUIRE(Shortcuts::parseChord("[")->keyCode == '[');
    REQUIRE(Shortcuts::parseChord("Delete")->keyCode == juce::KeyPress::deleteKey);
    REQUIRE(Shortcuts::parseChord("esc")->keyCode == juce::KeyPress::escapeKey);
    REQUIRE(Shortcuts::parseChord("F5")->keyCode == juce::KeyPress::F5Key);
    REQUIRE(Shortcuts::parseChord("+")->keyCode == '+');
    REQUIRE(Shortcuts::parseChord("Shift++")->keyCode == '+');
    REQUIRE(Shortcuts::parseChord("Shift++")->modifiers == ModifierFlags::Shift);

    REQUIRE_FALSE(Shortcuts::parseChord("Hyper+T").has_value());
    REQUIRE_FALSE(Shortcuts::parseChord("Spacebar").has_value());
    REQUIRE_FALSE(Shortcuts::parseChord("F13").has_value());
    REQUIRE_FALSE(Shortcuts::parseChord("").has_value());
}

TEST_CASE("Shortcuts - chord text round trips", "[shortcuts]")
{
    for (const auto* s : { "Shift+T", "Alt+Left", "Escape", "]", "F12" })
        REQUIRE(Shortcuts::chordText(*Shortcuts::parseChord(s)) == juce::String(s));
}

TEST_CASE("Shortcuts - a key press matches its modifiers exactly", "[shortcuts]")
{
    REQUIRE(Shortcuts::chordOf(key('t')) == *Shortcuts::parseChord("T"));
    REQUIRE(Shortcuts::chordOf(key('T', juce::ModifierKeys::shiftModifier)) == *Shortcuts::parseChord("Shift+T"));
    REQUIRE_FALSE(Shortcuts::chordOf(key('T', juce::ModifierKeys::shiftModifier)) == *Shortcuts::parseChord("T"));
}

TEST_CASE("Shortcuts - the defaults cover every command", "[shortcuts]")
{
    Table t;
    for (const auto& c : Shortcuts::commands())
        REQUIRE(t.s.describe(c.command).isNotEmpty());
    REQUIRE(t.s.describe(WriteCommand::DeleteSelection) == "Delete, Backspace");
    REQUIRE(t.s.describe(WriteCommand::PreviousChart) == ",");
}

TEST_CASE("Shortcuts - the comma is a key as well as the separator", "[shortcuts]")
{
    Table t("previous-chart = Shift+,, ,\n");
    REQUIRE(t.s.describe(WriteCommand::PreviousChart) == "Shift+,, ,");
}

TEST_CASE("Shortcuts - the default file loads back to the defaults", "[shortcuts]")
{
    juce::StringArray errors;
    Table t;
    t.s.loadFromText(Shortcuts::defaultFileText(), &errors);
    REQUIRE(errors.isEmpty());
    REQUIRE(t.s.describe(WriteCommand::ToggleMonitorClick) == "K");
    REQUIRE(t.s.describe(WriteCommand::CycleTuplet) == "Shift+T");
}

TEST_CASE("Shortcuts - the file rebinds, unbinds and leaves the rest alone", "[shortcuts]")
{
    Table t("toggle-metronome = Shift+K, F1\n"
            "toggle-note-sounds =\n");
    REQUIRE(t.s.describe(WriteCommand::ToggleMonitorClick) == "Shift+K, F1");
    REQUIRE(t.s.describe(WriteCommand::ToggleMonitorNotes).isEmpty());
    REQUIRE(t.s.describe(WriteCommand::ToggleWriteMode) == "W");
}

TEST_CASE("Shortcuts - a bad line is reported and keeps the default", "[shortcuts]")
{
    juce::StringArray errors;
    Table t;
    t.s.loadFromText("toggle-snap = Hyper+E\n"
                     "no-such-command = X\n"
                     "just some words\n"
                     "; a comment = fine\n", &errors);
    REQUIRE(errors.size() == 3);
    REQUIRE(t.s.describe(WriteCommand::ToggleSnap) == "E");
}
