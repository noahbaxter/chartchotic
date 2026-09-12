/*
    ==============================================================================

        UtilityStripComponent.h
        Author: Noah Baxter

        Vertical time-axis strip beside the highways, for everything that is not a
        gem on a lane: sections, and (later) star power, solo and fill. [coda] is the
        BRE tool's to draw, since that is the only thing its position means anything to.

        The view is a window on the whole project: a position and a span, free to sit
        anywhere and at any width. A chart is a region the window may be inside, never
        a mode you enter, and the only thing holding it is the timeline itself. Zoomed
        out far enough it draws each chart as a named block instead of sections.

        The view scrolls on its own and only follows the playhead during playback,
        which is where song structure is read, so bar lines are drawn in chunks rather
        than per bar: a line every bar is a wash, a line every 4 or 8 is a ruler.

        Design: docs/superpowers/specs/2026-09-10-utility-strip-design.md

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../Midi/SongSections.h"
#include "../Midi/SongList.h"
#include "../Midi/Utils/TimeConverter.h"   // defines TempoTimeSignatureMap, which the helper assumes
#include "../Midi/Utils/TempoTimeSignatureEventHelper.h"
#include "../Midi/Providers/REAPER/MidiCache.h"
#include "../Midi/Providers/TrackNoteProvider.h"   // defines ClipSpan
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/GridMath.h"
#include "Managers/AssetManager.h"
#include "../Editor/StripCommandMapper.h"
#include "StripViewMath.h"
#include "../UI/ControlConstants.h"
#include "../UI/Theme.h"

// Neutral and dark, in the same family as the neck rather than a UI blue.
namespace UtilityColours
{
    inline const juce::Colour background   = juce::Colour(0xFF121214);
    inline const juce::Colour border       = juce::Colour(0xFF303036);
    inline const juce::Colour chunkLine    = juce::Colour(0xFF3A3A42);
    inline const juce::Colour chunkLineBig = juce::Colour(0xFF56565F);
    // Structure stays neutral like the neck; the section label is the one accent.
    inline const juce::Colour sectionLine  = juce::Colour(0xFF6E8FB5);
    // Faint on purpose: the band sits under the note map, so anything heavier reads as a
    // colour the notes are, rather than as ground they sit on.
    inline const juce::Colour sectionBand  = juce::Colour(0x0D2F5F96);
    // The section the playhead is in, filled a step brighter than the rest.
    inline const juce::Colour sectionBandCurrent = juce::Colour(0x2C3E77B8);
    // Same blue as a section line but dashed and thinner: a clip edge is a fact about the
    // timeline, not a thing you charted.
    inline const juce::Colour clipEdge     = juce::Colour(0x996E8FB5);
    inline const juce::Colour sectionText  = juce::Colour(0xFF9CC4F5);
    inline const juce::Colour uiText       = juce::Colour(0xFFE2E2E8);
    inline const juce::Colour hoverLine    = juce::Colour(0xFFBFD4EC);
    inline const juce::Colour measureText  = juce::Colour(0xFF5E5E68);
    inline const juce::Colour playhead     = juce::Colour(0xFFFFFFFF);
    // The bar-number tab is a readout, not the playhead itself, so it sits a good way
    // below white: a solid white plate that size is the brightest thing on the strip.
    inline const juce::Colour playheadTab  = juce::Colour(0xFF9CA1AB);
    inline const juce::Colour playheadGlow = juce::Colour(0x2AFFFFFF);
    inline const juce::Colour label        = juce::Colour(0xFF7D7D85);
    inline const juce::Colour panelFill    = juce::Colour(0xFF1B1B1F);
    inline const juce::Colour noteMark     = juce::Colour(0xFF6E6E7A);
    inline const juce::Colour marqueeFill  = juce::Colour(0x333E77B8);
    inline const juce::Colour deleteFill   = juce::Colour(0x33C04A3A);
    inline const juce::Colour loopFill     = juce::Colour(0x2FD8B65A);
    inline const juce::Colour selectedLine = juce::Colour(0xFFFFFFFF);
    inline const juce::Colour namePlateHover = juce::Colour(0xFF2E4A72);
    // Follow still switched on but taken out of gear by a scroll: armed rather than acting,
    // which is a different thing from off and reads as a warning rather than as a layer.
    inline const juce::Colour followHeldFill = juce::Colour(0x3AD8B65A);
    inline const juce::Colour followHeldLine = juce::Colour(0xFFD8B65A);
    inline const juce::Colour followHeldText = juce::Colour(0xFFE8CE96);
    // An armed edit mode, which changes what the mouse does rather than what is drawn. Green
    // so it cannot be mistaken for a lit layer toggle or for follow being out of gear.
    inline const juce::Colour modeActiveFill = juce::Colour(0x3A4FB06A);
    inline const juce::Colour modeActiveLine = juce::Colour(0xFF4FB06A);
    inline const juce::Colour modeActiveText = juce::Colour(0xFFA8E2BA);
    // The draggable band behind a section while section editing is on, sized to the hit test
    // rather than to the line. Translucent so the note map underneath still reads.
    inline const juce::Colour sectionGrab         = juce::Colour(0x242F5F96);
    inline const juce::Colour sectionGrabHover    = juce::Colour(0x453E77B8);
    inline const juce::Colour sectionGrabSelected = juce::Colour(0x5A4E8FD8);

}

class UtilityStripComponent : public juce::Component
{
public:
    UtilityStripComponent() { setInterceptsMouseClicks(true, false); }

    void setSections(const SongSections* s)          { sections = s; }
    void setTempoMap(const TempoTimeSignatureMap* m) { tempoMap = m; }
    void setPlayhead(PPQ ppq)
    {
        playheadPPQ = ppq;

        // Nothing to drop out of any more: the view is a window on the project, so playing
        // from one chart into the next just scrolls, and follow carries the view across the
        // boundary the same way it carries it across a section.
        if (playing && followOn() && followEngaged) centreViewOn(ppq.toDouble());
    }

    // Pressing play re-arms follow. Scrolling during playback switches it off until then,
    // which is how every DAW handles someone reading ahead while the transport runs: it
    // stops chasing, and it starts again when you next ask for playback rather than making
    // you go and re-tick anything.
    void setPlaying(bool nowPlaying)
    {
        if (playing == nowPlaying) return;
        playing = nowPlaying;
        if (playing) followEngaged = true;
    }

    // Whole-song notes for the focused part, drawn as a density map rather than a chart.
    // One per visible highway. The map splits its width between them, so a single
    // instrument spreads across the strip and four share it.
    struct NoteSource
    {
        const std::vector<MidiCache::CachedNote>* notes = nullptr;
        Part part = Part::GUITAR;
        SkillLevel skill = SkillLevel::EXPERT;
    };

    void setNoteSources(std::vector<NoteSource> s) { noteSources = std::move(s); }

    // Merged MIDI clip extents for the whole project. Where several charts share one
    // timeline this is the only thing that says where each of them stops.
    void setClipSpans(const std::vector<ClipSpan>* s) { clipSpans = s; }

    // EVENTS items cannot overlap, so which song you are in is a fact about where the
    // playhead is rather than something to be chosen. There is no selection to keep.
    void setSongs(const SongList* s) { songs = s; }

    // Section the playhead is inside, for the banner over the highway. Empty when there is
    // none, or when the strip's own sections filter is off.
    juce::String currentSectionName() const;

    // The chart the playhead is in, for the banner over the highway. Empty in the gap between
    // two charts: you are not in one, and the nearest is not the same claim.
    juce::String currentSongName() const
    {
        const auto* song = songAtPlayhead();
        return song != nullptr ? song->name : juce::String();
    }

    // Lane colours come from the same place the gems and strikeline pads get them, so the
    // map cannot drift from the highway it summarises.
    void setAssetManager(AssetManager* am) { assetManager = am; }

    static constexpr int DEFAULT_ZOOM_BARS = SongList::DEFAULT_ZOOM_BARS;

    void zoomIn()  { scaleSpanAbout(0.5, 1.0 / ZOOM_STEP); }
    void zoomOut() { scaleSpanAbout(0.5, ZOOM_STEP); }

    // What the song and session rungs turned into. A view of one chart is a thing to ask
    // for, not a place to be, so it is a command like any other rather than a mode with its
    // own rules about what the wheel may do next.
    void fitChart();
    void fitProject();

    // The neighbouring chart's start, which is the navigation you want once nothing confines
    // the view: a chart is a place on the timeline rather than somewhere you enter.
    void gotoAdjacentChart(int direction);

    // Straight to the whole project and back to where you were working, since those are the
    // two ends anyone actually wants to reach in one keystroke.
    void toggleZoom()
    {
        if (viewSpanQN >= projectEndQN() - GridMath::TICK) fitChart();
        else                                               fitProject();
    }


    // Zooming keeps the moment under the pointer where it is, the way a map does, by moving
    // the view rather than the playhead. Entering a chart from the session view is the one
    // exception, since which chart the strip is in is read off the playhead.
    void zoomAbout(int y, int direction);

    // Scrolling the map seeks, freely: the wheel's travel scaled by the visible span, so
    // one notch covers the same proportion of screen at every zoom. Clicking snaps to a bar.
    std::function<void(double targetQN)> onSeekTo;

    // Whole replacement set, because renaming or inserting one section can renumber the
    // rest and the writer takes them all at once anyway.
    struct SectionEdit { double qn; juce::String name; };
    std::function<void(const std::vector<SectionEdit>&)> onSectionsEdited;

    std::function<void(double startQN, double endQN)> onTimeSelection;

    // Naming a song names its EVENTS item, addressed by where the item starts.
    std::function<void(double startQN, const juce::String& name)> onSongRenamed;

    // A layer toggle flipped, for saving it alongside the other view toggles. Follow's latch
    // is deliberately not reported: what you asked for persists, where you are does not.
    std::function<void(const juce::String& key, bool on)> onLayerToggled;

    // Drops the selected sections. Answers false when nothing was selected, which is what
    // lets the editor offer the delete key on to the highway instead.
    bool deleteSelection();

    // Every layer's saved key, so restoring can walk the layers without a second list to
    // keep in step with this one. A layer with nothing saved keeps its built-in default,
    // which is why there is no defaults list to hand out.
    std::vector<juce::String> layerKeys() const
    {
        std::vector<juce::String> keys;
        for (const auto& f : filters) keys.push_back(f.key);

        return keys;
    }

    // Unknown keys are ignored, so a project saved with a layer that no longer exists still
    // opens rather than tripping over its own state.
    void setLayerOn(const juce::String& key, bool on)
    {
        for (auto& f : filters)
            if (key == f.key) { f.on = on; return; }
    }

    // What the strip lets you edit. Its own axis, deliberately not the highway's write mode:
    // the two surfaces author different things, and turning on note editing has no business
    // taking the playhead off the strip's mouse button.
    enum class EditMode { None, Sections };

    void setEditMode(EditMode mode)
    {
        if (editMode == mode) return;
        editMode = mode;
        selection.clear();
        if (onEditModeChanged) onEditModeChanged((int)editMode);
        repaint();
    }

    EditMode getEditMode() const { return editMode; }

    // Restore only. Out-of-range values fall back to None rather than throwing, so state
    // saved by a build with more modes than this one still opens.
    void setEditModeIndex(int index)
    {
        editMode = index == (int)EditMode::Sections ? EditMode::Sections : EditMode::None;
        selection.clear();
    }

    std::function<void(int mode)> onEditModeChanged;

    StripCommandMapper& commandMapper() { return mapper; }

    void paint(juce::Graphics& g) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    void clearHover();

    // Pixels of map travel per unit of wheel delta. JUCE's own viewport rate (14px of a
    // 16px step), so the strip scrolls at the speed everything else in the OS does.
    static constexpr double WHEEL_PIXELS_PER_UNIT = 224.0;

    // How long after landing on a section's start a second step back means the section
    // before it rather than that same start again. Long enough to press twice deliberately,
    // short enough that coming back later restarts the section you are listening to.
    static constexpr double SECTION_BACKSTEP_GRACE_S = 1.5;
    // Lane colour, dimmed by alpha alone. Desaturating turned every mark towards the band
    // behind it, so the map stopped reading as the highway it summarises.
    static constexpr float NOTE_MAP_SATURATION = 0.9f;
    static constexpr float NOTE_MAP_ALPHA = 0.8f;

    static constexpr float CLIP_EDGE_DASH = 4.0f;
    // A section within this of a clip edge counts as marking it, so the two never double up.
    static constexpr double CLIP_EDGE_TOLERANCE_QN = 0.01;
    // The zoom readout and the song name are the two things you read from across the room
    // while charting, so both are sized past the strip's own label scale.
    static constexpr int FOOTER_H = 28;
    static constexpr int BUTTON_W = 24;
    static constexpr float FOOTER_BUTTON_PT = 16.0f;
    static constexpr float FOOTER_LABEL_PT = 12.0f;

    // Every size the strip draws text at, in one place and all through Theme::getUIFont, so
    // the panel uses the app's typeface rather than whatever JUCE defaults to.
    static constexpr float SECTION_LABEL_PT = 16.0f;
    static constexpr float FILTER_PT = 11.0f;
    static constexpr float MEASURE_PT = 10.0f;
    static constexpr float PLAYHEAD_TAB_PT = 11.0f;
    // One row of segments across the top rather than a list you expand: four toggles fit
    // the width, and a panel that has to be opened to see what is on hides its own state.
    static constexpr int FILTER_ROW_H = 22;
    // The mode row sits above the layer toggles and is deliberately a separate control: one
    // says what the strip draws, the other what a drag does to it.
    static constexpr int MODE_ROW_H = 22;
    static constexpr int FILTER_GAP = 2;

    // The key is what the layer is saved under, spelled out rather than derived from the
    // label so that renaming what the button says cannot silently orphan saved state.
    struct Filter { const char* label; const char* key; bool on; };
    // Sections are the reason the strip exists, so they are on and stay first. Follow is a
    // behaviour rather than something drawn, but this block is the strip's only switch panel
    // and a toggle nobody can find is a toggle nobody turns off.
    Filter filters[4] = { { "sections", "stripSections", true },
                          { "notes",    "stripNotes",    true  },
                          { "measures", "stripMeasures", true },
                          { "follow",   "stripFollow",   true } };

    static constexpr int FILTER_FOLLOW = 3;

    bool filterOn(int i) const { return filters[i].on; }

    int filterAreaHeight() const { return MODE_ROW_H + FILTER_ROW_H; }

    struct ModeButton { const char* label; EditMode mode; };
    static constexpr ModeButton MODES[] = { { "sections", EditMode::Sections } };

    juce::Rectangle<int> modeButtonBounds(int i) const
    {
        int count = (int)std::size(MODES);
        int w = getWidth() / count;
        bool last = i == count - 1;
        return { i * w, 0, last ? getWidth() - i * w : w, MODE_ROW_H };
    }

    void drawModes(juce::Graphics& g) const;

    struct View { double startQN, endQN; };
    View visibleRange() const;

    // Sets the window outright, clamped into the project. Everything that moves the view
    // ends up here, so there is one place that decides what a legal view is.
    void setView(double startQN, double spanQN);

    // Moves the view, never the playhead, and stops at the edges of the song it is in.
    // Answers false when it changed nothing, which is what keeps a wheel notch against a
    // wall from counting as a scroll.
    bool scrollViewBy(double deltaQN);

    // Puts `qn` at the follow position, or as near as the song's edges allow: at the start
    // and end of a chart the view runs out of room, so the playhead travels from the bottom
    // up to the middle and from the middle up to the top rather than the view scrolling
    // past the chart to keep it centred.
    void centreViewOn(double qn);

    // Fraction of the view above the bottom edge, so a little below centre: more of what is
    // coming than of what has gone, without losing sight of what you just played.
    static constexpr double FOLLOW_POSITION = 0.35;

    int trackTop() const { return filterAreaHeight(); }
    int trackHeight() const { return std::max(1, getHeight() - FOOTER_H - trackTop()); }

    double yForQN(double qn, const View& v) const
    {
        double span = std::max(1.0e-6, v.endQN - v.startQN);
        return trackTop() + trackHeight() - (qn - v.startQN) / span * trackHeight();
    }

    double qnForY(double y, const View& v) const
    {
        double span = std::max(1.0e-6, v.endQN - v.startQN);
        return v.startQN + (trackTop() + trackHeight() - y) / trackHeight() * span;
    }

    // Position under `y`, snapped to the grid that is drawn at this zoom, plus sections.
    double snappedBarQN(int y, const View& v) const;
    double barLineAtOrBefore(double qn) const;
    double chunkLineAtOrBefore(double qn, const View& v) const;
    int visibleChunk(const View& v) const;
    double sectionStep(int direction) const;

    // Last charted QN: where the final section has to stop.
    double contentEndQN() const;
    double clipEndAt(double qn) const;

    // Every seek the strip issues goes through here, so the project bounds scrubbing,
    // clicking and section stepping alike rather than only one of them.
    double clampSeek(double qn) const;

    // Zoomed out far enough that per-section detail is a wash, so the strip stops being a
    // view of one chart's sections and becomes a map of the project's charts. A span test
    // rather than a mode: there is no longer a rung to be on.
    bool songOverview() const
    { return hasSongs() && viewSpanQN >= projectEndQN() * SONG_OVERVIEW_SPAN; }

    // Most of the project on screen at once. Below this the blocks are wider than the view
    // and the shelf stops telling you anything the section lines do not.
    static constexpr double SONG_OVERVIEW_SPAN = 0.6;

    std::vector<juce::Rectangle<int>> songChunkBounds(const View& v) const;
    int songChunkAt(juce::Point<int> p) const;

    // The chunk's top strip renames it, the rest of it jumps into it. Same split as a
    // section's name plate against its row.
    int songNameAt(juce::Point<int> p) const;
    void drawSongChunks(juce::Graphics& g, const View& v) const;
    void beginSongRename(int index);

    static constexpr int SONG_CHUNK_INSET = 5;

    // Measured and drawn through the same call, or a plate sized with one font and filled
    // with another ellipsises names that would have fit.
    static juce::Font sectionLabelFont()
    { return Theme::getUIFont(SECTION_LABEL_PT).boldened(); }

    // The song covering `qn`, or nullptr in a gap. Items cannot overlap, so there is at most
    // one and the first match is the answer.
    const SongList::Song* songAt(double qn) const
    {
        if (songs == nullptr) return nullptr;

        // The start edge counts as inside. The cursor parks on a chart's first tick as a
        // matter of course, and a strict compare answers "gap" for a position that is drawn
        // sitting right on the block.
        for (const auto& song : songs->getSongs())
            if (qn >= song.startQN - GridMath::TICK && qn < song.endQN) return &song;

        return nullptr;
    }

    // The chart nearest `qn`, used where a command has to pick one and the position may sit
    // in a gap: fitting a chart, and stepping to the next.
    const SongList::Song* nearestSong(double qn) const
    {
        if (const auto* hit = songAt(qn)) return hit;
        if (songs == nullptr) return nullptr;

        const SongList::Song* best = nullptr;
        double bestGap = 0.0;

        for (const auto& song : songs->getSongs())
        {
            double gap = qn < song.startQN ? song.startQN - qn : qn - song.endQN;
            if (best == nullptr || gap < bestGap) { best = &song; bestGap = gap; }
        }
        return best;
    }

    const SongList::Song* songAtPlayhead() const { return songAt(playheadPPQ.toDouble()); }

    bool hasSongs() const { return songs != nullptr && songs->size() > 0; }

    // Where the project stops. The only bound the view has now: scrolling and seeking are
    // held inside the timeline and nothing narrower, so crossing between charts is a thing
    // you do rather than a thing you are prevented from doing.
    double projectEndQN() const { return std::max(contentEndQN(), 1.0); }

    // Zoom is a ratio per notch rather than a rung, so it feels the same at 8 bars and at
    // 800 and there is no ladder whose ends depend on which chart you are in.
    static constexpr double ZOOM_STEP = 1.35;
    static constexpr double MIN_SPAN_QN = 2.0;

    // `anchor` is 0 at the bottom of the track and 1 at the top: the point the zoom holds
    // still, so the moment under the pointer stays under the pointer.
    void scaleSpanAbout(double anchor, double factor);

    // Friction at a chart edge, in fractions of the visible span and milliseconds of rest.
    static constexpr double BOUNDARY_RESIST_SPAN = 0.12;
    static constexpr int    BOUNDARY_RESIST_RESET_MS = 400;

    // Applies the detent to a scroll that would leave the chart the view is sitting in.
    // Answers the start the view should actually take.
    double resistBoundary(double wantStartQN);

    static constexpr int MAX_BARS_WALKED = 100000;

    bool inTrackArea(juce::Point<int> p) const
    {
        return p.y >= trackTop() && p.y <= trackTop() + trackHeight();
    }

    void drawHover(juce::Graphics& g, const View& v) const;

    // Measure the containing section starts on, or 1 before the first section.
    int sectionStartMeasure(double qn) const;
    int measureAtQN(double qn) const;
    void drawMeasureNumber(juce::Graphics& g, float y, int measure, bool emphasised) const;

    // Two targets per section: the name plate renames, the rest of its row jumps to it.
    int sectionHeaderAt(juce::Point<int> p) const;
    int sectionNameAt(juce::Point<int> p) const;
    juce::Rectangle<int> sectionHeaderBounds(size_t index, const View& v) const;
    juce::Rectangle<int> sectionRowBounds(size_t index, const View& v) const;
    int stickyIndex(const View& v) const;

    // Where a section stops: the next one, or the end of its own clip if that comes first.
    double sectionEndQN(size_t index, const View& v) const;

    // Plate per section, empty where the name was dropped for crowding.
    std::vector<juce::Rectangle<int>> labelPlates(const View& v) const;

    static constexpr float SECTION_LINE_H = 2.0f;
    static constexpr float SECTION_LINE_SELECTED_EXTRA = 1.0f;

    static constexpr int LABEL_H = 19;
    static constexpr int LABEL_MIN_GAP = 2;
    // A plate sits this far above the line it names, and this far in from each edge of the
    // strip. The gap is what the pinned label has to clear to leave the next name its room.
    static constexpr int LABEL_LINE_GAP = 2;
    static constexpr int LABEL_INSET_X = 3;
    static constexpr int LABEL_TEXT_PAD = 16;
    // Clear of the pinned plate, close enough to read as that label's own underline.
    static constexpr float STICKY_HOVER_LINE_GAP = 2.0f;
    static constexpr float STICKY_HOVER_LINE_H = 2.0f;
    // A section this close to the snapped hover bar counts as the one being pointed at.

    juce::Rectangle<int> minusBounds() const
    { return { 4, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    juce::Rectangle<int> plusBounds() const
    { return { 4 + BUTTON_W + 2, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    // Equal segments across the width, last one taking the rounding so the group ends flush
    // with the strip rather than a pixel or two short of it.
    juce::Rectangle<int> filterButtonBounds(int i) const
    {
        int count = (int)std::size(filters);
        int w = getWidth() / count;
        bool last = i == count - 1;
        return { i * w, MODE_ROW_H, last ? getWidth() - i * w : w, FILTER_ROW_H };
    }

    void drawChunkLines(juce::Graphics& g, const View& v) const;
    void drawSectionBands(juce::Graphics& g, const View& v) const;
    void drawSections(juce::Graphics& g, const View& v) const;
    void drawClipEdges(juce::Graphics& g, const View& v) const;
    int litSectionIndex(const View& v) const;
    void drawNotes(juce::Graphics& g, const View& v) const;
    void drawFilters(juce::Graphics& g) const;
    void drawFooter(juce::Graphics& g) const;

    const SongSections* sections = nullptr;
    const TempoTimeSignatureMap* tempoMap = nullptr;
    std::vector<NoteSource> noteSources;
    const std::vector<ClipSpan>* clipSpans = nullptr;
    const SongList* songs = nullptr;

    // How much of the project the view spans. A plain length rather than a rung on a ladder:
    // the view is a window on the whole timeline, and a chart is a region it may happen to
    // sit inside. Seeded in QN because the tempo map is not loaded yet at construction.
    double viewSpanQN = DEFAULT_ZOOM_BARS * 4.0;

    // Where the bottom of the view sits. The strip scrolls on its own and never chases the
    // playhead: clicking sets where playback starts without dragging the view somewhere you
    // did not ask to be, and the playhead is free to run off the top while you read
    // somewhere else. Same as clicking around a DAW's arrange view with follow turned off.
    double viewStartQN = 0.0;

    StripViewMath::BoundaryDetent detent;

    bool playing = false;
    // Follow is the user's setting; engaged is whether it is currently acting. Scrolling
    // during playback drops the latch without touching the setting, and pressing play sets
    // it again.
    bool followEngaged = true;
    bool followOn() const { return filterOn(FILTER_FOLLOW); }

    AssetManager* assetManager = nullptr;
    PPQ playheadPPQ { 0.0 };
    int hoverY = -1;
    int hoverSection = -1;
    int hoverName = -1;

    // Where the pointer was when a scroll last started. Hover stays off until it is
    // somewhere else, so the move event a scroll leaves behind does not count as pointing.
    bool hoverHeld = false;
    juce::Point<int> hoverHeldFrom;

    EditMode editMode = EditMode::None;
    StripCommandMapper mapper;

    std::vector<int> selection;        // indices into the section list
    bool   dragActive = false;
    int    dragAnchorY = 0;
    int    dragCurrentY = 0;
    double dragMoveDeltaQN = 0.0;
    StripCommand dragCommand = StripCommand::None;

    // One editor for both, because only one thing is ever being renamed and the two commit
    // to completely different places.
    enum class RenameTarget { Section, Song };

    std::unique_ptr<juce::TextEditor> renameEditor;
    int renameIndex = -1;
    RenameTarget renameTarget = RenameTarget::Section;
    int hoverSongChunk = -1;

    StripMode stripMode() const
    { return editMode == EditMode::Sections ? StripMode::Write : StripMode::Play; }

    bool sectionEditing() const { return editMode == EditMode::Sections; }
    bool isSelected(int index) const
    { return std::find(selection.begin(), selection.end(), index) != selection.end(); }

    void dispatch(StripCommand cmd, const juce::MouseEvent& e);
    void beginRename(int index);
    void commitRename();
    // How far section `index` is drawn from where it actually sits, while a move drag is in
    // flight. Everything that positions a section goes through this, so the line, the name
    // plate and the bar number all preview together instead of one of them moving alone.
    double movePreviewQN(size_t index) const
    {
        if (!dragActive || dragCommand != StripCommand::UpdateMove) return 0.0;

        return isSelected((int)index) ? dragMoveDeltaQN : 0.0;
    }

    void commitSections(std::vector<SectionEdit> edits);
    std::vector<SectionEdit> currentEdits() const;
    void drawMarquee(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UtilityStripComponent)
};
