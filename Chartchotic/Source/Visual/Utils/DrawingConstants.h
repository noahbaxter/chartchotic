/*
    ==============================================================================

        DrawingConstants.h
        Author: Noah Baxter

        Central location for drawing and rendering constants used throughout
        the visualization pipeline. Includes opacity values, visual effects,
        and rendering parameters.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <vector>
#include <functional>
#include "../Geometry/BemaniConfig.h"

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// Opacity & Visual Effects
//==============================================================================

// Gem and note opacity
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
// A lane has to stay readable under a wall of gems and kick bars, which is exactly the passage
// most likely to carry one. Sustains can afford to be quieter: they sit under a single note.
constexpr float LANE_OPACITY = 0.6f;         // Lane sustain opacity

// SP view: each star power phrase as one white lane across the highway, faint enough that the
// gems it covers still read through it.
// A literal, not juce::Colours::white: that is a static in another translation unit, so a
// namespace-scope copy of it can initialise first and come out black.
// What sits beside the board when the highway is letterboxed. Matches the editor's own backdrop,
// so an opaque highway is indistinguishable from a transparent one wherever no background image
// is showing through.
inline const juce::Colour HIGHWAY_LETTERBOX_COLOUR { 0xFF000000 };

inline const juce::Colour PHRASE_STAR_POWER_COLOUR { 0xFFFFFFFF };
constexpr float PHRASE_STAR_POWER_OPACITY = 0.3f;
// Star power is the one phrase drawn in pure white, so the shared phrase-view boost blows it out
// where it merely strengthens the coloured ones. It takes its own value there instead.
constexpr float PHRASE_STAR_POWER_VIEW_OPACITY = 0.4f;

// Phrase view: a solo covers the whole board too, in the blue the games light it. Bright enough
// to survive a dense section: at the old dark 0x1E3F78 over 0.22 the wash all but vanished under
// a double kick run, where the bars and gems leave little board showing.
inline const juce::Colour PHRASE_SOLO_COLOUR { 0xFF2F6BC8 };
constexpr float PHRASE_SOLO_OPACITY = 0.34f;

// Gates across the board, one colour per kind. A disco flip is white; a section takes the
// strip's label blue, so the same section reads as the same thing on both surfaces.
inline const juce::Colour GATE_DISCO_COLOUR { 0xFFFFFFFF };
inline const juce::Colour SECTION_ACCENT_COLOUR { 0xFF9CC4F5 };
inline const juce::Colour GATE_TIME_SIG_COLOUR { 0xFF8FE0C0 };
inline const juce::Colour GATE_TEMPO_COLOUR { 0xFFF2B866 };
// How far a gate's label may be squeezed sideways to fit its lane before it wraps or truncates.
constexpr float GATE_LABEL_MIN_SCALE = 0.7f;

// Bar mode: how far the lanes that are not bar notes fade back.
constexpr float BAR_MODE_DIM = 0.3f;


// Phrase view: a phrase is what the view is for, so it draws well up from the faint wash it is
// in the note views. Multiplied rather than replaced, so each kind keeps its relative weight, and
// capped short of opaque so the gems under it stay findable.
constexpr float PHRASE_VIEW_OPACITY_BOOST = 2.5f;

// A BRE covers the same columns as a fill, so the two are told apart by shape rather than colour:
// the BRE is its lanes filled flat, the fill is that same colour as a border around a darkened
// interior. Border thickness is a fraction of the lane's own width so it narrows with the lane
// instead of swallowing the far end.
constexpr float PHRASE_FILL_BORDER_FRACTION = 0.13f;
// The gap a fill leaves on every side, as a fraction of lane width: the same between two fills as
// between an outer fill and the highway edge.
constexpr float PHRASE_FILL_GUTTER_FRACTION = 0.11f;
constexpr float PHRASE_FILL_INNER_DARKEN = 0.85f;
// The interior draws harder than the border it sits in: left at the border's alpha the board
// reads straight through it and the lane looks tinted rather than hollow.
constexpr float PHRASE_FILL_INNER_OPACITY = 0.92f;
// Depth step the border's pixel thickness is measured against, to inset the two ends by as much
// as the sides.
constexpr float PHRASE_FILL_DEPTH_PROBE = 0.002f;

// Whether each phrase kind is drawn at all. Here rather than with the other control defaults so
// the renderers can default their own members from the same constant the saved state is seeded
// with, instead of each carrying its own copy that has to be kept in step by hand.
// A fill paints every hand lane solid for its whole length, which buries the notes under it. Off
// by default: the activation lane and its recoloured gem say where the fill is without that.
constexpr bool DEFAULT_SHOW_FILLS = false;
constexpr bool DEFAULT_SHOW_BRE = true;
constexpr bool DEFAULT_SHOW_ACTIVATION = true;

// Phrase view: the activation note at the end of a drum fill is recut in a more vivid version of
// its own lane colour, so it reads as the note that takes star power without changing size -- a
// note drawn at the wrong size reads as a different note.
constexpr float PHRASE_ACTIVATION_VIVID_SATURATION = 1.5f;
constexpr float PHRASE_ACTIVATION_VIVID_BRIGHTEN = 0.25f;
// How far the gem's neutral pixels -- its white highlights and framing -- get pulled to the lane
// hue. This is the part that actually reads: the rest of the gem is already that colour.
constexpr float PHRASE_ACTIVATION_VIVID_NEUTRAL_PULL = 0.7f;

// The activation band is the lane's own colour at full strength where it meets the gem, fading
// to nothing along its length, so it reads as pointing at the note rather than covering it.
constexpr float PHRASE_ACTIVATION_OPACITY = 0.8f;

// Clone Hero draws the activation lane as the lane's two rails rather than a filled lane, which
// keeps the gems underneath readable. This is how much of the middle is missing.
constexpr float PHRASE_ACTIVATION_HOLE_FRACTION = 0.1f;

// A kick's lane is the whole board, so the same fraction leaves a gap several times wider in
// pixels than it does on a hand lane. Its rails reach further in to close that back up.
constexpr float PHRASE_ACTIVATION_HOLE_FRACTION_BAR = 0.07f;

// Each rail also fades across its width, solid at the lane's outer edge to nothing at the inner.
// It is banded because a fill takes one gradient and the length fade already has it: enough strips
// to read as smooth, each overlapping the next by a fraction of a strip to hide the seams. The
// curve bends the falloff -- above 1 holds the colour out at the edge and drops off later.
// Hand lanes cut lengthwise into vertical strips, sized in pixels so a wider lane gets more of
// them rather than wider ones. A kick's board-wide lane cuts across depth instead.
constexpr float PHRASE_ACTIVATION_EDGE_STRIP_PX = 5.0f;
constexpr int PHRASE_ACTIVATION_EDGE_STRIPS_MIN = 8;
constexpr int PHRASE_ACTIVATION_EDGE_STRIPS_MAX = 80;
constexpr int PHRASE_ACTIVATION_DEPTH_BANDS = 28;
constexpr int PHRASE_ACTIVATION_EDGE_CURVE_STOPS = 6;
// Bands abut rather than overlap. Overlapping translucent fills stack to roughly double alpha in
// the shared strip, and that jump reads far louder than the thin antialiased seam it was hiding.
constexpr float PHRASE_ACTIVATION_EDGE_OVERLAP = 0.0f;
constexpr float PHRASE_ACTIVATION_EDGE_FADE_CURVE = 1.0f;

// The bar closing the band's top end at the note, as a fraction of the lane's width there, so it
// keeps its thickness relative to the lane at any depth.
constexpr float PHRASE_ACTIVATION_CAP_THICKNESS = 0.07f;
// A kick's lane is the whole board, roughly five times a hand lane, so its cap takes a fraction
// scaled down to match -- landing near the thickness of the kick bar it caps.
constexpr float PHRASE_ACTIVATION_CAP_THICKNESS_BAR = 0.014f;

// How far gems fade back under a phrase drawn over them. Faded, not hidden: which notes a
// phrase covers is the point of these views.
constexpr float PHRASE_VIEW_GEM_DIM = 0.55f;

// Elite hi-hat ringing zone: solid until fadeStart, then a fainter tail. The span matches the
// pedal bar's so a ring lines up with the bar that generated it.
constexpr float HIHAT_SUSTAIN_LANE_SPAN = 3.0f;      // full lane widths, cols 1-3
constexpr float HIHAT_SUSTAIN_OPACITY = 0.30f;
constexpr float HIHAT_SUSTAIN_FADE_OPACITY = 0.11f;

// Gridline opacity by type
constexpr float MEASURE_OPACITY = 1.0f;      // Gridline opacity for measure lines
constexpr float BEAT_OPACITY = 0.3f;         // Gridline opacity for beat lines
constexpr float HALF_BEAT_OPACITY = 0.15f;   // Gridline opacity for half-beat lines

// Write-mode gridline opacities. The marker PNGs already bake in alpha
// (MEASURE peaks ~191/255, BEAT/HALF_BEAT peak ~128/255), which compresses
// the per-type contrast. We pick multipliers that, AFTER the baked alpha,
// keep MEASURE clearly brightest, BEAT clearly the rhythm anchor, and STEP
// the dimmest "subgrid". HALF_BEAT is hidden in write mode (renderer skips it).
// STEP is generated only in write mode.
constexpr float WRITE_MEASURE_OPACITY   = 1.0f;   // baked-in 0.75 -> ~0.75 effective
constexpr float WRITE_BEAT_OPACITY      = 0.9f;   // baked-in 0.50 -> ~0.45 effective
constexpr float WRITE_HALF_BEAT_OPACITY = 0.35f;  // hidden in write mode (kept for completeness)
constexpr float WRITE_STEP_OPACITY      = 0.25f;  // baked-in 0.50 -> ~0.13 effective (faded with depth)

// Maximum non-MEASURE gridlines rendered per frame. Budget is filled
// by subdivision priority (coarsest first) front-to-back, so zooming
// out naturally drops finer divisions. This is a ceiling — fewer render
// when fewer exist.
constexpr int MAX_HIGHWAY_GRIDLINES = 150;


// Side-of-highway protrusions: flat white tabs at write-mode MEASURE/BEAT
// gridlines, sticking horizontally outward from each highway edge. STEP
// gets no protrusion. The protrusion is THE structural anchor indicator —
// the on-highway lines stay subtle (same look as the placement grid).
//
// All sizes are fractions of strike-reference highway width — they scale
// with the highway like gems/glyphs do, NOT with absolute window pixels.
// Multiplied by widthRatio at render time for perspective foreshortening.
constexpr float WRITE_PROTRUSION_MEASURE_LENGTH_FRAC    = 0.080f;
constexpr float WRITE_PROTRUSION_BEAT_LENGTH_FRAC       = 0.045f;
constexpr float WRITE_PROTRUSION_MEASURE_THICKNESS_FRAC = 0.008f;
constexpr float WRITE_PROTRUSION_BEAT_THICKNESS_FRAC    = 0.008f;
// Small downward (closer-to-camera) Y nudge — protrusions are flat 2D, so
// they draw at the gridline's projected centerY, but the marker PNG's
// visible alpha line sits slightly below sprite center on the highway
// plane. This nudges the tabs to visually meet the line as drawn.
constexpr float WRITE_PROTRUSION_Z_NUDGE_FRAC           = 0.006f;

// Right-side measure number labels in write mode. Font size and gap are
// fractions of strike-reference highway width (scale with the highway
// the same way gems do). Anchored past the MEASURE protrusion's tip so
// the label sits clear of the white tab. Left-aligned so digits read
// consistently. REAPER-style 1-indexed display.
constexpr float WRITE_MEASURE_LABEL_FONT_FRAC      = 0.075f;
constexpr float WRITE_MEASURE_LABEL_GAP_FRAC       = 0.030f;
constexpr float WRITE_MEASURE_LABEL_MIN_FONT_PX    = 9.0f;   // screen-pixel readability floor — kept absolute

// Single visibility knob: approximate maximum number of labels visible
// across the rendered highway at once.
//
// Higher = more labels (denser, sub-divisions fill in). Lower = fewer
// (sparser, only structural beats). Internally converted to a normalized-
// position spacing threshold (1.0 / count) — labels are linear in TIME /
// scroll speed, so faster scroll naturally fits fewer measures-of-music
// per highway and finer divisions drop out, while the back of the
// highway can't stack regardless of perspective compression.
//
// Priority order when filtering: MEASURE > BEAT > HALF_BEAT > STEP.
constexpr float WRITE_LABEL_TARGET_COUNT = 3.0f;

// Highway texture
constexpr float TEXTURE_OPACITY_DEFAULT = 1.0f;  // Default highway texture opacity

// Hit animation rendering
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity
constexpr double HIT_ANIMATION_DURATION = 0.10;   // Hit flash total duration (seconds)
constexpr double KICK_ANIMATION_DURATION = 0.15;  // Kick/bar flash total duration (seconds)
constexpr int HIT_FLARE_MAX_FRAME = 2;            // Show flare for first N frames of hit animation

// Far-end fade (highway length)
constexpr float FAR_FADE_DEFAULT = 1.20f;     // Default user slider value (position space)
constexpr float FAR_FADE_MIN     = 0.50f;     // Minimum user slider value
constexpr float FAR_FADE_MAX     = 5.00f;     // Maximum user slider value
constexpr float FAR_FADE_LEN     = 0.35f;     // Length of fade zone
constexpr float FAR_FADE_CURVE   = 1.0f;      // Fade exponent (1=linear)

inline float getHwyScale(bool isDrums) { return bemaniConfig.hwyScale(isDrums); }


inline float calculateFarFade(float position, float fadeEnd, float fadeLen, float fadeCurve)
{
    float fadeStart = fadeEnd - fadeLen;
    if (position <= fadeStart) return 1.0f;
    if (position >= fadeEnd)   return 0.0f;
    float t = (position - fadeStart) / fadeLen;
    return 1.0f - std::pow(t, fadeCurve);
}

//==============================================================================
// Draw Order & Render Pipeline
//==============================================================================

enum class DrawOrder
{
    BACKGROUND,
    TRACK,
    GRID,
    TRACK_SIDEBARS,
    TRACK_LANE_LINES,
    TRACK_STRIKELINE,
    SUSTAIN,
    BAR,
    // Above the bars, below the gems. Down with the track art it was buried by every kick bar
    // that crossed it, which during a blast beat is most of them, so the lane read as a few
    // scraps of colour between notes. Gems still draw on top, so what you are playing is never
    // obscured by the marker telling you how to play it.
    LANE,
    NOTE,
    TRACK_CONNECTORS,
    OVERLAY,
    BAR_ANIMATION,
    NOTE_ANIMATION,
    TEXT_EVENT,
};

static constexpr int DRAW_ORDER_COUNT = 15;

//==============================================================================
// Text Event Data
//==============================================================================

struct TimeBasedFlipRegion {
    double startTime;
    double endTime;
};
using TimeBasedFlipRegions = std::vector<TimeBasedFlipRegion>;

// What a gate across the board marks. Each kind that is showing gets its own lane, side by
// side in this order, so two kinds landing on the same beat sit beside each other instead of
// stacking into one unreadable wall.
enum class GateKind { Section, TimeSig, Tempo, DiscoFlip, Count };
constexpr int GATE_KIND_COUNT = (int)GateKind::Count;

inline juce::Colour gateColour(GateKind kind)
{
    switch (kind)
    {
        case GateKind::Section:   return SECTION_ACCENT_COLOUR;
        case GateKind::TimeSig:   return GATE_TIME_SIG_COLOUR;
        case GateKind::Tempo:     return GATE_TEMPO_COLOUR;
        case GateKind::DiscoFlip:
        case GateKind::Count:     break;
    }
    return GATE_DISCO_COLOUR;
}

struct TimeBasedEventMarker {
    double time;
    juce::String label;
    GateKind kind = GateKind::Section;
};
using TimeBasedEventMarkers = std::vector<TimeBasedEventMarker>;
static constexpr int MAX_DRAW_COLUMNS = 8;
using DrawCallBucket = std::vector<std::function<void(juce::Graphics&)>>;
using DrawCallGrid = std::array<std::array<DrawCallBucket, MAX_DRAW_COLUMNS>, DRAW_ORDER_COUNT>;
using DrawCallMap = DrawCallGrid;
