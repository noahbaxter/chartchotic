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
};
