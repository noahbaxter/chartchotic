/*
    ==============================================================================

        UtilityStripComponent.h
        Author: Noah Baxter

        Vertical time-axis strip beside the highways, for everything that is not a
        gem on a lane: sections, [coda], and (later) star power, solo and fill.

        Zoom runs from 8 bars to the whole song. It follows the playhead and is
        where song structure is read, so bar lines are drawn in chunks rather than
        per bar: a line every bar is a wash, a line every 4 or 8 is a ruler.

        Design: docs/superpowers/specs/2026-09-10-utility-strip-design.md

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
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
#include "../UI/ControlConstants.h"

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
    inline const juce::Colour codaLine     = juce::Colour(0xFFC98A5A);
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

}

class UtilityStripComponent : public juce::Component
{
public:
    UtilityStripComponent() { setInterceptsMouseClicks(true, false); }

    void setSections(const SongSections* s)          { sections = s; }
    void setTempoMap(const TempoTimeSignatureMap* m) { tempoMap = m; }
    void setPlayhead(PPQ ppq)                        { playheadPPQ = ppq; }

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

    // The project's songs, and which one confines the view. Selection does not survive the
    // list changing shape: a span list carries no item identity, so guessing which item is
    // the one you had across a refetch would be a lie.
    void setSongs(const SongList* s)
    {
        songs = s;
        int count = s != nullptr ? s->size() : 0;
        if (count != lastSongCount)
        {
            lastSongCount = count;
            selectedSongIndex = SongList::SESSION;
        }
    }

    void setSelectedSong(int index)
    {
        int count = songs != nullptr ? songs->size() : 0;
        selectedSongIndex = (index >= 0 && index < count) ? index : SongList::SESSION;
        repaint();
    }

    int getSelectedSong() const { return selectedSongIndex; }

    // Section the playhead is inside, for the banner over the highway. Empty when there is
    // none, or when the strip's own sections filter is off.
    juce::String currentSectionName() const;

    // The chart the playhead is in, for the banner over the highway. Falls back to position
    // rather than to nothing on Session: which chart you are looking at is a fact about
    // where the playhead is, and it is true whether or not you picked one.
    //
    // Overlapping songs make the picked one the better answer when there is one, since an
    // album item and the track inside it both contain the playhead and only you know which
    // you meant.
    juce::String currentSongName() const
    {
        if (const auto* picked = selectedSong()) return picked->name;
        if (songs == nullptr) return {};

        double now = playheadPPQ.toDouble();
        for (const auto& song : songs->getSongs())
            if (now >= song.startQN && now < song.endQN) return song.name;

        return {};
    }

    // Lane colours come from the same place the gems and strikeline pads get them, so the
    // map cannot drift from the highway it summarises.
    void setAssetManager(AssetManager* am) { assetManager = am; }

    // 0 is the whole song. 64 bars is where structure and precision both hold.
    static constexpr int ZOOM_STEPS[] = { 8, 16, 32, 64, 128, 256, 0 };
    static constexpr int DEFAULT_ZOOM_INDEX = 3;

    int barsVisible() const { return ZOOM_STEPS[zoomIndex]; }
    void zoomIn()  { setZoomIndex(zoomIndex - 1); }

    // Past the whole of one song there is one thing left to zoom out to, which is the
    // session it sits in. Makes leaving a song the same gesture as widening the view rather
    // than a trip out to a selector.
    void zoomOut()
    {
        if (barsVisible() == 0 && selectedSong() != nullptr)
        {
            setSelectedSong(SongList::SESSION);
            return;
        }

        setZoomIndex(zoomIndex + 1);
    }
    void toggleZoom()
    {
        setZoomIndex(barsVisible() == 0 ? DEFAULT_ZOOM_INDEX
                                        : (int)std::size(ZOOM_STEPS) - 1);
    }

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

    void setWriteMode(bool on)
    {
        if (writeMode == on) return;
        writeMode = on;
        selection.clear();
        repaint();
    }

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

    static constexpr double PLAYHEAD_FROM_BOTTOM = 0.12;
    // Pixels of map travel per unit of wheel delta. JUCE's own viewport rate (14px of a
    // 16px step), so the strip scrolls at the speed everything else in the OS does.
    static constexpr double WHEEL_PIXELS_PER_UNIT = 224.0;
    // Lane colour, dimmed by alpha alone. Desaturating turned every mark towards the band
    // behind it, so the map stopped reading as the highway it summarises.
    static constexpr float NOTE_MAP_SATURATION = 0.9f;
    static constexpr float NOTE_MAP_ALPHA = 0.8f;

    static constexpr float CLIP_EDGE_DASH = 4.0f;
    // A section within this of a clip edge counts as marking it, so the two never double up.
    static constexpr double CLIP_EDGE_TOLERANCE_QN = 0.01;
    static constexpr int FOOTER_H = 20;
    static constexpr int BUTTON_W = 18;
    static constexpr int FILTER_ROW_H = 18;
    static constexpr int FILTER_HEADER_H = 20;

    struct Filter { const char* label; bool on; };
    // Sections are the reason the strip exists, so they are on and stay first.
    Filter filters[3] = { { "sections", true }, { "notes", false }, { "measures", true } };
    bool filtersExpanded = false;

    bool filterOn(int i) const { return filters[i].on; }

    int filterAreaHeight() const
    {
        return FILTER_HEADER_H + (filtersExpanded ? (int)std::size(filters) * FILTER_ROW_H : 0);
    }

    struct View { double startQN, endQN; };
    View visibleRange() const;

    void setZoomIndex(int i)
    {
        int clamped = juce::jlimit(0, (int)std::size(ZOOM_STEPS) - 1, i);
        if (clamped == zoomIndex) return;
        zoomIndex = clamped;
        repaint();
    }

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

    // Every seek the strip issues goes through here, so a picked song bounds scrubbing,
    // clicking and section stepping alike rather than only one of them.
    double clampSeek(double qn) const;

    // Session, zoomed all the way out, with songs to show. The strip stops being a view of
    // one chart's sections and becomes a map of the project's charts.
    bool songOverview() const
    { return selectedSong() == nullptr && barsVisible() == 0 && songs != nullptr && songs->size() > 0; }

    // One column per song, and a song takes the first column it does not collide in. An
    // album item spanning the per-song items lands beside them rather than on top of them.
    std::vector<int> songLanes(int& laneCount) const;
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
    { return juce::Font(juce::FontOptions(16.0f, juce::Font::bold)); }

    // The song confining the view, or nullptr on Session.
    const SongList::Song* selectedSong() const
    {
        return songs != nullptr ? songs->at(selectedSongIndex) : nullptr;
    }

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

    static constexpr int LABEL_H = 19;
    static constexpr int LABEL_MIN_GAP = 2;
    // Clear of the pinned plate, close enough to read as that label's own underline.
    static constexpr float STICKY_HOVER_LINE_GAP = 2.0f;
    static constexpr float STICKY_HOVER_LINE_H = 2.0f;
    // A section this close to the snapped hover bar counts as the one being pointed at.

    juce::Rectangle<int> minusBounds() const
    { return { 4, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    juce::Rectangle<int> plusBounds() const
    { return { 4 + BUTTON_W + 2, getHeight() - FOOTER_H + 2, BUTTON_W, FOOTER_H - 4 }; }
    juce::Rectangle<int> filterHeaderBounds() const
    { return { 0, 0, getWidth(), FILTER_HEADER_H }; }
    juce::Rectangle<int> filterRowBounds(int i) const
    { return { 0, FILTER_HEADER_H + i * FILTER_ROW_H, getWidth(), FILTER_ROW_H }; }

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
    int selectedSongIndex = SongList::SESSION;
    int lastSongCount = 0;

    AssetManager* assetManager = nullptr;
    PPQ playheadPPQ { 0.0 };
    int zoomIndex = DEFAULT_ZOOM_INDEX;
    int hoverY = -1;
    int hoverSection = -1;
    int hoverName = -1;

    // Where the pointer was when a scroll last started. Hover stays off until it is
    // somewhere else, so the move event a scroll leaves behind does not count as pointing.
    bool hoverHeld = false;
    juce::Point<int> hoverHeldFrom;

    bool writeMode = false;
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

    StripMode stripMode() const { return writeMode ? StripMode::Write : StripMode::Play; }
    bool isSelected(int index) const
    { return std::find(selection.begin(), selection.end(), index) != selection.end(); }

    void dispatch(StripCommand cmd, const juce::MouseEvent& e);
    void beginRename(int index);
    void commitRename();
    void commitSections(std::vector<SectionEdit> edits);
    std::vector<SectionEdit> currentEdits() const;
    void drawMarquee(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UtilityStripComponent)
};
