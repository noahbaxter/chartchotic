#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/GridMath.h"

//==============================================================================
// Authoring types shared between HighwayComponent (event dispatcher),
// WriteController (state owner), and GridlineGenerator (step grid).

constexpr double kTimeEpsilon = 0.002;

// Length given to a note that carries no sustain of its own. Placement, copy
// capture and the drag preview all read it here so they cannot disagree about
// how long an unmeasured note is.
constexpr double kShortNoteDurationQN = 0.1;

// How far back to search for a sustain whose body reaches the point tested.
// Heuristic, not a derived bound: 8 bars of 4/4, and a longer sustain is
// invisible to overlap resolution and to erase. See BACKLOG.
constexpr double kSustainLookbackQN = 32.0;

namespace AuthoringColours
{
    static const juce::Colour selectTint   = juce::Colour(180, 220, 255).withAlpha((uint8)140);
    static const juce::Colour eraseTint    = juce::Colour(255, 80, 80).withAlpha((uint8)160);
    static const juce::Colour marqueeErase  = juce::Colour(255, 80, 80);    // erase marquee outline/fill
    static const juce::Colour marqueeSelect = juce::Colour(100, 180, 255);  // select marquee outline/fill
}

struct AuthoringPoint
{
    juce::Point<float> screenPos;
    bool   onHighway = false;        // coupled with laneIndex: true ⇔ laneIndex >= 0
    int    laneIndex = -1;
    double rawProjectQN = 0.0;
    bool   overExistingNote = false;
    bool   hitSustainBody = false;
    double hitNoteStartQN = 0.0;
    int    hitNotePitch = -1;
};

struct AuthoringContext
{
    juce::ModifierKeys mods;
    bool leftButton = false;
    bool rightButton = false;
};

struct SelectedNote
{
    int    trackIdx    = -1;
    double startQN     = 0.0;
    double endQN       = 0.0;
    int    pitch       = -1;
    int    lane        = -1;
    bool   sustainOnly = false;
};

// What a range drag writes. Plural because a fill needs all five markers at once.
struct RangeTarget
{
    std::vector<int> pitches;
    bool             lockedToLane = true;

    int single() const { return pitches.empty() ? -1 : pitches.front(); }
};

struct MarqueeRect
{
    double startQN = 0.0;
    int    startLane = 0;
    double qnLo = 0.0, qnHi = 0.0;
    int    laneLo = 0, laneHi = 0;

    void begin(double qn, int lane)
    {
        startQN = qn; startLane = lane;
        qnLo = qnHi = qn;
        laneLo = laneHi = lane;
    }

    void update(double qn, int lane, bool /*barMode*/, bool /*isDrums*/)
    {
        qnLo = std::min(startQN, qn);
        qnHi = std::max(startQN, qn);
        laneLo = std::min(startLane, lane);
        laneHi = std::max(startLane, lane);
    }

    bool contains(double qn, int lane) const
    {
        return !GridMath::before(qn, qnLo) && !GridMath::after(qn, qnHi)
            && lane >= laneLo && lane <= laneHi;
    }
};

struct OverlayState
{
    struct PreviewNote
    {
        int    lane = -1;
        double startQN = 0.0;
        double endQN = 0.0;
        int    pitch = -1;
        // What the note actually is. Faded previews have to show the real
        // gem, otherwise dragging a ghost or a cymbal makes it look like it
        // turned into a plain note for the duration of the drag.
        GemWrapper gem;
    };

    // Hover ghost
    bool   ghostVisible = false;
    int    ghostLane = -1;
    double ghostQN = 0.0;
    bool   ghostShowsErase = false;
    GemWrapper ghostGem;   // whole wrapper: hover must show exactly what will be placed
    // Transient hint for a modifier-held mode, e.g. alternating kick paint.
    juce::String ghostModeLabel;
    struct StampGhost { int lane; double qnOffset; double duration; GemWrapper gem; };
    std::vector<StampGhost> stampGhosts;

    // Draw stroke preview
    bool                     drawPreviewVisible = false;
    std::vector<PreviewNote> drawPreviewNotes;

    // Phrase drag preview, in the SP and Phrase views: a range with no lane, drawn as the
    // phrase it would write.
    // A lane is written through the phrase path but is not a phrase kind, so it rides here as a
    // value no Kind takes. Without it a lane preview drew in whichever phrase colour was last
    // selected, which is how a lane ended up previewing as a solo.
    static constexpr int PREVIEW_KIND_LANE = -1;

