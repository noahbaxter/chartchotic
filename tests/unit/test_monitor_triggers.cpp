#include "test_helpers.h"
#include <catch2/catch_approx.hpp>
#include "Audio/MonitorTriggers.h"

namespace
{
    struct Note
    {
        PPQ startPPQ;
        PPQ endPPQ;
        uint pitch;
        uint velocity = 100;
        bool muted = false;
    };

    Note note(double qn, uint pitch, uint velocity = 100, double len = 0.1)
    {
        return { PPQ(qn), PPQ(qn + len), pitch, velocity };
    }

    using D  = MidiPitchDefinitions::Drums;
    using ED = MidiPitchDefinitions::EliteDrums;
}

TEST_CASE("Monitor - pro drums: cymbal unless a tom marker covers it", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)D::EXPERT_KICK),
        note(1.0, (uint)D::EXPERT_RED, (uint)Dynamic::GHOST),
        note(2.0, (uint)D::EXPERT_YELLOW),
        note(3.0, (uint)D::EXPERT_YELLOW),
        note(2.5, (uint)D::TOM_YELLOW, 100, 1.0),   // covers 3.0, not 2.0
        note(4.0, (uint)D::EXPERT_BLUE),
        note(5.0, (uint)D::EXPERT_GREEN),
    };

    auto t = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, true);

    REQUIRE(t.size() == 6);
    REQUIRE(t[0].sample == MonitorSample::Kick);
    REQUIRE(t[1].sample == MonitorSample::Snare);
    REQUIRE(t[1].gain < t[0].gain);
    REQUIRE(t[2].sample == MonitorSample::HatClosed);
    REQUIRE(t[3].sample == MonitorSample::Tom1);
    REQUIRE(t[4].sample == MonitorSample::Ride);
    REQUIRE(t[5].sample == MonitorSample::Crash);
}

TEST_CASE("Monitor - without pro drums every pad is a tom", "[monitor]")
{
    std::vector<Note> notes = { note(0.0, (uint)D::EXPERT_YELLOW), note(1.0, (uint)D::EXPERT_GREEN) };
    auto t = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, false, true);
    REQUIRE(t.size() == 2);
    REQUIRE(t[0].sample == MonitorSample::Tom1);
    REQUIRE(t[1].sample == MonitorSample::Tom3);
}

TEST_CASE("Monitor - other difficulties stay silent", "[monitor]")
{
    std::vector<Note> notes = { note(0.0, (uint)D::HARD_RED) };
    REQUIRE(MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, true).empty());
}

TEST_CASE("Monitor - ghosts are quieter and accents louder than a normal hit", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)D::EXPERT_RED, (uint)Dynamic::GHOST),
        note(1.0, (uint)D::EXPERT_RED),
        note(2.0, (uint)D::EXPERT_RED, (uint)Dynamic::ACCENT),
    };

    auto on = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, true);
    REQUIRE(on.size() == 3);
    REQUIRE(on[0].gain < on[1].gain);
    REQUIRE(on[1].gain < on[2].gain);

    // Without chart dynamics the highway draws all three the same, so they sound the same.
    auto off = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, false);
    REQUIRE(off[0].gain == off[1].gain);
    REQUIRE(off[2].gain == off[1].gain);
}

TEST_CASE("Monitor - a 4-lane kick ignores its velocity", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)D::EXPERT_KICK, (uint)Dynamic::GHOST),
        note(1.0, (uint)D::EXPERT_KICK),
    };
    auto t = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, true);
    REQUIRE(t.size() == 2);
    REQUIRE(t[0].gain == t[1].gain);
}

TEST_CASE("Monitor - an accented hat opens and an accented ride rings the bell", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)D::EXPERT_YELLOW, (uint)Dynamic::ACCENT),
        note(1.0, (uint)D::EXPERT_BLUE,   (uint)Dynamic::ACCENT),
        note(2.0, (uint)D::EXPERT_GREEN,  (uint)Dynamic::ACCENT),
        note(3.0, (uint)D::EXPERT_YELLOW, (uint)Dynamic::ACCENT),
        note(3.0, (uint)D::TOM_YELLOW, 100, 0.5),   // an accented tom stays a tom
    };
    auto t = MonitorTriggerBuilder::drums(notes, SkillLevel::EXPERT, false, true, true);
    REQUIRE(t.size() == 4);
    REQUIRE(t[0].sample == MonitorSample::HatOpen);
    REQUIRE(t[1].sample == MonitorSample::RideBell);
    REQUIRE(t[2].sample == MonitorSample::Crash);
    REQUIRE(t[3].sample == MonitorSample::Tom1);
}

