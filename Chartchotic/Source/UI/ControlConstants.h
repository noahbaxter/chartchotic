#pragma once

#include "../Visual/Utils/DrawingConstants.h"

//==============================================================================
// Control Ranges, Steps, and Defaults
// Single source of truth for all user-facing control parameters.
//==============================================================================

// Note Speed
constexpr int NOTE_SPEED_MIN         = 2;
constexpr int NOTE_SPEED_MAX         = 20;
constexpr int NOTE_SPEED_DEFAULT     = 7;
constexpr int NOTE_SPEED_BEMANI_MIN  = 0;   // Bemani display range: 0-30
constexpr int NOTE_SPEED_BEMANI_MAX  = 30;
constexpr int NOTE_SPEED_BEMANI_DEFAULT = 12;
// Conversion factor: bemani flat viewport shows ~1.7x more readable area than perspective
constexpr float NOTE_SPEED_BEMANI_RATIO = 12.0f / 7.0f;

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
constexpr bool DEFAULT_DISCO_FLIP       = true;

//==============================================================================
// Menu Enums (1-based, matching state storage)

// Instrument types — organized by input family.
// Values are 1-based to match state storage.
enum class Part
{
    // 5-fret family (same MIDI pitch layout: 60-100 across 4 difficulties)
    GUITAR = 1,         // PART GUITAR (also covers PART GUITAR COOP, PART RHYTHM)
    BASS,               // PART BASS
    KEYS,               // PART KEYS (5-lane keys, uses guitar pitch mapping)

    // 6-fret / Guitar Hero Live (3 black + 3 white frets, different pitch layout)
    GHL_GUITAR,         // PART GUITAR GHL (also covers PART GUITAR COOP GHL, PART RHYTHM GHL)
    GHL_BASS,           // PART BASS GHL

    // Drums
    DRUMS,              // PART DRUMS (4-lane, 5-lane, Pro — variant via DrumType)
    ELITE_DRUMS,        // PART ELITE_DRUMS (separate track, 8-lane, 2 octaves/difficulty)

    // Vocals (pitch-based, not fret-based — completely different rendering)
    VOCALS,             // PART VOCALS
    HARMONIES,          // PART HARM1, PART HARM2, PART HARM3

    // Pro instruments (dedicated pitch layouts, not yet parseable)
    PRO_GUITAR,         // PART REAL_GUITAR, PART REAL_GUITAR_22
    PRO_BASS,           // PART REAL_BASS, PART REAL_BASS_22
    PRO_KEYS,           // PART REAL_KEYS_X/H/M/E (separate track per difficulty)
};

// How a Part renders on the highway — determines interpreter, lane count, gem style
enum class RenderType
{
    FIVE_FRET,          // 5-fret guitar/bass/keys (6 lanes: open + 5 frets)
    SIX_FRET,           // 6-fret GHL (3 black + 3 white)
    FOUR_LANE_DRUMS,    // Standard/Pro drums (kick + 4 pads/cymbals)
    FIVE_LANE_DRUMS,    // GH-style drums (kick + 3 pads + 2 cymbals)
    ELITE_DRUMS,        // 8-lane e-kit (kick + snare + 3 toms + 3 cymbals + pedal)
    VOCALS,             // Pitch-based vocal track
    PRO_GUITAR,         // 17/22-fret pro guitar/bass
    PRO_KEYS,           // 25-key two-octave keyboard
};

inline RenderType getRenderType(Part p)
{
    switch (p) {
        case Part::GUITAR:
        case Part::BASS:
        case Part::KEYS:         return RenderType::FIVE_FRET;
        case Part::GHL_GUITAR:
        case Part::GHL_BASS:     return RenderType::SIX_FRET;
        case Part::DRUMS:        return RenderType::FOUR_LANE_DRUMS;
        case Part::ELITE_DRUMS:  return RenderType::ELITE_DRUMS;
        case Part::VOCALS:
        case Part::HARMONIES:    return RenderType::VOCALS;
        case Part::PRO_GUITAR:
        case Part::PRO_BASS:     return RenderType::PRO_GUITAR;
        case Part::PRO_KEYS:     return RenderType::PRO_KEYS;
        default:                 return RenderType::FIVE_FRET;
    }
}

// Convenience — most code just needs "guitar-like or drum-like"
inline bool isGuitarLike(Part p) { return getRenderType(p) == RenderType::FIVE_FRET; }
inline bool isDrumLike(Part p)   { auto r = getRenderType(p); return r == RenderType::FOUR_LANE_DRUMS || r == RenderType::FIVE_LANE_DRUMS || r == RenderType::ELITE_DRUMS; }

// Can this Part be rendered as a scrolling highway? (Vocals, Pro Keys etc. need different rendering)
inline bool isHighwayRenderable(Part p) { return isGuitarLike(p) || isDrumLike(p); }

// Sort order: Guitar, guitar alts (GHL, Pro), Bass, bass alts, Keys, Drums, Vocals
inline int getPartSortOrder(Part p)
{
    switch (p) {
        case Part::GUITAR:      return 0;
        case Part::GHL_GUITAR:  return 1;
        case Part::PRO_GUITAR:  return 2;
        case Part::BASS:        return 3;
        case Part::GHL_BASS:    return 4;
        case Part::PRO_BASS:    return 5;
        case Part::KEYS:        return 6;
        case Part::PRO_KEYS:    return 7;
        case Part::DRUMS:       return 8;
        case Part::ELITE_DRUMS: return 9;
        case Part::VOCALS:      return 10;
        case Part::HARMONIES:   return 11;
        default:                return 99;
    }
}

// Short display name for Part (used in toggle labels, badges)
inline juce::String getPartDisplayName(Part p)
{
    switch (p) {
        case Part::GUITAR:      return "Guitar";
        case Part::BASS:        return "Bass";
        case Part::KEYS:        return "Keys";
        case Part::GHL_GUITAR:  return "GHL";
        case Part::GHL_BASS:    return "GHL Bass";
        case Part::DRUMS:       return "Drums";
        case Part::ELITE_DRUMS: return "E-Drums";
        case Part::VOCALS:      return "Vocals";
        case Part::HARMONIES:   return "Harmony";
        case Part::PRO_GUITAR:  return "Pro Gtr";
        case Part::PRO_BASS:    return "Pro Bass";
        case Part::PRO_KEYS:    return "Pro Keys";
        default:                return "Unknown";
    }
}

// Per-instrument icon (BinaryData pointer + size)
struct PartIconData {
    const char* data;
    int size;
};

inline PartIconData getPartIcon(Part p)
{
    switch (p) {
        case Part::DRUMS:
        case Part::ELITE_DRUMS: return { BinaryData::icon_drums_png,  BinaryData::icon_drums_pngSize };
        case Part::BASS:
        case Part::GHL_BASS:
        case Part::PRO_BASS:    return { BinaryData::icon_bass_png,   BinaryData::icon_bass_pngSize };
        case Part::KEYS:
        case Part::PRO_KEYS:    return { BinaryData::icon_keys_png,   BinaryData::icon_keys_pngSize };
        default:                return { BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize };
    }
}

// Drum track type — variants within PART DRUMS, distinguished by heuristics / song.ini
enum class DrumType { NORMAL = 1, PRO, FIVE_LANE };
enum class SkillLevel { EASY = 1, MEDIUM, HARD, EXPERT };
