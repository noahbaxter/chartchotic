#include "test_helpers.h"
#include "Midi/Utils/LaneDetector.h"

using Guitar = MidiPitchDefinitions::Guitar;
using Drums = MidiPitchDefinitions::Drums;

// ============================================================================
// Skill-level filtering

TEST_CASE("Lane detector - skill level filtering", "[lane_detector]")
{
    TestFixture f;
    f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));

    SECTION("EXPERT → lanes always apply")
    {
        f.state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }

    SECTION("HARD with velocity 41-50 → lanes apply")
    {
        f.state.setProperty("skillLevel", (int)SkillLevel::HARD, nullptr);
        f.addNote((uint)Guitar::HARD_GREEN, PPQ(1.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 45, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }

    SECTION("HARD with velocity outside 41-50 → empty")
    {
        f.state.setProperty("skillLevel", (int)SkillLevel::HARD, nullptr);
        f.addNote((uint)Guitar::HARD_GREEN, PPQ(1.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }

    SECTION("MEDIUM → always empty")
    {
        f.state.setProperty("skillLevel", (int)SkillLevel::MEDIUM, nullptr);
        f.addNote((uint)Guitar::MEDIUM_GREEN, PPQ(1.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }

    SECTION("EASY → always empty")
    {
        f.state.setProperty("skillLevel", (int)SkillLevel::EASY, nullptr);
        f.addNote((uint)Guitar::EASY_GREEN, PPQ(1.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }
}

// ============================================================================
// Lane column assignment

TEST_CASE("Lane detector - column assignment", "[lane_detector]")
{
    TestFixture f;

    SECTION("LANE_1 with one note → 1 lane event on that column")
    {
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.0), PPQ(2.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.0), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
        REQUIRE(lanes[0].gemColumn == 2); // RED = column 2
    }

    SECTION("LANE_2 with two notes → 2 lane events")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(2.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.5), PPQ(2.5));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_2, PPQ(0.5), PPQ(3.0), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 2);
    }

    SECTION("LANE_1 with zero notes in range → empty")
    {
        // Note is outside the lane time range
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(5.0), PPQ(6.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.0), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }

    SECTION("LANE_2 with one note → only 1 lane event")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(2.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_2, PPQ(0.5), PPQ(3.0), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }
}

// ============================================================================
// Output correctness

TEST_CASE("Lane detector - output properties", "[lane_detector]")
{
    TestFixture f;
    f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.5), PPQ(2.5));

    PPQ laneStart = PPQ(0.5);
    PPQ laneEnd = PPQ(3.0);

    auto lanes = LaneDetector::detectLanes(
        (uint)Guitar::LANE_1, laneStart, laneEnd, 100, f.state, f.array, f.lock);

    REQUIRE(lanes.size() == 1);

    SECTION("sustain PPQ matches modifier range, not the playable note")
    {
        REQUIRE(lanes[0].startPPQ == laneStart);
        REQUIRE(lanes[0].endPPQ == laneEnd);
    }

    SECTION("sustain type is LANE")
    {
        REQUIRE(lanes[0].sustainType == SustainType::LANE);
    }
}

// ============================================================================
// Lane note count caps

TEST_CASE("Lane detector - note count caps", "[lane_detector]")
{
    TestFixture f;

    SECTION("LANE_1 with 3 notes → only 1 lane event (capped)")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(2.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.5), PPQ(2.5));
        f.addNote((uint)Guitar::EXPERT_YELLOW, PPQ(2.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }

    SECTION("LANE_2 with 3 notes → only 2 lane events (capped)")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(2.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(1.5), PPQ(2.5));
        f.addNote((uint)Guitar::EXPERT_YELLOW, PPQ(2.0), PPQ(3.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_2, PPQ(0.5), PPQ(3.5), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 2);
    }
}

// ============================================================================
// HARD velocity boundaries

TEST_CASE("Lane detector - HARD velocity boundaries", "[lane_detector]")
{
    TestFixture f;
    f.state.setProperty("skillLevel", (int)SkillLevel::HARD, nullptr);
    f.addNote((uint)Guitar::HARD_GREEN, PPQ(1.0), PPQ(3.0));

    SECTION("velocity 41 (minimum) → lane applies")
    {
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 41, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }

    SECTION("velocity 40 (below minimum) → empty")
    {
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 40, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }

    SECTION("velocity 50 (maximum) → lane applies")
    {
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 50, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
    }

    SECTION("velocity 51 (above maximum) → empty")
    {
        auto lanes = LaneDetector::detectLanes(
            (uint)Guitar::LANE_1, PPQ(0.5), PPQ(3.5), 51, f.state, f.array, f.lock);
        REQUIRE(lanes.empty());
    }
}

// ============================================================================
// Drums lane detection

TEST_CASE("Lane detector - drums", "[lane_detector][drums]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);

    SECTION("drum note within lane range → lane detected")
    {
        f.addNote((uint)Drums::EXPERT_RED, PPQ(1.0), PPQ(2.0));
        auto lanes = LaneDetector::detectLanes(
            (uint)Drums::LANE_1, PPQ(0.5), PPQ(3.0), 100, f.state, f.array, f.lock);
        REQUIRE(lanes.size() == 1);
        REQUIRE(lanes[0].gemColumn == 1); // RED = column 1
    }
}
