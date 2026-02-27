#include "test_helpers.h"
#include "Midi/Processing/MidiInterpreter.h"

using Guitar = MidiPitchDefinitions::Guitar;
using Drums = MidiPitchDefinitions::Drums;

// ============================================================================
// generateTrackWindow — basic note rendering (regressions #1, #6)

TEST_CASE("MidiInterpreter - basic note rendering", "[midi_interpreter][track_window]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("single expert green at PPQ 2.0 → column 1 populated")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw.count(PPQ(2.0)) == 1);
        REQUIRE(tw[PPQ(2.0)][1].gem == Gem::NOTE); // GREEN = column 1
        // Other columns should be NONE
        REQUIRE(tw[PPQ(2.0)][0].gem == Gem::NONE);
        REQUIRE(tw[PPQ(2.0)][2].gem == Gem::NONE);
        REQUIRE(tw[PPQ(2.0)][3].gem == Gem::NONE);
        REQUIRE(tw[PPQ(2.0)][4].gem == Gem::NONE);
        REQUIRE(tw[PPQ(2.0)][5].gem == Gem::NONE);
    }

    SECTION("note outside window → not in output")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(5.0), PPQ(6.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw.count(PPQ(5.0)) == 0);
    }

    SECTION("note-off event (velocity 0) → skipped")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        // The note-off is at max(2.0+1tick, 3.0-1tick) — should not create a frame
        // Only one frame should exist for the note-on
        int noteOnFrames = 0;
        for (auto& [ppq, frame] : tw)
        {
            if (frame[1].gem != Gem::NONE) noteOnFrames++;
        }
        REQUIRE(noteOnFrames == 1);
    }

    SECTION("guitar chord → both columns populated in same frame")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw.count(PPQ(2.0)) == 1);
        REQUIRE(tw[PPQ(2.0)][1].gem == Gem::NOTE); // GREEN
        REQUIRE(tw[PPQ(2.0)][2].gem == Gem::NOTE); // RED
    }
}

// ============================================================================
// Star power propagation (regression #6)
// Bug: old system checked isNoteHeld at render time for SP, but data didn't carry SP state

TEST_CASE("MidiInterpreter - star power propagation", "[midi_interpreter][star_power]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("SP modifier held across note → starPower = true")
    {
        f.addModifier((uint)Guitar::SP, PPQ(1.0), PPQ(5.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(6.0));
        REQUIRE(tw[PPQ(2.0)][1].starPower == true);
    }

    SECTION("SP modifier NOT held → starPower = false")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][1].starPower == false);
    }

    SECTION("SP modifier ends before note → starPower = false")
    {
        f.addModifier((uint)Guitar::SP, PPQ(0.0), PPQ(1.5));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][1].starPower == false);
    }
}

// ============================================================================
// Modifier notes don't appear as playable notes (regression #1)
// Bug: modifier pitches routed to getGuitarColumn → defaulted to orange

TEST_CASE("MidiInterpreter - modifier pitches not rendered as notes", "[midi_interpreter]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("HOPO/STRUM/TAP/SP modifier pitches → NOT rendered as glyphs")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::EXPERT_STRUM, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::TAP, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::SP, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::LANE_1, PPQ(1.0), PPQ(5.0));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(6.0));

        // No frames should have any visible notes (modifiers should be invisible)
        for (auto& [ppq, frame] : tw)
        {
            for (uint col = 0; col < LANE_COUNT; col++)
            {
                REQUIRE(frame[col].gem == Gem::NONE);
            }
        }
    }
}

// ============================================================================
// Drum mode

TEST_CASE("MidiInterpreter - drum mode", "[midi_interpreter][drums]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("expert kick → column 0")
    {
        f.addNote((uint)Drums::EXPERT_KICK, PPQ(2.0), PPQ(2.5));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][0].gem == Gem::NOTE);
    }

    SECTION("kick2x enabled, pitch 95 → column 6")
    {
        f.addNote((uint)Drums::EXPERT_KICK_2X, PPQ(2.0), PPQ(2.5));

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][6].gem == Gem::NOTE);
    }
}

// ============================================================================
// generateSustainWindow (regressions #6, #7)

