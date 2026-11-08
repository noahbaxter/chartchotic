#include "test_helpers.h"
#include "Editor/AuthoringConfig.h"
#include "Editor/AuthoringTypes.h"
#include "Editor/LaneMath.h"
#include "Editor/ModifierSlots.h"
#include "Midi/Utils/MidiConstants.h"

using EliteDrums = MidiPitchDefinitions::EliteDrums;
using Drums = MidiPitchDefinitions::Drums;
using Guitar = MidiPitchDefinitions::Guitar;

TEST_CASE("AuthoringConfig - lookup", "[authoring_config]")
{
    SECTION("the three authorable render types resolve")
    {
        REQUIRE(getAuthoringConfig(RenderType::FIVE_FRET) != nullptr);
        REQUIRE(getAuthoringConfig(RenderType::FOUR_LANE_DRUMS) != nullptr);
        REQUIRE(getAuthoringConfig(RenderType::ELITE_DRUMS) != nullptr);
    }

    SECTION("unauthorable render types return null")
    {
        REQUIRE(getAuthoringConfig(RenderType::VOCALS) == nullptr);
        REQUIRE(getAuthoringConfig(RenderType::PRO_KEYS) == nullptr);
    }

    SECTION("the Part overload agrees with the RenderType one")
    {
        REQUIRE(getAuthoringConfig(Part::ELITE_DRUMS) == getAuthoringConfig(RenderType::ELITE_DRUMS));
        REQUIRE(getAuthoringConfig(Part::DRUMS) == getAuthoringConfig(RenderType::FOUR_LANE_DRUMS));
    }
}

TEST_CASE("AuthoringConfig - lane extents", "[authoring_config]")
{
    // 4-lane's 2x kick sits INSIDE the lane range (6) while elite's sits ABOVE its hand
    // lanes (9). A single shared expression can't produce both, which is what clamped
    // elite to 6 and hid Ride and R-Crash.
    SECTION("4-lane drums")
    {
        const auto* c = getAuthoringConfig(RenderType::FOUR_LANE_DRUMS);
        REQUIRE(c->highestLane == 4);
        REQUIRE(c->highestLaneKick2x == DRUM_KICK_2X_COLUMN);
        REQUIRE(c->kick2xColumn == DRUM_KICK_2X_COLUMN);
    }

    SECTION("elite drums reaches Ride and R-Crash")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->highestLane == 8);
        REQUIRE(c->highestLaneKick2x == ELITE_KICK_2X_COLUMN);
        REQUIRE(c->kick2xColumn == ELITE_KICK_2X_COLUMN);
    }

    SECTION("guitar has no kick")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->highestLane == 5);
        REQUIRE(c->kick2xColumn == -1);
    }
}

TEST_CASE("AuthoringConfig - laneToPitch", "[authoring_config]")
{
    SECTION("elite lane 6 is Tom 3, not a kick")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 6, true) == (int)EliteDrums::EXPERT_TOM3);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 0, true) == (int)EliteDrums::EXPERT_KICK);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN, true)
                == (int)EliteDrums::EXPERT_KICK_2X);
    }

    SECTION("elite reaches every hand lane")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        for (int lane = 0; lane <= 8; ++lane)
            REQUIRE(c->laneToPitch(SkillLevel::EXPERT, lane, true) >= 0);
    }

    SECTION("elite 2x kick is refused when the toggle is off")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN, false) == -1);
    }

    SECTION("4-lane column 6 IS the 2x kick")
    {
        const auto* c = getAuthoringConfig(RenderType::FOUR_LANE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, DRUM_KICK_2X_COLUMN, true)
                == (int)Drums::EXPERT_KICK_2X);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, DRUM_KICK_2X_COLUMN, false) == -1);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 1, false) == (int)Drums::EXPERT_RED);
    }

    SECTION("guitar lane 0 is the open bar")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 0, false) == (int)Guitar::EXPERT_OPEN);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 5, false) == (int)Guitar::EXPERT_ORANGE);
    }
}

