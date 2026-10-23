#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "ChartExporter.h"
#include "IconPreview.h"
#include "../UI/Theme.h"
#include "../UI/Controls/ApplyToAllButton.h"
#include "../UI/Controls/CheckboxToggle.h"
#include "../UI/Controls/DifficultyGrid.h"
#include "../UI/Controls/InfoTooltip.h"
#include "../UI/Controls/InstrumentBadgeStrip.h"
#include "../UI/Controls/PencilButton.h"
#include "../UI/Controls/SegmentedButtons.h"
#include "../UI/Controls/ValueMenu.h"
#include "../UI/LookAndFeel/MenuLookAndFeel.h"

/**
    The export dialog: pick which songs to export, and what goes in them.

    Songs are project regions, which is what lets one album project hold eleven
    charts. Nothing is imposed on how regions are named: a region's name is the
    default title and nothing more, and which rows are ticked is the whole of
    the selection, so a project can be laid out however suits it.

    Fields are split by what they belong to. Artist, album, year, genre,
    artwork and packaging are the same for every song on an album and are asked
    once; title, track number and difficulty belong to one song and follow its
    region around.

    It collects and validates only. Nothing here writes a file; Export hands
    back one SongExport per ticked row and the caller runs them.

    Fills whatever window it is given rather than drawing its own card. See
    ExportWindow for why it stopped being an overlay inside the editor.
*/
class ExportDialogComponent : public juce::Component,
                             private juce::Timer
{
public:
    /**
        What the window re-reads from REAPER while it is open.

        The project carries on being edited with this on screen: a time
        selection gets made, a region gets added or renamed. Reading it once at
        open time meant the window sat there insisting there was nothing to
        export while the answer was right there on the timeline.
    */
    struct Snapshot
    {
        ChartExporter::TimeRange selection;
        std::vector<ChartExporter::Region> regions;
        juce::StringArray trackNames;
    };
    struct Context
    {
        /** Used when the project has no regions at all. */
        ChartExporter::TimeRange selection;
        std::vector<ChartExporter::Region> regions;

        ChartExporter::ChartName inferred;
        juce::StringArray trackNames;
        std::vector<ChartExporter::Sink> audioFormats;
        juce::File destinationRoot;
        juce::Array<juce::File> artworkSearchPaths;
        ChartMidiWriter::DrumProfile drums;

        /** Album-wide values, with a child per song keyed by region GUID. */
        juce::ValueTree remembered;
        juce::String charter, icon;
    };

    explicit ExportDialogComponent(Context context);

    /** Clears the background staging folder. Nothing there outlives the window. */
    ~ExportDialogComponent() override;

    /**
        The size, which is the only size.

        Every column in here is sized to its content rather than to a share of
        the window, so there is one width at which it all fits and nothing is
        left over. Dragging the window wider only ever made the gaps bigger.
    */
    static constexpr int kWidth = 1080;
    static constexpr int kHeight = 900;

    /**
        Polled a few times a second. Cheap things only: enumerating regions and
        reading the time selection, not scanning notes.
    */
    std::function<Snapshot()> pollProject;

    /**
        Called only when the ranges actually change, since it reads every note
        on every drum track and is far too expensive to poll.
    */
    std::function<ChartMidiWriter::DrumProfile(const ChartExporter::TimeRange&)> analyseDrums;

    /**
        Which instruments carry notes inside a range.

        An album project holds every instrument on one set of tracks, and only
        some songs use each of them, so what the project has and what a song
        has are different questions. Reads notes, so it is asked once per
        selection rather than per frame.
    */
    std::function<juce::StringArray(const ChartExporter::TimeRange&)> analyseParts;

    /** Fired with one entry per ticked song. The dialog is still on screen. */
    std::function<void(const std::vector<ChartExporter::SongExport>&)> onExport;
    std::function<void()> onDismiss;

    /**
        Everything typed, whenever it changes, for the host to keep.

        Saving only on the way out meant a session that ended any other way
        (a crash, a reload, quitting REAPER with the window open) threw away
        every title typed into it. Coalesced onto the timer, so holding a key
        down does not write the tree per keystroke.
    */
    std::function<void(const juce::ValueTree&)> onStateChanged;

    /**
        Make the time selection a region, returning its GUID. Offered whenever
        the selection is not already inside one; export still makes any region
        it needs, so this is the deliberate way rather than the only way.
    */
    std::function<juce::String(const ChartExporter::TimeRange&, const juce::String& name)> createRegion;

    /**
        Drive the project's time selection from the list.

        Selecting a song and selecting its time are the same act: clicking a
        row moves the timeline to it, and losing the time selection drops the
        row. Multiple songs selected at once is its own state and does not
        try to express itself as one range.
    */
    std::function<void(const ChartExporter::TimeRange&)> setTimeSelection;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    /** Watches for the pointer entering the charter line, which shows its pencil. */
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    /** Clicking anywhere along the charter line opens it. */
    void mouseDown(const juce::MouseEvent& e) override;

    /**
        Takes on the regions the export just made, so the settings saved on
        close key to them rather than to a time selection that is about to
        stop being the way this song is identified.
    */
    void adoptRegions(const juce::StringPairArray& folderNameToGuid);

    /** Everything typed here, so reopening starts where it left off. */
    juce::ValueTree toValueTree() const;

    /** False while the icon lookup is still in flight. For snapshot tests. */
    bool iconSettled() const { return iconPreview.settled(); }

    /** Select every song, as shift-clicking the rail end to end would. For
        snapshot tests, which cannot otherwise reach the mixed state. */
    void selectAllForEditing();

    /** Open the settings form, as pressing Edit would. For snapshot tests. */
    void openSettingsForTests() { charterPencil.triggerClick(); }

    /** Turn re-rendering off, as clicking the box would. For snapshot tests. */
    void setRenderAudioForTests(bool on)
    {
        renderAudioToggle.setToggleState(on, juce::sendNotification);
    }

    /** Roll the wheel over a difficulty badge. For snapshot tests. */
    void adjustRatingForTests(int index, int delta)
    {
        if (difficulties.onAdjustRating) difficulties.onAdjustRating(index, delta);
    }

private:
    /**
        A text box that can report that the selected songs disagree.

        Drawn over the editor rather than through setTextToShowWhenEmpty,
        which cannot italicise the marker.
    */
    struct MixedTextEditor : public juce::TextEditor
    {
        void paintOverChildren(juce::Graphics& g) override;

        /** The selected songs hold different values, so none is shown. */
        bool mixed = false;

        /**
            The value shown is not on all of them yet.

            Drawn in the same italic the mixed marker uses, because it is the
            same claim: what is in the box is not the whole truth about the
            selection. Here there is a truth to be had, which is what the tick
            beside it is for.
        */
        bool partial = false;
    };

    /** A label and a text box, which is most of this dialog. */
    struct Field : public juce::Component
    {
        Field(const juce::String& labelText, bool numeric = false);
        void paint(juce::Graphics& g) override;
        void resized() override;

        juce::String text() const { return editor.getText().trim(); }

        /**
            The user typed, so the box speaks for the whole selection again.

            Every keystroke is written to all of them, so a value that arrived
            partial stops being partial the moment it is edited.
        */
        void clearPartial()
        {
            if (!editor.partial) return;
            editor.partial = false;
            applyAll.setVisible(false);
            refreshFont();
        }

        /** Every selected song holds this, so it is simply the value. */
        void setText(const juce::String& value)
        {
            editor.mixed = false;
            editor.partial = false;
            applyAll.setVisible(false);
            editor.setText(value, false);   // no notification: not a user edit
            refreshFont();
        }

        /**
            Some hold this and the rest hold nothing.

            Shown rather than hidden, since it is the only value in play, but
            marked as not yet true of everything and offered as a tick.
        */
        void setPartial(const juce::String& value)
        {
            editor.mixed = false;
            editor.partial = true;
            editor.setText(value, false);
            applyAll.setVisible(true);
            refreshFont();
        }

        /** Show the marker. Cleared by setText and by the user typing. */
        void setMixed()
        {
            editor.setText({}, false);
            editor.mixed = true;
            editor.partial = false;
            applyAll.setVisible(false);
            refreshFont();
            editor.repaint();
        }

        bool isMixed() const { return editor.mixed; }
        bool isPartial() const { return editor.partial; }

        /** Fired by the tick: put what is shown on every selected song. */
        std::function<void()> onApplyToAll;

        /**
            Label gutter in pixels, not a proportion of this field's width.
            Shared by every field in a panel so the boxes line up.
        */
        int labelWidth = 66;

        /**
            How wide the box is allowed to get, or 0 to fill what is left.

            A year is four characters and was being handed the whole panel.
            Sizing the box to the value is most of what stops this reading as
            a form dump.
        */
        int boxWidth = 0;

        /**
            Labels against the boxes rather than against the left margin, the
            way a tag editor sets them out. One ragged edge instead of two.
        */
        bool labelRight = false;

        juce::String label;
        MixedTextEditor editor;
        ApplyToAllButton applyAll;

    private:
        /** Italic while the box is speaking for only part of the selection. */
        void refreshFont();
    };

    /**
        The whole project on one line: every region, in time, to scale.

        A readout, not a control. Regions that contain other regions are the
        one overlap that means anything (an album holding its own tracks, a
        take with the intro holding the take without), and one lane cannot
        show that, so for now it draws the outermost and leaves the room.
    */
    struct TimelineStrip : public juce::Component
    {
        struct Block
        {
            juce::String name;
            double startSec = 0.0, endSec = 0.0;
            bool ticked = false;
            bool current = false;

            /** Which song this draws, so a click on it can select that song. */
            int song = -1;
        };

        TimelineStrip();
        void paint(juce::Graphics& g) override;
        void setBlocks(std::vector<Block> newBlocks, const ChartExporter::TimeRange& selection);

        /** Picking a song off the strip, with the rail's modifier rules. */
        void mouseDown(const juce::MouseEvent& e) override;
        std::function<void(int song, const juce::ModifierKeys&)> onSelect;

        std::vector<Block> blocks;
        ChartExporter::TimeRange selection;
        double endSec = 0.0;
    };

    /**
        An artwork slot: heading, then the picture.

        The picture is the control. Clicking it picks a file, and a clear badge
        appears over its corner on hover, so there is no button row competing
        with the thing it acts on.
    */
    struct ArtSlot : public juce::Component
    {
        ArtSlot(const juce::String& labelText);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void setFile(const juce::File& file);

        /** The tick badge over the picture's corner. */
        static constexpr int applyAllSize = 26;

        void mouseDown(const juce::MouseEvent& e) override;

        /** Copy, paste, spread across the selection, clear. */
        void showContextMenu();

        void mouseMove(const juce::MouseEvent& e) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override
        {
            hovering = false;
            overClear = false;
            repaint();
        }

        /** The picture itself, below the heading and above the filename. */
        juce::Rectangle<int> previewBounds() const;

        /** Top-right of the picture, and only while hovering something. */
        juce::Rectangle<int> clearBounds() const;

        /** Top-left of the picture. Only the slot that has options shows one. */
        juce::Rectangle<int> optionsBounds() const;

        /**
            Where this file will be once the chart is written. Generated
            artwork lives somewhere temporary until then, and showing that
            path says nothing useful about what is going to ship.
        */
        juce::String caption;

        bool hasOptions = false;
        std::function<void()> onOptions;

        /** The selected songs have different artwork, so none is shown. */
        void setMixed(bool isMixed) { mixed = isMixed; repaint(); }

        /**
            Some of the selection has this picture and the rest have none.

            Bordered rather than italicised, since a picture cannot lean, and
            offered through the same tick the fields use.
        */
        void setPartial(bool isPartial) { partial = isPartial; applyAll.setVisible(isPartial); repaint(); }

        /** Put this picture on every selected song. */
        std::function<void()> onApplyToAll;

        /**
            Copy this picture, and paste one copied from another song.

            An album's songs mostly share a cover, and picking the same file
            out of a chooser eleven times is not how anyone wants to say so.
        */
        std::function<void(const juce::File&)> onCopy;
        std::function<juce::File()> onPaste;

        ApplyToAllButton applyAll;
        bool partial = false;

        /**
            Fired when the user picks or clears a picture, never when the slot
            is filled from the selection. Loading must not look like editing.
        */
        std::function<void(const juce::File&)> onFileChanged;

        juce::String label;
        juce::File file;
        bool mixed = false;
        bool hovering = false;
        bool overClear = false;
        juce::Image thumbnail;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    /**
        One song and everything about it.

        All metadata lives here, not on the album: a session can be a
        compilation or a split, not just one record. Only the charter identity
        is session-wide. Repeated typing is avoided by the fields editing every
        selected song at once.
    */
    struct Song
    {
        ChartExporter::Region region;

        /** Ticked for export. Distinct from being selected for editing. */
        bool selected = false;

        juce::String title, track;
        juce::String artist, album, genre, year;
        int diffDrums = ChartExporter::ExportOptions::kUnrated;
        int diffPro = ChartExporter::ExportOptions::kUnrated;

        /**
            Elite drums, which is its own tier and not a view of the others.

            song.ini carries diff_elite_drums separately from diff_drums and
            diff_drums_real. Without somewhere of its own to live, a rating
            typed on the elite badge had nowhere to go and nothing to come
            back from.
        */
        int diffElite = ChartExporter::ExportOptions::kUnrated;
        /**
            Read off this song's own notes, not off the project.

            An album can hold a pro chart and a 5-lane chart, so asking one
            range and seeding every song from the answer got one of them
            wrong. Re-read whenever the song is selected, since the chart is
            being edited while this window is open.
        */
        bool proDrums = true;
        bool fiveLane = false;

        /**
            The type was set by hand, so reading the notes must not undo it.

            The one thing detection cannot see is a chart meant to be pro that
            has no tom markers to prove it, which is exactly when someone
            reaches for the switch.
        */
        bool drumTypeForced = false;
        juce::File albumArtFile, backgroundFile;

        /** The chart already on disk for this song, if there is one. */
        juce::File exported;

        /** Its song.ini has been read, so it is not read again every refresh. */
        bool adoptedFromDisk = false;

        /** Empty when ready, otherwise why it cannot be exported. */
        juce::String blocker(bool charterNamed, bool drums) const;
    };

    /** A row in the song list: tick, track number, name, and whether it is ready. */
    struct SongRow : public juce::Component
    {
        SongRow();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override { if (onSelect) onSelect(e.mods); }

        CheckboxToggle tick { "" };

        /** Track number, shown on every row so gaps and duplicates show up. */
        juce::String number;

        /**
            Where this song sits on the timeline, and what region it is. An
            untitled row is otherwise blank and two of them are then
            indistinguishable.
        */
        juce::String timecode, source;

        juce::String name, status;
        bool untitled = false;
        bool ready = false;

        /** Being edited by the fields on the right. Not the same as ticked. */
        bool current = false;

        bool exported = false;
        std::function<void(const juce::ModifierKeys&)> onSelect;
    };

    void timerCallback() override;
    void applySnapshot(const Snapshot& snapshot);
    bool differsFromCurrent(const Snapshot& snapshot) const;
    void rebuildRows();

    /** Rebuild the rail's order and headings from the current metadata. */
    void regroup();

    /** Fill every control from the selection, marking disagreements mixed. */
    void loadSelection();

    /**
        Apply one field to every selected song. The only write path, which is
        what keeps a mixed field mixed until the user edits it.
    */
    void applyToSelection(std::function<void(Song&)> change);

    /** Click, shift-click and cmd-click, in the rail's display order. */
    void selectSong(int index, const juce::ModifierKeys& mods);

    void refreshRows();

    /**
        Read the selected songs' drums and set their type from the notes.

        Run on every selection rather than once per song: reading one range is
        quick, and the chart is being edited while this window is open.
    */
    void detectDrumTypes();

    /**
        Fill anything still blank from the chart already exported for this song.

        A chart that has shipped carries its own metadata and its own ratings,
        so asking for them again is asking someone to remember what they
        already published.
    */
    void adoptExported(Song& song);

    /** Feed the top strip the current regions, ticks and selection. */
    void rebuildTimeline();

    void refreshValidity();
    void prefillArtwork();
    void restore(const juce::ValueTree& tree);

    ChartExporter::ExportOptions optionsFor(const Song& song) const;
    juce::String albumBlocker() const;

    /** How many ticked songs would actually be written if Export were pressed. */
    int readyCount() const;

    /** True when the time selection lies within a region that already exists. */
    bool selectionInsideRegion() const;

    /**
        Render a background from each selected song's album art in the chosen
        template and hand it to that song. Writes a real file, so export ships
        the image that was on screen rather than making its own later.

        With onlyMissing, songs that already have a background are left alone.
    */
    void generateBackgrounds(bool onlyMissing = false);

    /**
        Every rect paint() draws into, measured once in resized().

        One rect per thing drawn, and no rect is shared: paint() reads these
        and never slices them. Sharing a rect between the two was how a change
        to one section moved a heading in another.
    */
    struct PaintRects
    {
        juce::Rectangle<int> title, projectSummary;
        juce::Rectangle<int> railPanel, railHeading;
        juce::Rectangle<int> selectionHeading, selectionRange, selectionLength, selectionContents;
        juce::Rectangle<int> songPanel, songHeading, metaHeading, chartHeading, drumsHeading;
        juce::Rectangle<int> summaryLine, charterLine;
    };
    PaintRects ui;

    /** Fills PaintRects and places the controls each one owns. */
    void layoutHeader(juce::Rectangle<int> area);
    void layoutFooter(juce::Rectangle<int>& content);
    void layoutRail(juce::Rectangle<int> area);
    void layoutSongPane(juce::Rectangle<int> area);

    /** Panel background and heading rule, in the one place that draws them. */
    void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawHeading(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text) const;

    /**
        What the time selection currently covers, and what is in it.

        Reading every note on every drum track is far too expensive to do while
        a selection is being dragged, so it is analysed only once the range has
        stopped moving: lastPolledSelection is the previous tick's range and
        analysedSelection is the one selectionDrums describes.
    */
    ChartExporter::TimeRange lastPolledSelection, analysedSelection;
    ChartMidiWriter::DrumProfile selectionDrums;

    Context context;
    std::vector<Song> songs;

    /** Songs the fields are editing, by index into songs. Never empty. */
    std::set<int> editing;

    /** Where a shift-click measures its range from. */
    int anchor = 0;

    bool hasDrums = false;

    /** Something worth remembering changed, and the timer has not saved it yet. */
    bool stateDirty = false;

    juce::Viewport listView;
    juce::Component listContent;
    std::vector<std::unique_ptr<SongRow>> rows;

    /** Song indices per release, and the rail's order those produce. */
    std::vector<std::vector<int>> groups;
    std::vector<int> displayOrder;

    // Per song. Metadata first, then what the chart itself is.
    Field titleField { "Title" };
    Field artistField { "Artist" };
    Field albumField { "Album" };
    Field yearField { "Year", true };
    Field trackField { "Track", true };
    Field genreField { "Genre" };

    CheckboxToggle proDrumsToggle { "Pro drums" };
    CheckboxToggle fiveLaneToggle { "5-lane drums" };

    /** Every instrument the chart could hold, and how hard each one is. */
    DifficultyGrid difficulties;

    /** Fill the grid from the project's tracks and the selected songs. */
    void refreshDifficulties();

    /**
        Where a badge's rating is kept, or null when it is kept nowhere.

        Only the drums have somewhere to put a number: the chart format keeps
        diff_drums and diff_drums_real, and nothing on this side of the export
        holds a rating for guitar or bass yet. Their badges show what the
        project has and take no number until they do.
    */
    int Song::* ratingMemberFor(const DifficultyGrid::Entry& entry) const;

    /** What the project holds, beside the title. */
    InstrumentBadgeStrip instruments;

    TimelineStrip timeline;

    // Session wide
    Field charterField { "Charter" };
    Field iconField { "Icon" };

    /**
        Where the charts land, which is editable rather than reported.

        Defaults to an export folder beside the .rpp, which is right nearly
        always. Nearly is not always: a project that has never been saved has
        no folder to be beside, and then this is the only way to say where.
    */
    Field outputField { "Output" };
    IconPreview iconPreview;

    ArtSlot albumArt { "ALBUM ART" };
    ArtSlot backgroundArt { "BACKGROUND" };

    /** Artwork found beside the project, used to seed songs as they appear. */
    juce::File prefilledAlbumArt, prefilledBackground;

    /** The picture last copied off a slot, for pasting onto another song. */
    juce::File copiedArtwork;

    /**
        What this export does, as opposed to what the chart is.

        Three answers on one line: whether the audio is rendered at all, what
        it is encoded as, and whether the result is a folder or a container.
        Only the first is worth a control you can hit without aiming.
    */
    CheckboxToggle renderAudioToggle { "Render audio" };
    ValueMenu formatMenu;
    ValueMenu packagingMenu;

    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton createRegionButton { "Create region" };

    /**
        Who we are and where the files go, folded into one line.

        These are set once and then never touched, so as a form they were four
        labelled rows of window spent on settings nobody is editing. Collapsed
        they are a summary; the button opens them when something does change.
    */
    /**
        Explains the settings whose names do not.

        Pro drums and 5-lane are format tags with real consequences and names
        that say nothing about them, and this window is where someone meets
        them for the first time. Below each toggle rather than beside it, so
        the card never covers the one you are comparing against.
    */
    InfoTooltip proDrumsHelp, fiveLaneHelp;

    /** Shared by every menu in here, so they all belong to the same window. */
    MenuLookAndFeel menuLook;

    PencilButton charterPencil;
    bool settingsOpen = false;

    /** True while the pointer is over the charter line, which shows the pencil. */
    bool overCharterLine = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialogComponent)
};
