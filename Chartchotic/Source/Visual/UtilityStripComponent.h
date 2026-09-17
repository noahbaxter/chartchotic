/*
    ==============================================================================

        UtilityStripComponent.h
        Author: Noah Baxter

        Vertical time-axis strip beside the highways: a read-only overview of the
        editor's current view at song scale, and a way around the project. Nothing
        is written from it; the highways author.

        The view is a window on the whole project: a position and a span, free to sit
        anywhere and at any width. A chart is a region the window may be inside, never
        a mode you enter, and the only thing holding it is the timeline itself. Zoomed
        out far enough it draws each chart as a named block instead of sections.

        The view scrolls on its own and only follows the playhead during playback,
        which is where song structure is read, so bar lines are drawn in chunks rather
        than per bar: a line every bar is a wash, a line every 4 or 8 is a ruler.

        Design: docs/superpowers/specs/2026-09-10-utility-strip-design.md
                docs/superpowers/specs/2026-09-15-editor-views-design.md

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
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
#include "PhraseMath.h"
#include "../UI/ControlConstants.h"
#include "../UI/Theme.h"

// Neutral and dark, in the same family as the neck rather than a UI blue.
namespace UtilityColours
{
    inline const juce::Colour background   = juce::Colour(0xFF121214);
    // The ground a chart sits on, a shade up from the strip's own background. Everywhere
    // else is timeline with nothing charted on it, and which side of an edge you are on has
    // to read at a glance: a hairline alone does not say that, it only says "something here".
    inline const juce::Colour chartGround  = juce::Colour(0xFF2A2A33);
    inline const juce::Colour border       = juce::Colour(0xFF303036);
    inline const juce::Colour chunkLine    = juce::Colour(0xFF3A3A42);
    inline const juce::Colour chunkLineBig = juce::Colour(0xFF56565F);
    // Below a bar. A clear step under the bar line it sits inside, because a subdivision
    // drawn at the same weight as a bar line loses you the bars altogether.
    inline const juce::Colour subBarLine   = juce::Colour(0xFF26262C);
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
    // The chart name in the footer, in the same white as the name over the highway.
    inline const juce::Colour chartName    = juce::Colour(Theme::textWhite);
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
    inline const juce::Colour loopFill     = juce::Colour(0x2FD8B65A);
    // Follow still switched on but taken out of gear by a scroll: armed rather than acting,
    // which is a different thing from off and reads as a warning rather than as a layer.
    inline const juce::Colour followHeldFill = juce::Colour(0x3AD8B65A);
    inline const juce::Colour followHeldLine = juce::Colour(0xFFD8B65A);
    inline const juce::Colour followHeldText = juce::Colour(0xFFE8CE96);
    // A song block under the pointer in the overview.
    inline const juce::Colour songChunkHover = juce::Colour(0x453E77B8);

    // Phrase columns, in the colours the games draw them: star power the pale blue it lights
    // the highway, solo the deeper blue of a solo lane, a fill purple and a BRE orange.
    inline const juce::Colour phraseStarPower = juce::Colour(0xCC8FD8FF);
    inline const juce::Colour phraseSolo      = juce::Colour(0xCC3F7FE0);
    inline const juce::Colour phraseFill      = juce::Colour(0xCCA06CD8);
    inline const juce::Colour phraseBre       = juce::Colour(0xCCE08A3C);
    // 120-124 on a part that has no fills, sitting before the coda: not a BRE yet, so it is
    // drawn as the data it is rather than as something that plays.
    inline const juce::Colour phraseInert     = juce::Colour(0x665E5E68);
    inline const juce::Colour phraseColumnLine = juce::Colour(0xFF26262C);
    inline const juce::Colour codaLine        = juce::Colour(0xFFE08A3C);
}

class UtilityStripComponent : public juce::Component
{
public:
    UtilityStripComponent()
    {
        setInterceptsMouseClicks(true, false);
        rebuildMetrics();  // before the first resize, so nothing reads an unset size
    }

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

    // Whole-song notes, drawn as a density map rather than a chart, or as phrase columns in
    // the phrase views. One per visible highway. The strip splits its width between them, so
    // a single instrument spreads across it and four share it.
    struct NoteSource
    {
        const std::vector<MidiCache::CachedNote>* notes = nullptr;
        Part part = Part::GUITAR;
        SkillLevel skill = SkillLevel::EXPERT;
    };

    void setNoteSources(std::vector<NoteSource> s) { noteSources = std::move(s); }

    // The editor's view, which decides what the strip draws. It never decides what a click
    // does: the strip only navigates.
    void setEditorView(EditorView v)
    {
        if (editorView == v) return;
        editorView = v;
        repaint();
    }

    // Merged MIDI clip extents for the whole project. Where several charts share one
    // timeline this is the only thing that says where each of them stops.
    void setClipSpans(const std::vector<ClipSpan>* s) { clipSpans = s; }

    // EVENTS items cannot overlap, so which song you are in is a fact about where the
    // playhead is rather than something to be chosen. There is no selection to keep.
    void setSongs(const SongList* s) { songs = s; }

    // Section the playhead is inside, for the banner over the highway. Empty when there is
    // none.
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

    void zoomIn()  { stepSpanAbout(0.5, -1); }
    void zoomOut() { stepSpanAbout(0.5, 1); }

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

    std::function<void(double startQN, double endQN)> onTimeSelection;

    // Naming a song names its EVENTS item, addressed by where the item starts. Project
    // metadata rather than chart data, so it is the one thing the strip still writes.
    std::function<void(double startQN, const juce::String& name)> onSongRenamed;

    // Follow was switched on or off, for saving. The latch is deliberately not reported: what
    // you asked for persists, where you are does not.
    std::function<void(bool on)> onFollowChanged;
    void setFollowOn(bool on) { followSetting = on; }

    StripCommandMapper& commandMapper() { return mapper; }

    void paint(juce::Graphics& g) override;
    void resized() override;
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

    // How long after landing on a start a second step back means the one before it rather
    // than that same start again. Long enough to press twice deliberately, short enough that
    // coming back later restarts what you are listening to. Shared by sections and charts:
    // both are things you step back through while the transport keeps moving underneath.
    static constexpr double BACKSTEP_GRACE_S = 1.5;
    // Lane colour, dimmed by alpha alone. Desaturating turned every mark towards the band
    // behind it, so the map stopped reading as the highway it summarises.
    static constexpr float NOTE_MAP_SATURATION = 0.9f;
    static constexpr float NOTE_MAP_ALPHA = 0.8f;

    static constexpr float CLIP_EDGE_DASH = 4.0f;
    // A section within this of a clip edge counts as marking it, so the two never double up.
    static constexpr double CLIP_EDGE_TOLERANCE_QN = 0.01;
    // Every constant below is a size at REF_WIDTH, the width the strip was drawn for. The
    // editor hands it a share of the window instead now, so the whole panel scales bodily
    // from that reference rather than each number being tuned by eye: a font and the row it
    // sits in cannot drift apart if they come off the same multiplier.
    static constexpr float REF_WIDTH = 188.0f;
    // Text stops tracking the column past these. A wide strip should spend the extra width
    // on longer section names, not on headlines, and a narrow one still has to be readable.
    static constexpr float MIN_SCALE = 0.8f;
    static constexpr float MAX_SCALE = 1.6f;

    // What those constants come out as at the width we were actually given. Resolved once
    // per resize: paint() runs off the render clock, and a juce::Font built per item costs
    // more than everything it draws.
    struct Metrics
    {
        float scale = 1.0f;

        juce::Font sectionLabel, columnLabel, measure, measureBold, footerButton, playheadTab,
                   songName;

        int labelH, labelMinGap, labelLineGap, labelInsetX, labelTextPad;
        int segmentGap;
        int buttonW, zoomInset;
        int songInfoH, songNameH, songInfoPad;
        int songChunkInset, measureDrop, measureRowH, playheadTabH;

        float sectionLineH;
        float stickyHoverLineGap, stickyHoverLineH;
    };

    Metrics m;
    void rebuildMetrics();

    // Plate widths by name rather than by index: sections are edited between resizes and a
    // name is what the width depends on. Measuring one costs a font build, and labelPlates
    // walks every section three times a frame.
    mutable std::map<juce::String, int> plateWidthCache;
    static constexpr size_t PLATE_CACHE_MAX = 2048;
    int plateTextWidth(const juce::String& name) const;

    // What you are in: the chart's name between the arrows that step to its neighbours.
    static constexpr int SONG_INFO_H = 28;
    static constexpr int SONG_NAME_H = 20;
    static constexpr int SONG_INFO_PAD = 4;
    static constexpr float SONG_NAME_PT = 15.0f;
    int footerAreaHeight() const { return m.songInfoH; }
    int songInfoTop() const { return getHeight() - m.songInfoH; }
    void drawSongInfo(juce::Graphics& g) const;
    static constexpr int BUTTON_W = 24;
    static constexpr float FOOTER_BUTTON_PT = 16.0f;

    // Every size the strip draws text at, in one place and all through Theme::getUIFont, so
    // the panel uses the app's typeface rather than whatever JUCE defaults to.
    static constexpr float SECTION_LABEL_PT = 16.0f;
    static constexpr float COLUMN_LABEL_PT = 11.0f;
    static constexpr float MEASURE_PT = 10.0f;
    // How far above its line a ruler number sits: clear of the line without reading as part
    // of the bar above it.
    static constexpr int MEASURE_DROP = 14;
    static constexpr float PLAYHEAD_TAB_PT = 11.0f;
    static constexpr int SEGMENT_GAP = 2;
    static constexpr float FOLLOW_GLYPH_INSET = 0.28f;   // of the button, each side

    EditorView editorView = EditorView::Notes;
    bool followSetting = true;

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

    int trackTop() const { return 0; }
    int trackHeight() const { return std::max(1, getHeight() - footerAreaHeight() - trackTop()); }

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
    // The grid step in quarter notes at this zoom. Below a bar it is a beat, or a division
    // of one, rather than a fraction of the bar.
    double visibleChunk(const View& v) const;
    // Everywhere shift+scroll will stop, sorted and deduped. Chart edges, section edges, and
    // out past the charts, the scraps of MIDI that sit on their own.
    std::vector<double> snapPoints() const;
    double snapStep(int direction) const;

    // Both edges inclusive, unlike songAt, which is half open. A snap step landing exactly on
    // a chart's end has not left it yet: that is the stop before the one that leaves.
    const SongList::Song* chartContaining(double qn) const;

    // Holds a snap step that would leave the chart you are in at that chart's edge, so
    // crossing takes a deliberate second notch rather than happening on the way past.
    double resistChartCross(double targetQN);
    static constexpr int CHART_CROSS_NOTCHES = 2;
    static constexpr int CHART_CROSS_RESET_MS = 400;

    // Silence this long ends a cluster. Anything shorter is a rest inside one, not a gap
    // between two: too small and a sparse intro becomes a stop per phrase, too large and two
    // separate scraps read as one.
    static constexpr double CLUSTER_GAP_BARS = 4.0;

    // Nearest bar line, which is where a cluster edge is reported rather than at whatever
    // offbeat the first note happens to sit on.
    double nearestBarLine(double qn) const;

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

    // Measured and drawn through the same font, or a plate sized with one and filled with
    // another ellipsises names that would have fit.
    const juce::Font& sectionLabelFont() const { return m.sectionLabel; }

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

    static constexpr double MIN_ZOOM_BARS = 2.0;

    // Bars are the unit the ladder is written in, so every rung needs a bar length to become
    // a span. Taken at the view's own position, which is where the meter you are reading is.
    double barQN() const;
    double minSpanQN() const;
    std::vector<double> zoomSpans() const;

    // `anchor` is 0 at the bottom of the track and 1 at the top: the point the zoom holds
    // still, so the moment under the pointer stays under the pointer.
    void stepSpanAbout(double anchor, int direction);

    static constexpr int MAX_BARS_WALKED = 100000;

    bool inTrackArea(juce::Point<int> p) const
    {
        return p.y >= trackTop() && p.y <= trackTop() + trackHeight();
    }

    void drawHover(juce::Graphics& g, const View& v) const;

    // Measure of the last structural edge at or before `qn`: a section start, or a chart
    // start or end. What the chunk grouping counts from.
    int groupAnchorMeasure(double qn) const;
    int measureAtQN(double qn) const;
    // "6" on a downbeat, "6.2" on a beat, "6.2.5" off one. Takes a position rather than a
    // measure, because once the grid subdivides the bar number alone stops saying where you
    // are: every line in the bar would read the same.
    juce::String positionLabel(double qn) const;
    // How loudly a position says itself. Grid numbers are scenery on the heavier ruler lines;
    // a section start and the cursor are events, and those always read clearly.
    enum class NumberWeight { Grid, Section, Cursor };
    static constexpr float GRID_NUMBER_ALPHA = 0.5f;

    void drawMeasureNumber(juce::Graphics& g, float y, double qn, NumberWeight weight) const;

    // Five things want to put a number in the right-hand gutter: the ruler, a section start,
    // the hover bar, a pinned label's real measure, and the playhead's own tab. They used to
    // draw in paint order and land on top of each other. Now the whole set is resolved
    // before any of it is drawn, most specific first, and a number that would collide with
    // one already claimed is dropped rather than stacked under it.
    struct NumberDraw { float y; double qn; NumberWeight weight; };
    mutable std::vector<NumberDraw> numberDraws;
    mutable std::vector<juce::Range<int>> numberRows;

    void resolveMeasureNumbers(const View& v) const;
    juce::Range<int> numberRowAt(float y) const;
    bool claimNumberRow(juce::Range<int> row) const;
    bool claimNumber(float y, double qn, NumberWeight weight) const;
    void drawMeasureNumbers(juce::Graphics& g) const;

    // Plates for this paint, built once by resolveMeasureNumbers and read by drawSections
    // rather than rebuilt.
    mutable std::vector<juce::Rectangle<int>> framePlates;

    static constexpr int MEASURE_ROW_H = 12;
    static constexpr int PLAYHEAD_TAB_H = 16;

    int sectionHeaderAt(juce::Point<int> p) const;
    juce::Rectangle<int> sectionHeaderBounds(size_t index, const View& v) const;
    juce::Rectangle<int> sectionRowBounds(size_t index, const View& v) const;
    int stickyIndex(const View& v) const;

    // Where a section stops: the next one, or the end of its own clip if that comes first.
    double sectionEndQN(size_t index, const View& v) const;

    // Plate per section, empty where the name was dropped for crowding.
    std::vector<juce::Rectangle<int>> labelPlates(const View& v) const;

    static constexpr float SECTION_LINE_H = 2.0f;

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

    // Follow and zoom float over the top-right of the track instead of taking a row. Faint
    // until the pointer is on them, so they stay out of the way of the map they sit on.
    static constexpr int ZOOM_INSET = 4;
    static constexpr float ZOOM_IDLE_ALPHA = 0.35f;
    bool zoomHovered = false;

    juce::Rectangle<int> zoomBounds() const
    {
        int w = m.buttonW * 3 + m.segmentGap * 2;
        return { getWidth() - m.zoomInset - w, trackTop() + m.zoomInset, w, m.buttonW };
    }
    juce::Rectangle<int> followButtonBounds() const { return zoomBounds().withWidth(m.buttonW); }
    juce::Rectangle<int> minusBounds() const
    { return zoomBounds().withTrimmedLeft(m.buttonW + m.segmentGap).withWidth(m.buttonW); }
    juce::Rectangle<int> plusBounds() const
    { return zoomBounds().withTrimmedLeft((m.buttonW + m.segmentGap) * 2); }
    void drawZoomOverlay(juce::Graphics& g) const;
    void drawFollowButton(juce::Graphics& g, float alpha) const;

    // Square arrows at either end of the name row. Arrows alone, no words: the name between
    // them says what they move off.
    juce::Rectangle<int> prevChartBounds() const
    { return { m.songInfoPad, songInfoTop() + m.songInfoPad, m.songNameH, m.songNameH }; }
    juce::Rectangle<int> nextChartBounds() const
    { return { getWidth() - m.songInfoPad - m.songNameH, songInfoTop() + m.songInfoPad,
               m.songNameH, m.songNameH }; }

    void drawChartGround(juce::Graphics& g, const View& v) const;
    void drawChunkLines(juce::Graphics& g, const View& v) const;
    void drawSectionBands(juce::Graphics& g, const View& v) const;
    void drawSections(juce::Graphics& g, const View& v) const;
    void drawClipEdges(juce::Graphics& g, const View& v) const;
    int litSectionIndex(const View& v) const;
    void drawNotes(juce::Graphics& g, const View& v) const;

    //==========================================================================
    // Phrase columns, drawn in place of the note map in the SP and Phrase views

    bool showsPhrases() const
    { return editorView == EditorView::StarPower || editorView == EditorView::Phrase; }

    // The note map's side inset, so phrase columns line up with the notes they replace.
    static constexpr float PHRASE_COLUMN_INSET = 3.0f;

    // The kinds the current view shows. Each visible highway gets its own column, like the
    // note map, and each kind a sub-column inside it.
    std::vector<PhraseMath::Kind> phraseColumns() const;

    // One kind's phrases on one part, read off its reference pitch.
    std::vector<PhraseMath::PhraseSpan> phraseSpans(PhraseMath::Kind kind,
                                                         const NoteSource& source) const;
    std::optional<double> codaQN() const;

    juce::Colour phraseColour(PhraseMath::Kind kind, double startQN, Part part) const;
    void drawPhrases(juce::Graphics& g, const View& v) const;

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

    StripViewMath::Detent crossDetent;

    bool playing = false;
    // Follow is the user's setting; engaged is whether it is currently acting. Scrolling
    // during playback drops the latch without touching the setting, and pressing play sets
    // it again.
    bool followEngaged = true;
    bool followOn() const { return followSetting; }

    AssetManager* assetManager = nullptr;
    PPQ playheadPPQ { 0.0 };
    int hoverY = -1;
    int hoverSection = -1;

    // Where the pointer was when a scroll last started. Hover stays off until it is
    // somewhere else, so the move event a scroll leaves behind does not count as pointing.
    bool hoverHeld = false;
    juce::Point<int> hoverHeldFrom;

    StripCommandMapper mapper;

    bool   dragActive = false;
    int    dragAnchorY = 0;
    int    dragCurrentY = 0;
    StripCommand dragCommand = StripCommand::None;

    std::unique_ptr<juce::TextEditor> renameEditor;
    int renameIndex = -1;
    int hoverSongChunk = -1;

    void dispatch(StripCommand cmd, const juce::MouseEvent& e);
    void commitRename();
    void drawTimeSelection(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UtilityStripComponent)
};
