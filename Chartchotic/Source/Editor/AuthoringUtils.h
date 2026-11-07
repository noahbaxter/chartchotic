#pragma once

#include "AuthoringTypes.h"

inline double snapQN(double rawQN, int stepDivision, int tuplet, bool snapEnabled,
                     GridSection section = {})
{
    return snapEnabled
        ? snapToStep(rawQN, stepDivision, tuplet, section)
        : rawQN;
}
