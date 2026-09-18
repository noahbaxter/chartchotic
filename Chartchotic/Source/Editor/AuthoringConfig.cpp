/*
    ==============================================================================

        AuthoringConfig.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "AuthoringConfig.h"

namespace
{
    //==========================================================================
    // Guitar (5-fret): lane 0 is the open bar, 1-5 the frets. No kicks.

    int guitarLaneToPitch(SkillLevel skill, int lane, bool)
    {
        return InstrumentMapper::columnToGuitarPitch(skill, lane);
    }

    bool guitarNoKick(uint, SkillLevel) { return false; }

    InstrumentMapper::KickConflict guitarNoConflict(int, SkillLevel)
    {
        return { -1, -1 };
    }

    //==========================================================================
    // 4-lane drums: kick 0, pads 1-4, 2x kick on virtual column 6.

    int drumLaneToPitch(SkillLevel skill, int lane, bool kick2xEnabled)
    {
        if (InstrumentMapper::isKickLane(lane))
            return InstrumentMapper::resolveKickPitch(skill, lane, kick2xEnabled);
        return InstrumentMapper::columnToDrumPitch(skill, lane, false);
    }

    bool drumIsKickPitch(uint pitch, SkillLevel)
    {
        return InstrumentMapper::isDrumKick(pitch);
    }

    InstrumentMapper::KickConflict drumConflictingKick(int pitch, SkillLevel)
    {
        return InstrumentMapper::getConflictingKick(pitch);
    }

    //==========================================================================
    // Elite drums: kick 0, hand lanes 1-8, 2x kick on virtual column 9. Column 6 is
    // Tom 3, NOT a kick, which is the assumption that broke every 4-lane-shaped check.

    int eliteLaneToPitch(SkillLevel skill, int lane, bool kick2xEnabled)
    {
        if (lane == ELITE_KICK_2X_COLUMN && !kick2xEnabled) return -1;
        return InstrumentMapper::columnToEliteDrumPitch(skill, lane);
    }

    //==========================================================================

    // The two lane markers are shared by guitar and 4/5-lane drums.
    int sharedPhrasePitch(PhraseTarget target, SkillLevel, int)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (target)
        {
            case PhraseTarget::Lane1: return (int)Guitar::LANE_1;
            case PhraseTarget::Lane2: return (int)Guitar::LANE_2;
        }
        return -1;
    }

    // Elite has one roll lane per column instead of the shared 1-lane / 2-lane pair.
    int elitePhrasePitch(PhraseTarget target, SkillLevel, int lane)
    {
        switch (target)
        {
            case PhraseTarget::Lane1: return InstrumentMapper::eliteRollLanePitch(lane);
            case PhraseTarget::Lane2: return -1;
        }
        return -1;
    }

    //==========================================================================

    const AuthoringConfig guitarFiveFret = {
        5, 5, -1, false, false,
        &guitarLaneToPitch,
        &sharedPhrasePitch,
        &guitarNoKick,
        &guitarNoConflict,
    };

    const AuthoringConfig drumsFourLane = {
        4, DRUM_KICK_2X_COLUMN, DRUM_KICK_2X_COLUMN, true, false,
        &drumLaneToPitch,
        &sharedPhrasePitch,
        &drumIsKickPitch,
        &drumConflictingKick,
    };

    const AuthoringConfig eliteDrums = {
        8, ELITE_KICK_2X_COLUMN, ELITE_KICK_2X_COLUMN, false, true,
        &eliteLaneToPitch,
        &elitePhrasePitch,
        &InstrumentMapper::isEliteDrumKick,
        &InstrumentMapper::getConflictingEliteKick,
    };
}

const AuthoringConfig* getAuthoringConfig(RenderType type)
{
    switch (type)
    {
        case RenderType::FIVE_FRET:       return &guitarFiveFret;
        case RenderType::FOUR_LANE_DRUMS: return &drumsFourLane;
        case RenderType::ELITE_DRUMS:     return &eliteDrums;
        default:                          return nullptr;
    }
}
