#include "test_helpers.h"
#include "Midi/Utils/ChordAnalyzer.h"

using Guitar = MidiPitchDefinitions::Guitar;

// ============================================================================
// fixChordHOPOs (regression #2)
// Bug: chord HOPO fixing happened per-note as notes arrived, not after all
// notes at a position were inserted

TEST_CASE("fixChordHOPOs", "[chord_analyzer]")
{
    TestFixture f;

    SECTION("two notes at same PPQ, both HOPO_GHOST, no forced modifier → both become NOTE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0)};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::NOTE);
        REQUIRE(f.array[(uint)Guitar::EXPERT_RED][PPQ(2.0)].gemType == Gem::NOTE);
    }

    SECTION("two notes, forced HOPO modifier held → stay HOPO_GHOST")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0)};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::HOPO_GHOST);
        REQUIRE(f.array[(uint)Guitar::EXPERT_RED][PPQ(2.0)].gemType == Gem::HOPO_GHOST);
    }

    SECTION("single note with HOPO_GHOST → stays HOPO_GHOST (not a chord)")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0)};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::HOPO_GHOST);
    }

    SECTION("three notes at same PPQ → all HOPOs become NOTE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_YELLOW, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0)};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::NOTE);
        REQUIRE(f.array[(uint)Guitar::EXPERT_RED][PPQ(2.0)].gemType == Gem::NOTE);
        REQUIRE(f.array[(uint)Guitar::EXPERT_YELLOW][PPQ(2.0)].gemType == Gem::NOTE);
    }

    SECTION("notes at different PPQs outside tolerance → HOPOs preserved")
    {
        PPQ separation = MIDI_CHORD_TOLERANCE + PPQ(0.05);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0) + separation, PPQ(3.0) + separation, 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0), PPQ(2.0) + separation};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::HOPO_GHOST);
        REQUIRE(f.array[(uint)Guitar::EXPERT_RED][PPQ(2.0) + separation].gemType == Gem::HOPO_GHOST);
    }

    SECTION("notes within chord tolerance but not exactly same PPQ → treated as chord")
    {
        // Slightly different PPQs but within tolerance
        PPQ offset = MIDI_CHORD_TOLERANCE - PPQ(0.001);
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0), 100, Gem::HOPO_GHOST);
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0) + offset, PPQ(3.0), 100, Gem::HOPO_GHOST);

        std::vector<PPQ> positions = {PPQ(2.0), PPQ(2.0) + offset};
        ChordAnalyzer::fixChordHOPOs(positions, SkillLevel::EXPERT, f.array, f.lock);

        REQUIRE(f.array[(uint)Guitar::EXPERT_GREEN][PPQ(2.0)].gemType == Gem::NOTE);
        REQUIRE(f.array[(uint)Guitar::EXPERT_RED][PPQ(2.0) + offset].gemType == Gem::NOTE);
    }
}

// ============================================================================
// isNoteHeld

TEST_CASE("isNoteHeld", "[chord_analyzer]")
{
    TestFixture f;

    SECTION("modifier ON at PPQ 1.0, query at PPQ 2.0 within sustain → true")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(3.0));

        bool held = ChordAnalyzer::isNoteHeld(
            (uint)Guitar::EXPERT_HOPO, PPQ(2.0), f.array, f.lock);
        REQUIRE(held == true);
    }

    SECTION("modifier ON at 1.0, OFF at 3.0, query at 4.0 → false")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(3.0));

        bool held = ChordAnalyzer::isNoteHeld(
            (uint)Guitar::EXPERT_HOPO, PPQ(4.0), f.array, f.lock);
        REQUIRE(held == false);
    }

    SECTION("query before any note exists → false")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(5.0), PPQ(8.0));

        bool held = ChordAnalyzer::isNoteHeld(
            (uint)Guitar::EXPERT_HOPO, PPQ(1.0), f.array, f.lock);
        REQUIRE(held == false);
    }

    SECTION("two note-on events, query between them → held from first")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(3.0));
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(5.0), PPQ(8.0));

        bool held = ChordAnalyzer::isNoteHeld(
            (uint)Guitar::EXPERT_HOPO, PPQ(2.0), f.array, f.lock);
        REQUIRE(held == true);

        // Between the two — after first note-off
        bool heldBetween = ChordAnalyzer::isNoteHeld(
            (uint)Guitar::EXPERT_HOPO, PPQ(4.0), f.array, f.lock);
        REQUIRE(heldBetween == false);
    }
}

// ============================================================================
// isNoteHeldWithTolerance

TEST_CASE("isNoteHeldWithTolerance", "[chord_analyzer]")
{
    TestFixture f;

    SECTION("note ON at 1.0, query at 1.0 + tolerance - epsilon → true")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));
        PPQ queryPos = PPQ(1.0) + MIDI_CHORD_TOLERANCE - PPQ(0.001);

        bool held = ChordAnalyzer::isNoteHeldWithTolerance(
            (uint)Guitar::EXPERT_GREEN, queryPos, f.array, f.lock);
        REQUIRE(held == true);
    }

    SECTION("query at 1.0 + tolerance + epsilon → false")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));
        // Query far enough that note-on at 1.0 is outside tolerance window
        PPQ queryPos = PPQ(1.0) + MIDI_CHORD_TOLERANCE + PPQ(0.01);

        bool held = ChordAnalyzer::isNoteHeldWithTolerance(
            (uint)Guitar::EXPERT_GREEN, queryPos, f.array, f.lock);
        REQUIRE(held == false);
    }
}

// ============================================================================
// isWithinChordTolerance

TEST_CASE("isWithinChordTolerance", "[chord_analyzer]")
{
    SECTION("same position → within tolerance")
    {
        REQUIRE(ChordAnalyzer::isWithinChordTolerance(PPQ(2.0), PPQ(2.0)) == true);
    }

    SECTION("just within tolerance → true")
    {
        PPQ a = PPQ(2.0);
        PPQ b = a + MIDI_CHORD_TOLERANCE;
        REQUIRE(ChordAnalyzer::isWithinChordTolerance(a, b) == true);
    }

    SECTION("just beyond tolerance → false")
    {
        PPQ a = PPQ(2.0);
        PPQ b = a + MIDI_CHORD_TOLERANCE + PPQ(0.01);
        REQUIRE(ChordAnalyzer::isWithinChordTolerance(a, b) == false);
    }

    SECTION("order doesn't matter")
    {
        PPQ a = PPQ(2.0);
        PPQ b = PPQ(2.005);
        REQUIRE(ChordAnalyzer::isWithinChordTolerance(a, b) ==
                ChordAnalyzer::isWithinChordTolerance(b, a));
    }
}
