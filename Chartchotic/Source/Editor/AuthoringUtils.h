#pragma once

#include "AuthoringTypes.h"
#include "../Midi/Utils/TimeConverter.h"   // TempoTimeSignatureMap, which the helper assumes
#include "../Midi/Utils/TempoTimeSignatureEventHelper.h"

// The time signature stretch `qn` sits in, read the way the gridlines read it: anchored at the
// last time signature reset, ending at the next, measuring bars by the signature at the anchor.
inline GridSection gridSectionFor(const TempoTimeSignatureMap& map, double qn)
{
    GridSection section;
    section.anchorQN = TempoTimeSignatureEventHelper::getLastTimeSigResetPosition(map, PPQ(qn)).toDouble();
    if (auto end = TempoTimeSignatureEventHelper::getNextTimeSigResetPosition(map, PPQ(qn)))
        section.endQN = end->toDouble();
    const auto ts = TempoTimeSignatureEventHelper::getEffectiveStateAtPPQ(map, PPQ(section.anchorQN));
    if (ts.timeSigNumerator > 0 && ts.timeSigDenominator > 0)
        section.barQN = ts.timeSigNumerator * (4.0 / ts.timeSigDenominator);
    return section;
}

inline double snapQN(double rawQN, int stepDivision, int tuplet, bool snapEnabled,
                     GridSection section = {})
{
    return snapEnabled
        ? snapToStep(rawQN, stepDivision, tuplet, section)
        : rawQN;
}
