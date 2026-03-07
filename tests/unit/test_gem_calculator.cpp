#include "test_helpers.h"
#include "Midi/Utils/GemCalculator.h"

using Guitar = MidiPitchDefinitions::Guitar;
using Drums = MidiPitchDefinitions::Drums;

// ============================================================================
// Guitar modifier priority (regressions #1, #10)
// Bug: modifier pitches routed through getGuitarColumn → defaulted to orange
// Bug: guitar modifiers not processed before playable notes

TEST_CASE("Guitar gem type - modifier priority", "[gem_calculator][guitar]")
{
    TestFixture f;

    SECTION("strum modifier held → NOTE")
    {
        f.addModifier((uint)Guitar::EXPERT_STRUM, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("HOPO modifier held → HOPO_GHOST")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("TAP modifier held → TAP_ACCENT")
    {
        f.addModifier((uint)Guitar::TAP, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::TAP_ACCENT);
    }

    SECTION("strum + HOPO both held → NOTE (strum wins)")
    {
        f.addModifier((uint)Guitar::EXPERT_STRUM, PPQ(0.0), PPQ(4.0));
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("strum + TAP both held → NOTE (strum wins)")
    {
        f.addModifier((uint)Guitar::EXPERT_STRUM, PPQ(0.0), PPQ(4.0));
        f.addModifier((uint)Guitar::TAP, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("no modifier → NOTE (default strum)")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }
}

// ============================================================================
// Guitar chords (regressions #2, #3)
// Bug: chord HOPO fixing happened per-note instead of after all notes inserted
// Bug: chords could be auto-HOPOs

TEST_CASE("Guitar gem type - chords always strum", "[gem_calculator][guitar][chord]")
{
    TestFixture f;
    // Enable auto-HOPO so we can verify chords override it
    f.state.setProperty("autoHopo", true, nullptr);
    f.state.setProperty("hopoThreshold", 0, nullptr); // 1/16

    SECTION("two notes at same PPQ, no modifiers → both NOTE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0));

        Gem green = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        Gem red = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(green == Gem::NOTE);
        REQUIRE(red == Gem::NOTE);
    }

    SECTION("chord that would individually auto-HOPO → both NOTE")
    {
        // Previous note to trigger auto-HOPO distance
        f.addNote((uint)Guitar::EXPERT_ORANGE, PPQ(1.7), PPQ(1.9));
        // Chord close enough to auto-HOPO
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.9), PPQ(2.5));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.9), PPQ(2.5));

        Gem green = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(1.9), f.state, f.array, f.lock);
        Gem red = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, PPQ(1.9), f.state, f.array, f.lock);
        REQUIRE(green == Gem::NOTE);
        REQUIRE(red == Gem::NOTE);
    }

    SECTION("chord with forced HOPO modifier → HOPO_GHOST")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0));

        Gem green = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        Gem red = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(green == Gem::HOPO_GHOST);
        REQUIRE(red == Gem::HOPO_GHOST);
    }

    SECTION("three-note chord → all NOTE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_YELLOW, PPQ(2.0), PPQ(3.0));

        Gem green = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        Gem red = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, PPQ(2.0), f.state, f.array, f.lock);
        Gem yellow = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_YELLOW, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(green == Gem::NOTE);
        REQUIRE(red == Gem::NOTE);
        REQUIRE(yellow == Gem::NOTE);
    }

    SECTION("two notes outside chord tolerance → treated independently")
    {
        // Previous note close enough for auto-HOPO
        f.addNote((uint)Guitar::EXPERT_ORANGE, PPQ(1.65), PPQ(1.75));
        // Two notes far enough apart to not be a chord
        PPQ separation = MIDI_CHORD_TOLERANCE + PPQ(0.05);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.8), PPQ(2.5));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.8) + separation, PPQ(3.0));

        // Green should auto-HOPO (single note, within threshold of orange, different column)
        Gem green = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(1.8), f.state, f.array, f.lock);
        REQUIRE(green == Gem::HOPO_GHOST);
    }
}

// ============================================================================
// Auto-HOPO threshold logic (regression #3)

