/*
  ==============================================================================

    GemCalculator.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    Pure gem type resolvers — no locks, no noteStateMapArray access.
    Used by TrackResolver with pre-extracted modifier state.

  ==============================================================================
*/

#pragma once

#include "MidiTypes.h"
#include "../../Utils/Utils.h"

class GemCalculator
{
public:
    static Gem resolveGuitarGem(bool isChord, bool autoHOPO,
                                bool hopoForced, bool strumForced, bool tapForced);
    static Gem resolveDrumGem(bool cymbal, bool dynamicsEnabled, Dynamic dynamic);
};