TEST_CASE("AuthoringConfig - kick conflicts", "[authoring_config]")
{
    SECTION("elite pairs its own kicks and ignores toms")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->isKickPitch((uint)EliteDrums::EXPERT_KICK, SkillLevel::EXPERT));
        REQUIRE(c->isKickPitch((uint)EliteDrums::EXPERT_TOM3, SkillLevel::EXPERT) == false);

        auto conflict = c->conflictingKick((int)EliteDrums::EXPERT_KICK, SkillLevel::EXPERT);
        REQUIRE(conflict.pitch == (int)EliteDrums::EXPERT_KICK_2X);
        REQUIRE(conflict.lane == ELITE_KICK_2X_COLUMN);
    }

    SECTION("guitar has no kick to conflict with")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->isKickPitch((uint)Guitar::EXPERT_GREEN, SkillLevel::EXPERT) == false);
        REQUIRE(c->conflictingKick((int)Guitar::EXPERT_GREEN, SkillLevel::EXPERT).lane == -1);
    }
}

TEST_CASE("AuthoringConfig - cymbal toggle", "[authoring_config]")
{
    // Elite fixes cymbal-ness by lane, so its toggle is inert and the sub-toolbar omits it.
    REQUIRE(getAuthoringConfig(RenderType::FOUR_LANE_DRUMS)->hasCymbalToggle);
    REQUIRE(getAuthoringConfig(RenderType::ELITE_DRUMS)->hasCymbalToggle == false);
    REQUIRE(getAuthoringConfig(RenderType::FIVE_FRET)->hasCymbalToggle == false);
}

TEST_CASE("AuthoringConfig - roll lane pitches", "[authoring_config]")
{
    // These go into real charts, so they are pinned to the spec rather than to whatever the
    // mapper happens to return. Sources: _refs/midi/drums.md:76-77,
    // _refs/midi/5-fret-guitar.md:21-22, _refs/midi/elite-drums.md:153-163.

    SECTION("guitar and 4-lane share one pair, and it ignores the column")
    {
        for (auto type : { RenderType::FIVE_FRET, RenderType::FOUR_LANE_DRUMS })
        {
            const auto* c = getAuthoringConfig(type);
            REQUIRE(c->lanesPerColumn == false);
            REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, 0) == 126);
            REQUIRE(c->phrasePitch(PhraseTarget::Lane2, SkillLevel::EXPERT, 0) == 127);

            // No column of its own: the game reads the columns off the notes underneath.
            REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, 3) == 126);
        }
    }

    SECTION("every lane pitch is difficulty-independent")
    {
        // Lanes are Expert-only unless their velocity says otherwise; there is no per-skill
        // pitch anywhere, so authoring on Hard must not shift the marker.
        const auto* c = getAuthoringConfig(RenderType::FOUR_LANE_DRUMS);
        for (auto skill : { SkillLevel::EASY, SkillLevel::MEDIUM,
                            SkillLevel::HARD, SkillLevel::EXPERT })
            REQUIRE(c->phrasePitch(PhraseTarget::Lane1, skill, 0) == 126);
    }

    SECTION("elite carries the column in the pitch, one lane per pad")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->lanesPerColumn);

        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, 0) == (int)EliteDrums::ROLL_KICK);
        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, 1) == (int)EliteDrums::ROLL_SNARE);
        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, ELITE_HIHAT_COLUMN)
                == (int)EliteDrums::ROLL_HIHAT);
        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, 8) == (int)EliteDrums::ROLL_RCRASH);
        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, ELITE_STOMP_COLUMN)
                == (int)EliteDrums::ROLL_STOMP);
    }

    SECTION("elite has no 1-lane/2-lane variant, and no 2x kick lane")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->phrasePitch(PhraseTarget::Lane2, SkillLevel::EXPERT, 1) == -1);
        // 109 is explicitly unused: there are no separate 2x kick roll lanes.
        REQUIRE(c->phrasePitch(PhraseTarget::Lane1, SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN) == -1);
    }
}

