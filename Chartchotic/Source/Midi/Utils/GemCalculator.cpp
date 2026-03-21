#include "GemCalculator.h"

Gem GemCalculator::resolveDrumGem(bool cymbal, bool dynamicsEnabled, Dynamic dynamic)
{
    if (!dynamicsEnabled)           return cymbal ? Gem::CYM        : Gem::NOTE;
    switch (dynamic)
    {
        case Dynamic::GHOST:        return cymbal ? Gem::CYM_GHOST  : Gem::HOPO_GHOST;
        case Dynamic::ACCENT:       return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
        default:                    return cymbal ? Gem::CYM        : Gem::NOTE;
    }
}

Gem GemCalculator::resolveGuitarGem(bool isChord, bool autoHOPO,
                                    bool hopoForced, bool strumForced, bool tapForced)
{
    // Priority: strum force > tap > HOPO force > chord default > auto-HOPO > strum default
    if (strumForced)                return Gem::NOTE;
    if (tapForced)                  return Gem::TAP_ACCENT;
    if (hopoForced)                return Gem::HOPO_GHOST;
    if (isChord)                    return Gem::NOTE;
    if (autoHOPO)                   return Gem::HOPO_GHOST;
    return Gem::NOTE;
}
