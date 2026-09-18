/*
  ==============================================================================

    DifficultyWindow.h
    Interpreted output for one difficulty — TrackWindow + SustainWindow.

  ==============================================================================
*/

#pragma once

#include "../../Utils/ChartTypes.h"

struct DifficultyWindow {
    TrackWindow trackWindow;
    SustainWindow sustainWindow;

    // Drum fill activation bands. Every other phrase comes from its markers alone and so belongs
    // to the part, but a fill's band sits on the lane of the note that activates star power, and
    // that note differs per difficulty: green on Expert can be red on Hard.
    SustainWindow phraseWindow;
};