TEST_CASE("LaneMath - a lane is writable at every grid setting", "[authoring_config]")
{
    // Every division the write bar offers, against every tuplet from none through 20. The bar
    // only exposes the odd ones, so the rest are here so that widening that list cannot quietly
    // reintroduce a grid a lane refuses to sit on.
    const int divisions[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };

    int finerThanSustainMinimum = 0;

    for (int tuplet = 0; tuplet <= 20; ++tuplet)
        for (int division : divisions)
        {
            const double spacing = stepSpacingQN(division, tuplet);
            REQUIRE(spacing > 0.0);

            // Built through PPQ, because that is what a note's position has been through by the
            // time a lane snaps to it. Septuplets do not divide 960 evenly and land wherever
            // rounding puts them, which is exactly why the rule is about notes, not arithmetic.
            const double startQN = PPQ(4.0).toDouble();
            const double endQN   = PPQ(4.0 + spacing).toDouble();

            INFO("division " << division << " tuplet " << tuplet << " spacing " << spacing);
            REQUIRE(LaneMath::isWritableLength(startQN, endQN));

            // The two positions must still be distinct once rounded to ticks, or "snap to the
            // next note" would have nowhere to go. The tightest case here is division 64 against
            // tuplet 19, which is 6 ticks apart, so there is real room left.
            REQUIRE(PPQ(endQN).toScaled() > PPQ(startQN).toScaled());

            if (spacing < double(MIDI_MIN_SUSTAIN_LENGTH)) ++finerThanSustainMinimum;
        }

    // The reason this rule cannot borrow the sustain minimum: most of the grid is finer than it,
    // plain 16ths included. Pinned so that swapping the rule back fails loudly here.
    REQUIRE(finerThanSustainMinimum > 0);
    REQUIRE(stepSpacingQN(16, 0) < double(MIDI_MIN_SUSTAIN_LENGTH));

    // Still refuses a lane that does not span two distinct positions.
    REQUIRE_FALSE(LaneMath::isWritableLength(4.0, 4.0));
    REQUIRE_FALSE(LaneMath::isWritableLength(4.0, 4.0 + GridMath::TICK / 4.0));
}

TEST_CASE("LaneMath - a marker reaches as far as its own kind", "[authoring_config]")
{
    // Which kind is picked on the write bar, not worked out from the drag. Two rounds of
    // guessing failed: counting struck columns made a plain roll a trill whenever a neighbour
    // had a note in range, and counting sideways travel breaks on chord tremolos, which cover
    // several columns and are still tremolos (_refs/midi/5-fret-guitar.md:105). Reach follows
    // the kind chosen, which is all the geometry a marker has.
    REQUIRE(LaneMath::reachOf(PhraseTarget::Lane1) == 1u);
    REQUIRE(LaneMath::reachOf(PhraseTarget::Lane2) == 2u);
}

TEST_CASE("LaneMath - only the shared markers have a kind to pick", "[authoring_config]")
{
    // Elite has one roll-lane pitch per pad and no 1-vs-2 variant, so its write bar offers no
    // choice and the columns a drag reaches are the whole answer.
    REQUIRE(modifierSlotsFor(RenderType::FIVE_FRET, true).size() == 2);
    REQUIRE(modifierSlotsFor(RenderType::FOUR_LANE_DRUMS, true).size() == 2);
    REQUIRE(modifierSlotsFor(RenderType::ELITE_DRUMS, true).empty());

    // And with the tool off the slots are the ordinary note types again.
    REQUIRE(modifierSlotsFor(RenderType::FIVE_FRET, false).size() == 4);
}

TEST_CASE("Tuplets - odd values 3 to 19, cycled in order and wrapping", "[authoring_config]")
{
    for (int t = 0; t <= 20; ++t)
        REQUIRE(isTuplet(t) == (t >= 3 && t % 2 == 1 && t <= 19));

    REQUIRE(nextTuplet(0)  == 3);    // off turns on
    REQUIRE(nextTuplet(3)  == 5);
    REQUIRE(nextTuplet(17) == 19);
    REQUIRE(nextTuplet(19) == 3);
    REQUIRE(nextTuplet(4)  == 3);    // not a tuplet, so start over
}