    struct PhrasePreview
    {
        bool   visible = false;
        double startQN = 0.0;
        double endQN = 0.0;
        int    kind = 0;      // PhraseMath::Kind, or PREVIEW_KIND_LANE
        // Lanes only. A lane is drawn per column, and which columns it lands on is derived
        // rather than picked, so the preview has to be told. Every other kind spans the
        // whole board or fans out over the hand columns on its own.
        std::vector<int> columns;
    };
    PhrasePreview phrasePreview;

    // Hover preview: one grid step at the snapped cursor, so a phrase view shows what a click
    // would start the way the note views show a ghost gem.
    PhrasePreview phraseHover;

    // Sections view: a click here would add a section at ghostQN, so the highway draws a gate
    // there. False over an existing section, whose own gate is already on screen.
    bool sectionHoverNew = false;

    // Lane tool: notes a lane would capture, drawn at full strength through the phrase dim. A
    // lane snaps its edges to note starts rather than to the grid, so this is how the edges
    // are shown: on hover the one note an edge would land on, during a drag every note the
    // lane is going to cover.
    struct LaneHighlight { int lane; double qn; };
    std::vector<LaneHighlight> laneHighlights;

    // Erase sweep preview
    bool                     eraseSweepVisible = false;
    std::vector<PreviewNote> eraseSweepTargets;

    // Selection (edit mode)
    std::vector<SelectedNote> selectedNotes;
    bool selectionBoundsVisible = false;
    struct SelectionBounds {
        int    minLane = 0;
        int    maxLane = 0;
        double minQN  = 0.0;
        double maxQN  = 0.0;
    } selectionBounds;

    // Move drag preview
    bool                     moveDragVisible = false;
    std::vector<PreviewNote> movePreviewNotes;

    // Marquee (select or erase) — highway-space coordinates
    bool        marqueeVisible = false;
    bool        marqueeErase   = false;
    // Off for a shift (toggle) box, which shows its result through the selection instead.
    bool        marqueeTintsContents = true;
    MarqueeRect marqueeRect;
    double      eraseClickedNoteQN    = -1.0;
    int         eraseClickedLane     = -1;
    double      eraseClickedSustainQN = -1.0;
    int         eraseClickedSustainLane = -1;

    bool   barMode = false;
};

//==============================================================================

class OptimisticPatchBuffer
{
public:
    // `gem` is what the add renders as for the few frames before the reparse lands, so it has
    // to be everything the note will parse back as or the preview flashes and snaps.
    struct Patch {
        int    lane = -1;
        double startQN = 0.0;
        int    framesLeft = 0;
        GemWrapper gem;
        double endQN = 0.0;   // > startQN draws a sustain body
    };

    void addRemove(int lane, double qn) { removes.push_back({ lane, qn, kFrames }); }
    void addAdd(int lane, double qn, const GemWrapper& gem, double endQN = 0.0)
    {
        adds.push_back({ lane, qn, kFrames, gem, endQN });
    }

    void tick()
    {
        auto expire = [](auto& vec) {
            for (auto& p : vec) --p.framesLeft;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [](const auto& p) { return p.framesLeft <= 0; }), vec.end());
        };
        expire(adds);
        expire(removes);
    }

    const std::vector<Patch>& getAdds()    const { return adds; }
    const std::vector<Patch>& getRemoves() const { return removes; }

private:
    std::vector<Patch> adds;
    std::vector<Patch> removes;
    static constexpr int kFrames = 4;
};

enum class EventType { Down, Drag, Up, DoubleClick };

enum class MouseButton { None, Left, Right, Middle };