TEST_CASE("MidiInterpreter - guitar sustains", "[midi_interpreter][sustains]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("long note produces sustain with correct column")
    {
        // Duration well above MIN_SUSTAIN_LENGTH
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));

        SustainWindow sw = interp.generateSustainWindow(PPQ(0.0), PPQ(4.0), PPQ(5.0));

        // Find sustain for column 1 (green)
        bool found = false;
        for (auto& s : sw)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
            {
                found = true;
                REQUIRE(s.startPPQ == PPQ(1.0));
            }
        }
        REQUIRE(found);
    }

    SECTION("short note (below MIN_SUSTAIN_LENGTH) → no sustain")
    {
        // Very short note
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.1));

        SustainWindow sw = interp.generateSustainWindow(PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool foundSustain = false;
        for (auto& s : sw)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
                foundSustain = true;
        }
        REQUIRE_FALSE(foundSustain);
    }

    SECTION("no note-off → sustain extends to latencyBufferEnd")
    {
        // Manually add just a note-on with no note-off
        f.array[(uint)Guitar::EXPERT_GREEN][PPQ(1.0)] = NoteData(100, Gem::NOTE);

        PPQ latencyEnd = PPQ(10.0);
        SustainWindow sw = interp.generateSustainWindow(PPQ(0.0), PPQ(8.0), latencyEnd);

        bool found = false;
        for (auto& s : sw)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
            {
                found = true;
                REQUIRE(s.endPPQ == latencyEnd);
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("MidiInterpreter - sustain star power", "[midi_interpreter][sustains][star_power]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("SP held at sustain start → starPower = true")
    {
        f.addModifier((uint)Guitar::SP, PPQ(0.5), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));

        SustainWindow sw = interp.generateSustainWindow(PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool found = false;
        for (auto& s : sw)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
            {
                found = true;
                REQUIRE(s.gemType.starPower == true);
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("MidiInterpreter - lane sustains", "[midi_interpreter][sustains][lanes]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("LANE_1 modifier with playable note → SustainType::LANE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.5), PPQ(2.5));
        // Lane modifier covering the note
        f.addModifier((uint)Guitar::LANE_1, PPQ(1.0), PPQ(3.0));

        SustainWindow sw = interp.generateSustainWindow(PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool foundLane = false;
        for (auto& s : sw)
        {
            if (s.sustainType == SustainType::LANE)
            {
                foundLane = true;
                REQUIRE(s.gemColumn == 1); // GREEN column
            }
        }
        REQUIRE(foundLane);
    }
}

TEST_CASE("MidiInterpreter - zero-length notes", "[midi_interpreter][regression_7]")
{
    TestFixture f;
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("0-tick note still appears in TrackWindow")
    {
        // Simulate a 0-tick note: start == end
        // addNote uses std::max(start + PPQ(1), end - PPQ(1)) for note-off
        // With start == end, note-off goes to start + PPQ(1), so note-on at start survives
        PPQ pos = PPQ(2.0);
        f.addNote((uint)Guitar::EXPERT_GREEN, pos, pos);

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw.count(pos) == 1);
        REQUIRE(tw[pos][1].gem == Gem::NOTE);
    }

    SECTION("1-tick note still appears in TrackWindow")
    {
        PPQ pos = PPQ(2.0);
        PPQ end = pos + PPQ(1); // 1 tick
        f.addNote((uint)Guitar::EXPERT_GREEN, pos, end);

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw.count(pos) == 1);
        REQUIRE(tw[pos][1].gem == Gem::NOTE);
    }
}

// ============================================================================
// Drum gem type passthrough — interpreter preserves pre-computed gem types

TEST_CASE("MidiInterpreter - drum gem type passthrough", "[midi_interpreter][drums]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);
    MidiInterpreter interp(f.state, f.array, f.lock);

    SECTION("HOPO_GHOST gem stored → HOPO_GHOST in track window")
    {
        f.addNote((uint)Drums::EXPERT_RED, PPQ(2.0), PPQ(2.5), (uint8_t)Dynamic::GHOST, Gem::HOPO_GHOST);

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][1].gem == Gem::HOPO_GHOST);
    }

    SECTION("CYM_ACCENT gem stored → CYM_ACCENT in track window")
    {
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5), (uint8_t)Dynamic::ACCENT, Gem::CYM_ACCENT);

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][2].gem == Gem::CYM_ACCENT);
    }

    SECTION("CYM gem stored → CYM in track window")
    {
        f.addNote((uint)Drums::EXPERT_BLUE, PPQ(2.0), PPQ(2.5), 100, Gem::CYM);

        TrackWindow tw = interp.generateTrackWindow(PPQ(0.0), PPQ(4.0));
        REQUIRE(tw[PPQ(2.0)][3].gem == Gem::CYM);
    }
}