TEST_CASE("Monitor - elite hi-hat is open unless the pedal is down", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)ED::EXPERT_HIHAT),
        note(1.0, (uint)ED::EXPERT_HIHAT),
        note(1.0, (uint)ED::EXPERT_PEDAL),
        note(2.0, (uint)ED::EXPERT_TOM1),
        note(3.0, (uint)ED::EXPERT_KICK),
    };

    auto t = MonitorTriggerBuilder::eliteDrums(notes, SkillLevel::EXPERT, false, true);

    REQUIRE(t.size() == 4);
    REQUIRE(t[0].sample == MonitorSample::HatOpen);
    REQUIRE(t[1].sample == MonitorSample::HatClosed);
    REQUIRE(t[2].sample == MonitorSample::Tom1);
    REQUIRE(t[3].sample == MonitorSample::Kick);
}

TEST_CASE("Monitor - a held elite pedal closes every hat under it", "[monitor]")
{
    std::vector<Note> notes = {
        note(0.0, (uint)ED::EXPERT_PEDAL, 1, 2.0),   // held from 0 to 2
        note(0.0, (uint)ED::EXPERT_HIHAT),
        note(1.0, (uint)ED::EXPERT_HIHAT),
        note(2.0, (uint)ED::EXPERT_HIHAT),           // pedal released: open
        note(3.0, (uint)ED::EXPERT_RIDE, (uint)Dynamic::ACCENT),
    };

    auto t = MonitorTriggerBuilder::eliteDrums(notes, SkillLevel::EXPERT, false, true);

    REQUIRE(t.size() == 4);
    REQUIRE(t[0].sample == MonitorSample::HatClosed);
    REQUIRE(t[1].sample == MonitorSample::HatClosed);
    REQUIRE(t[2].sample == MonitorSample::HatOpen);
    REQUIRE(t[3].sample == MonitorSample::RideBell);
}

TEST_CASE("Monitor - fire window plays each hit once across blocks", "[monitor]")
{
    MonitorFireWindow w;

    // A tempo change makes the first block's projected end run past where the next one starts.
    REQUIRE(w.begin(0.0, 1.05) == Catch::Approx(0.0));
    REQUIRE(w.begin(1.0, 2.0)  == Catch::Approx(1.05));   // [1.0, 1.05) already fired

    // Falling short leaves a gap, which the next block picks up.
    REQUIRE(w.begin(2.1, 3.1)  == Catch::Approx(2.0));

    // A seek or a loop back starts fresh.
    REQUIRE(w.begin(40.0, 41.0) == Catch::Approx(40.0));
    REQUIRE(w.begin(0.0, 1.0)   == Catch::Approx(0.0));

    // So does starting again after a stop.
    w.stop();
    REQUIRE(w.begin(1.5, 2.5) == Catch::Approx(1.5));
}

TEST_CASE("Monitor - a guitar chord clicks once", "[monitor]")
{
    using G = MidiPitchDefinitions::Guitar;
    std::vector<Note> notes = {
        note(0.0, (uint)G::EXPERT_GREEN),
        note(0.0, (uint)G::EXPERT_RED),
        note(1.0, (uint)G::EXPERT_YELLOW),
    };

    auto t = MonitorTriggerBuilder::guitar(notes, SkillLevel::EXPERT);
    REQUIRE(t.size() == 2);
    REQUIRE(t[0].qn == Catch::Approx(0.0));
    REQUIRE(t[1].qn == Catch::Approx(1.0));
}

TEST_CASE("Monitor - metronome clicks bars and beats off the tempo map", "[monitor]")
{
    TempoTimeSignatureMap map;
    map[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4, true);

    auto t = MonitorTriggerBuilder::metronome(map, 0.0, 8.0);

    int bars = 0, beats = 0;
    for (const auto& c : t)
    {
        if (c.sample == MonitorSample::Bar)  ++bars;
        if (c.sample == MonitorSample::Beat) ++beats;
    }
    REQUIRE(bars == 2);    // 0, 4
    REQUIRE(beats == 6);   // 1, 2, 3, 5, 6, 7
    REQUIRE(t.front().sample == MonitorSample::Bar);
}