TEST_CASE("Guitar gem type - auto-HOPO", "[gem_calculator][guitar][auto_hopo]")
{
    TestFixture f;

    SECTION("auto-HOPO off → never auto-HOPO")
    {
        f.state.setProperty("autoHopo", false, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.1), PPQ(1.6));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, PPQ(1.1), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("1/16 threshold, different column within threshold → HOPO_GHOST")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 0, nullptr);
        // Place notes within 1/16th note distance
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.2));
        PPQ sixteenthApart = PPQ(1.0) + MIDI_HOPO_SIXTEENTH - PPQ(0.01);
        f.addNote((uint)Guitar::EXPERT_RED, sixteenthApart, sixteenthApart + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, sixteenthApart, f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("same column → NOTE (same color can't auto-HOPO)")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 0, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.2));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.15), PPQ(1.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(1.15), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("previous note was chord → current note NOT auto-HOPO")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 0, nullptr);
        // Chord at PPQ 1.0
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.0), PPQ(1.5));
        // Single note close after
        f.addNote((uint)Guitar::EXPERT_YELLOW, PPQ(1.15), PPQ(1.6));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_YELLOW, PPQ(1.15), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("note just beyond threshold → NOTE")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 0, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        // Place beyond threshold + buffer
        PPQ beyondThreshold = PPQ(1.0) + MIDI_HOPO_SIXTEENTH + MIDI_HOPO_THRESHOLD_BUFFER + PPQ(0.02);
        f.addNote((uint)Guitar::EXPERT_RED, beyondThreshold, beyondThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, beyondThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("no previous note in search window → NOTE")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 0, nullptr);
        // Only one note, no previous
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(5.0), PPQ(5.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_GREEN, PPQ(5.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("Dot 1/16 threshold, within threshold → HOPO_GHOST")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 1, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.2));
        PPQ withinThreshold = PPQ(1.0) + MIDI_HOPO_SIXTEENTH_DOT - PPQ(0.01);
        f.addNote((uint)Guitar::EXPERT_RED, withinThreshold, withinThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, withinThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("Dot 1/16 threshold, beyond threshold → NOTE")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 1, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        PPQ beyondThreshold = PPQ(1.0) + MIDI_HOPO_SIXTEENTH_DOT + MIDI_HOPO_THRESHOLD_BUFFER + PPQ(0.02);
        f.addNote((uint)Guitar::EXPERT_RED, beyondThreshold, beyondThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, beyondThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("170 Tick threshold, within threshold → HOPO_GHOST")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 2, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.2));
        PPQ withinThreshold = PPQ(1.0) + MIDI_HOPO_CLASSIC_170 - PPQ(0.01);
        f.addNote((uint)Guitar::EXPERT_RED, withinThreshold, withinThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, withinThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("170 Tick threshold, beyond threshold → NOTE")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 2, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        PPQ beyondThreshold = PPQ(1.0) + MIDI_HOPO_CLASSIC_170 + MIDI_HOPO_THRESHOLD_BUFFER + PPQ(0.02);
        f.addNote((uint)Guitar::EXPERT_RED, beyondThreshold, beyondThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, beyondThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("1/8 threshold, within threshold → HOPO_GHOST")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 3, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.2));
        PPQ withinThreshold = PPQ(1.0) + MIDI_HOPO_EIGHTH - PPQ(0.01);
        f.addNote((uint)Guitar::EXPERT_RED, withinThreshold, withinThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, withinThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("1/8 threshold, beyond threshold → NOTE")
    {
        f.state.setProperty("autoHopo", true, nullptr);
        f.state.setProperty("hopoThreshold", 3, nullptr);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.5));
        PPQ beyondThreshold = PPQ(1.0) + MIDI_HOPO_EIGHTH + MIDI_HOPO_THRESHOLD_BUFFER + PPQ(0.02);
        f.addNote((uint)Guitar::EXPERT_RED, beyondThreshold, beyondThreshold + PPQ(0.5));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::EXPERT_RED, beyondThreshold, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }
}

// ============================================================================
// Drum gem type — cymbal/tom logic (regressions #4, #5)
// Bug: tom markers not processed before playable notes → all SP drums were cymbals
// Bug: snare accidentally excluded from dynamics