enum class ModifierFlags : int {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
    // Never bound to anything: the host owns Cmd. Tracked only so a bare-key
    // binding refuses to fire while Cmd is held, leaving Cmd+S and friends to
    // reach REAPER.
    Cmd   = 1 << 3,
};
inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<int>(a) | static_cast<int>(b));
}
inline bool operator&(ModifierFlags a, ModifierFlags b) {
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

enum class WriteCommand {
    None,
    // Draw-mode mouse commands
    BeginSustain,
    UpdateSustain,
    CommitSustain,
    BeginPaint,
    ContinuePaint,
    CommitPaint,
    BeginErase,
    ContinueErase,
    EndErase,
    // Edit-mode mouse commands
    SelectAt,
    BeginMarquee,
    ContinueMarquee,
    CommitMarquee,
    BeginMove,
    ContinueMove,
    CommitMove,
    DoubleClick,
    // Key commands
    ToggleWriteMode,
    ToggleSubMode,
    ToggleLaneTool,
    ToggleSnap,
    ToggleTuplet,
    CycleTuplet,
    StepDown,
    StepUp,
    // Home-row note-type slots; meaning is per-instrument, see ModifierSlots.h
    ModifierSlot1,
    ModifierSlot2,
    ModifierSlot3,
    ModifierSlot4,
    ModifierSlot5,
    DeleteSelection,
    DeselectAll,
    ToggleBarMode,
    CaptureStamp,
    RecallStamp,
    // Editor-wide, handled by the editor rather than the authoring controllers
    ToggleUtilityStrip,
    ToggleStripZoom,
    PreviousChart,
    NextChart,
    ToggleMonitorClick,
    ToggleMonitorNotes,
};

enum class SubMode { Draw, Edit };

// One for the whole editor, orthogonal to SubMode: Draw/Edit is how you interact, the view is
// what every highway and the strip show and what a click writes.
enum class EditorView { Notes, StarPower, Phrase, Sections, Tempo };
enum class DrumDynamic { Normal, Ghost, Accent };
enum class GuitarForce { None, Hopo, Strum, Tap };

//==============================================================================
// Step grid math — shared between WriteController (click snap) and
// GridlineGenerator (visual grid). One definition, no drift.

// The tuplets on offer. Odd only: an even tuplet is an odd one at a finer division (6 is 3 at
// twice the division, 10 is 5), so it would only duplicate a grid already reachable.
constexpr int kTuplets[] = { 3, 5, 7, 9, 11, 13, 15, 17, 19 };

inline bool isTuplet(int t)
{
    for (int k : kTuplets) if (k == t) return true;
    return false;
}

// The tuplet after `t`, wrapping, and the first one when `t` is off or unknown.
inline int nextTuplet(int t)
{
    constexpr int count = (int)(sizeof(kTuplets) / sizeof(kTuplets[0]));
    for (int i = 0; i < count; ++i)
        if (kTuplets[i] == t) return kTuplets[(i + 1) % count];
    return kTuplets[0];
}

inline double stepSpacingQN(int stepDivision, int tuplet)
{
    if (tuplet > 0)
        return 8.0 / (double(stepDivision) * double(tuplet));
    return 4.0 / double(stepDivision);
}

// The stretch of the map one time signature governs. Steps count from its anchor, the way
// the gridlines do, so a bar that is short by a 16th moves the grid after it with it. The
// next anchor is a grid point too, even when the last step before it falls short.
struct GridSection
{
    double anchorQN = 0.0;
    double endQN    = std::numeric_limits<double>::infinity();
    // One bar of the section's time signature, 0 when unknown. Every bar line is a snap point
    // whatever the step: a 1/1 step is four beats, which in 3/4 or 7/8 walks off the bar lines,
    // and the 1 of a bar is the one place a coarse grid most needs to reach.
    double barQN    = 0.0;
};

inline double snapToStep(double rawQN, int stepDivision, int tuplet, GridSection section = {})
{
    const double spacing = stepSpacingQN(stepDivision, tuplet);
    if (spacing <= 0.0)
        return rawQN;

    auto nearestOn = [&](double step)
    {
        const double lo = section.anchorQN + std::floor((rawQN - section.anchorQN) / step) * step;
        const double hi = std::min(lo + step, section.endQN);
        return (rawQN - lo < hi - rawQN) ? lo : hi;
    };

    double snapped = nearestOn(spacing);
    if (section.barQN > 0.0)
    {
        const double bar = nearestOn(section.barQN);
        if (std::abs(rawQN - bar) < std::abs(rawQN - snapped)) snapped = bar;
    }

    const double oneOver128 = 1.0 / 128.0;
    const double quantized  = std::round(snapped / oneOver128) * oneOver128;
    if (std::abs(snapped - quantized) < 1e-6)
        snapped = quantized;
    return snapped;
}
