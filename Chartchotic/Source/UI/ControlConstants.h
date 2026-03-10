#pragma once

#include "../Visual/Utils/DrawingConstants.h"

//==============================================================================
// Control Ranges, Steps, and Defaults
// Single source of truth for all user-facing control parameters.
//==============================================================================

// Note Speed
constexpr int NOTE_SPEED_MIN     = 2;
constexpr int NOTE_SPEED_MAX     = 20;
constexpr int NOTE_SPEED_DEFAULT = 7;

// Gem Scale (continuous %)
constexpr int GEM_SCALE_MIN_PCT     = 20;
constexpr int GEM_SCALE_MAX_PCT     = 200;
constexpr int GEM_SCALE_STEP_PCT    = 5;
constexpr int GEM_SCALE_DEFAULT_PCT = 100;

// Bar Scale (continuous %) — kicks, opens, etc.
constexpr int BAR_SCALE_MIN_PCT     = 20;
constexpr int BAR_SCALE_MAX_PCT     = 200;
constexpr int BAR_SCALE_STEP_PCT    = 5;
constexpr int BAR_SCALE_DEFAULT_PCT = 100;

// Texture Scale (continuous %)
constexpr int TEX_SCALE_MIN_PCT     = 25;
constexpr int TEX_SCALE_MAX_PCT     = 400;
constexpr int TEX_SCALE_STEP_PCT    = 25;
constexpr int TEX_SCALE_DEFAULT_PCT = 100;

// Texture Opacity
constexpr int TEX_OPACITY_MIN     = 0;
constexpr int TEX_OPACITY_MAX     = 100;
constexpr int TEX_OPACITY_STEP    = 5;
constexpr int TEX_OPACITY_DEFAULT = 100;

// Highway Length (UI percentages derived from FAR_FADE drawing constants)
constexpr int HWY_LENGTH_MIN_PCT     = (int)(FAR_FADE_MIN * 100);
constexpr int HWY_LENGTH_MAX_PCT     = (int)(FAR_FADE_MAX * 100);
constexpr int HWY_LENGTH_STEP_PCT    = 10;
constexpr int HWY_LENGTH_DEFAULT_PCT = (int)(FAR_FADE_DEFAULT * 100);

// Calibration / Sync Offset
constexpr int CALIBRATION_MIN_MS  = -200;
constexpr int CALIBRATION_MAX_MS  = 2000;
constexpr int CALIBRATION_STEP_MS = 20;
constexpr int CALIBRATION_DEFAULT = 0;

// Enum selectors (1-based, matching state storage)
// Framerate: 1=15FPS, 2=30FPS, 3=60FPS, 4=Native
constexpr int FRAMERATE_DEFAULT = 3; // 60 FPS

// Latency buffer: 1=250ms, 2=500ms, 3=750ms, 4=1000ms, 5=1500ms
constexpr int LATENCY_DEFAULT = 2; // 500ms

// HOPO: "autoHopo" (bool) + "hopoThreshold" (0-based index: 0=1/16, 1=Dot 1/16, 2=170 Tick, 3=1/8)
constexpr int HOPO_THRESHOLD_DEFAULT = 2; // "170 Tick"

// Toggle defaults (true = on)
constexpr bool DEFAULT_SHOW_GEMS        = true;
constexpr bool DEFAULT_SHOW_BARS        = true;
constexpr bool DEFAULT_SHOW_SUSTAINS    = true;
constexpr bool DEFAULT_SHOW_LANES       = true;
constexpr bool DEFAULT_SHOW_GRIDLINES   = true;
constexpr bool DEFAULT_SHOW_HIT_INDICATORS = true;
constexpr bool DEFAULT_SHOW_STAR_POWER  = true;
constexpr bool DEFAULT_SHOW_TRACK       = true;
constexpr bool DEFAULT_SHOW_STRIKELINE  = true;
constexpr bool DEFAULT_SHOW_LANE_SEPARATORS = true;
constexpr bool DEFAULT_SHOW_HIGHWAY     = true;
constexpr bool DEFAULT_SHOW_FPS         = false;
constexpr bool DEFAULT_AUTO_HOPO        = true;
constexpr bool DEFAULT_KICK_2X          = true;
constexpr bool DEFAULT_DYNAMICS         = true;
constexpr bool DEFAULT_CYMBALS          = true; // PRO drums

//==============================================================================
// Menu Enums (1-based, matching state storage)

enum class Part { GUITAR = 1, DRUMS, REAL_DRUMS };
enum class DrumType { NORMAL = 1, PRO };
enum class SkillLevel { EASY = 1, MEDIUM, HARD, EXPERT };