TEST_CASE("Drum gem type - pro drums cymbal vs tom", "[gem_calculator][drums]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);
    f.state.setProperty("drumType", (int)DrumType::PRO, nullptr);

    SECTION("yellow with TOM_YELLOW modifier → NOTE (tom)")
    {
        f.addModifier((uint)Drums::TOM_YELLOW, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("yellow WITHOUT TOM_YELLOW → CYM (cymbal)")
    {
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::CYM);
    }

    SECTION("blue with TOM_BLUE → NOTE")
    {
        f.addModifier((uint)Drums::TOM_BLUE, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Drums::EXPERT_BLUE, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_BLUE, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("green with TOM_GREEN → NOTE")
    {
        f.addModifier((uint)Drums::TOM_GREEN, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Drums::EXPERT_GREEN, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_GREEN, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("normal drums → always NOTE regardless of tom markers")
    {
        f.state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("red/kick → never cymbal even in pro drums")
    {
        f.addNote((uint)Drums::EXPERT_RED, PPQ(2.0), PPQ(2.5));
        f.addNote((uint)Drums::EXPERT_KICK, PPQ(2.0), PPQ(2.5));

        Gem red = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_RED, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        Gem kick = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_KICK, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(red == Gem::NOTE);
        REQUIRE(kick == Gem::NOTE);
    }
}

TEST_CASE("Drum gem type - dynamics", "[gem_calculator][drums][dynamics]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);
    f.state.setProperty("drumType", (int)DrumType::NORMAL, nullptr);

    SECTION("ghost velocity on non-kick → HOPO_GHOST")
    {
        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_RED, PPQ(2.0), Dynamic::GHOST, f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }

    SECTION("accent velocity on non-kick → TAP_ACCENT")
    {
        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_RED, PPQ(2.0), Dynamic::ACCENT, f.state, f.array, f.lock);
        REQUIRE(result == Gem::TAP_ACCENT);
    }

    SECTION("ghost velocity on kick → NOTE (kicks exempt)")
    {
        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_KICK, PPQ(2.0), Dynamic::GHOST, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }

    SECTION("dynamics disabled → always NOTE regardless of velocity")
    {
        f.state.setProperty("dynamics", 0, nullptr);
        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_RED, PPQ(2.0), Dynamic::GHOST, f.state, f.array, f.lock);
        REQUIRE(result == Gem::NOTE);
    }
}

// ============================================================================
// Drum gem type — cymbal + dynamics combos

TEST_CASE("Drum gem type - cymbal with dynamics", "[gem_calculator][drums][cymbal]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);
    f.state.setProperty("drumType", (int)DrumType::PRO, nullptr);

    SECTION("ghost on cymbal-eligible pitch → CYM_GHOST")
    {
        // No TOM modifier → cymbal
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::GHOST, f.state, f.array, f.lock);
        REQUIRE(result == Gem::CYM_GHOST);
    }

    SECTION("accent on cymbal-eligible pitch → CYM_ACCENT")
    {
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::ACCENT, f.state, f.array, f.lock);
        REQUIRE(result == Gem::CYM_ACCENT);
    }

    SECTION("normal velocity on cymbal-eligible pitch → CYM")
    {
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::NONE, f.state, f.array, f.lock);
        REQUIRE(result == Gem::CYM);
    }

    SECTION("ghost on cymbal with dynamics disabled → CYM")
    {
        f.state.setProperty("dynamics", 0, nullptr);
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        Gem result = GemCalculator::getDrumGemType(
            (uint)Drums::EXPERT_YELLOW, PPQ(2.0), Dynamic::GHOST, f.state, f.array, f.lock);
        REQUIRE(result == Gem::CYM);
    }
}

// ============================================================================
// Non-EXPERT skill guitar gem type

TEST_CASE("Guitar gem type - non-EXPERT skill", "[gem_calculator][guitar][skill]")
{
    TestFixture f;
    f.state.setProperty("skillLevel", (int)SkillLevel::HARD, nullptr);

    SECTION("HARD HOPO modifier + HARD_GREEN → HOPO_GHOST")
    {
        f.addModifier((uint)Guitar::HARD_HOPO, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Guitar::HARD_GREEN, PPQ(2.0), PPQ(3.0));

        Gem result = GemCalculator::getGuitarGemType(
            (uint)Guitar::HARD_GREEN, PPQ(2.0), f.state, f.array, f.lock);
        REQUIRE(result == Gem::HOPO_GHOST);
    }
}
