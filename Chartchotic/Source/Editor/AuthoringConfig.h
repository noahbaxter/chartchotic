/*
    ==============================================================================

        AuthoringConfig.h
        Author: Noah Baxter

        Per-instrument write-mode data, the authoring twin of
        PositionConstants::RenderTypeConfig. Replaces the isDrums/isElite
        branches and hardcoded lane numbers that were spread across
        AuthoringControllerBase, WriteController and EditController with one
        const-pointer indirection: `cfg->X`.

        Adding an instrument should mean adding a table entry here, not hunting
        through Editor/ for every place that assumed four lanes.

    ==============================================================================
*/

#pragma once

#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/InstrumentMapper.h"

// Ranges over time that still need a lane, which is why they are authored on the highway.
// Lane1/Lane2 are the MIDI names: on guitar they read as tremolo and trill, on drums as
// 1-lane and 2-lane rolls. Star power, solo and fill have no lane and live in the strip.
enum class PhraseTarget { Lane1, Lane2 };

// Per-instrument authoring data. Static-lifetime singletons returned by
// getAuthoringConfig(). Data and small pure functions only; genuine behaviour
// branches stay in the controllers.
struct AuthoringConfig
{
    // Highest authorable lane index. Elite's 2x kick is a virtual column ABOVE its hand
    // lanes (9), while 4-lane's sits inside the range (6), so both forms are stored.
    int highestLane;        // 2x kick disabled
    int highestLaneKick2x;  // 2x kick enabled

    // Lane the 2x kick lives on, or -1 where the instrument has none.
    int kick2xColumn;

    // Whether the Cym modifier means anything. Elite fixes cymbal-ness by lane, so its
    // toggle is inert and the write sub-toolbar omits the slot.
    bool hasCymbalToggle;

    // Whether the Flam modifier means anything. Elite only: no other kit notates flams.
    bool hasFlamToggle;

    // Whether a roll lane carries its own column. Elite has one lane pitch per pad; everyone
    // else shares one pair of pitches whose columns the game infers from the notes under the
    // marker. See LaneMath.h for what each shape means for authoring.
    bool lanesPerColumn;

    // Lane the user clicked -> pitch to write. Handles this instrument's kicks itself, so
    // callers never need a kick special case. Returns -1 for an unauthorable lane.
    int (*laneToPitch)(SkillLevel skill, int lane, bool kick2xEnabled);

    // Phrase pitch for this target, or -1 where the instrument has no such phrase. Elite's
    // roll lanes are per column, so `lane` matters there and is ignored everywhere else.
    int (*phrasePitch)(PhraseTarget target, SkillLevel skill, int lane);

    // Kick exclusivity: whether `pitch` is one of this instrument's kicks at this skill,
    // and which kick it conflicts with. Both are no-ops where kick2xColumn is -1.
    bool (*isKickPitch)(uint pitch, SkillLevel skill);
    InstrumentMapper::KickConflict (*conflictingKick)(int pitch, SkillLevel skill);
};

// Returns nullptr for render types that aren't authorable. Callers are gated upstream by
// write mode only being offered for authorable parts.
const AuthoringConfig* getAuthoringConfig(RenderType type);

// Convenience for the common `getAuthoringConfig(getRenderType(part))` pair.
inline const AuthoringConfig* getAuthoringConfig(Part part)
{
    return getAuthoringConfig(getRenderType(part));
}
