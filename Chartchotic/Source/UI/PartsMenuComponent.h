#pragma once

#include <JuceHeader.h>
#include <vector>
#include "ControlConstants.h"
#include "PartIcon.h"
#include "Theme.h"
#include "../Midi/Discovery/PartMapping.h"
#include "../Visual/Utils/LaneColours.h"
#include "../Visual/Utils/PaletteLanes.h"
#include "../Visual/Utils/PartPalettes.h"

// The parts menu: which REAPER track feeds which instrument, as a grid of tracks down the side
// and parts across the top. Reads a track list and a PartMapping::Map it does not own, so the
// host stays the one talking to REAPER and this stays a view over the answer.
class PartsMenuComponent : public juce::Component
{
public:
    struct TrackRow
    {
        int sourceTrackIndex;
        juce::String name;
        // What the track holds decides which cells in its row mean anything: a part needs
        // MIDI, the waveform needs audio. The row shows either way, the cells do the saying.
        bool hasMidiItems;
        bool hasAudioItems;
        // EVENTS, BEAT, VENUE and the like. Hidden by default because nothing maps to them,
        // but never dropped: showing all tracks has to mean all of them, or the one you are
        // looking for is missing with no way to ask for it.
        bool isUtility;
        // The track this plugin instance is on. Deleting it takes the plugin, the editor and
        // everything mid-callback down with it, so the row never offers the X at all.
        bool hostsPlugin;

        // The list is re-read while the menu is open, so the menu needs to know when the
        // answer is the same one it already has and nothing needs redrawing.
        bool operator==(const TrackRow& o) const
        {
            return sourceTrackIndex == o.sourceTrackIndex && name == o.name
                && hasMidiItems == o.hasMidiItems && hasAudioItems == o.hasAudioItems
                && isUtility == o.isUtility && hostsPlugin == o.hostsPlugin;
        }
    };

    // Takes focus so Escape closes it. Not opaque: the panel sits just short of solid so the
    // highway reads through it, the way the toolbar's own chrome does.
    PartsMenuComponent() { setWantsKeyboardFocus(true); }

    // Told, not asked: the host owns discovery and the map, and hands over what changed.
    std::function<void(Part, int)> onPin;      // part, track index
    // Turn the part off: no pin, and no guess either.
    std::function<void(Part)>      onClearPart;
    // Make a track named for this part and point the part at it.
    std::function<void(Part)>      onCreateTrackForPart;
    // Destructive, so the host confirms before doing anything.
    std::function<void(int, juce::String)> onDeleteTrack;   // track index, name for the prompt
    // The waveform view is not a part, so it gets its own pair rather than a fake Part
    // threaded through every switch that takes one.
    std::function<void(int)>       onPinWaveform;
    std::function<void()>          onUnpinWaveform;
    std::function<void()>          onDismiss;
    // The colour strip under a part's icon was clicked: open that part's palette.
    std::function<void(Part)>      onEditColours;

    // The toolbar's own scale, handed over rather than worked out again here, so a part icon
    // in this table is the same size as the same icon in the header and stays that way at
    // every window size. Taking it from the panel's width instead made the text grow when the
    // strip closed, the rows grow with it, and a list that fit a moment ago start scrolling.
    void setChromeScale(float toolbarScale)
    {
        if (juce::approximatelyEqual(toolbarScale, chromeScale)) return;
        chromeScale = toolbarScale;
        refresh();
    }

    // Says whether it took anything, so a caller polling the project does not relayout and
    // repaint the table twenty times a second over a list that has not moved.
    bool setTracks(std::vector<TrackRow> rows)
    {
        if (rows == tracks) return false;
        tracks = std::move(rows);
        refresh();
        return true;
    }

    void setMap(const PartMapping::Map* m)     { map = m; refresh(); }
    void refresh() { rebuildLayout(); repaint(); }

    // What the table needs if nothing constrained it. The host gives what it can and the
    // difference is what scrolls.
    juce::Point<int> preferredSize() const
    {
        // Header plus everything that scrolls under it, add row and bottom gutter included.
        // Asking for less than the table draws is what pinned the plus row to the bottom edge
        // and made a list that fits scroll anyway.
        return { m.nameColumnW + displayColumns() * m.cellW + m.pad,
                 m.headerH + contentH() };
    }

    void paint(juce::Graphics& g) override
    {
        const auto body = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(Theme::panelNeutralBg).withAlpha(panelAlpha));
        g.fillRoundedRectangle(body, m.corner);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(outlineAlpha));
        g.drawRoundedRectangle(body, m.corner, 1.0f);

        paintTopStrip(g);

        // Columns scroll sideways under a pinned name column, so the icons and the cells are
        // clipped to the same region and share one offset.
        // Under the headers and the cells both, so the block reads as one region rather than
        // two tinted strips. The row stripes and cell fills above it are all translucent.
        paintUtilityBand(g);

        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(gridViewport());
            for (int c = 0; c < displayColumns(); ++c) paintColumnHeader(g, c);
        }
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(cellViewport());
            for (size_t r = 0; r < visibleTracks.size(); ++r) paintRowCells(g, (int)r);
            paintAddRowCells(g);
            paintGridLines(g);
        }

        // Full height, so its own clip rather than the cells'.
        paintUtilityDivider(g);
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(nameViewport());
            for (size_t r = 0; r < visibleTracks.size(); ++r) paintRowName(g, (int)r);
            paintAddRowName(g);
        }

        paintTrackColumnHeader(g);
        paintScrollBars(g);

        if (visibleTracks.empty())
        {
            g.setColour(juce::Colour(Theme::textDim));
            g.setFont(juce::FontOptions().withHeight(m.fontH));
            g.drawText("No tracks in this project",
                       getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void resized() override { rebuildLayout(); }

    void mouseMove(const juce::MouseEvent& e) override
    {
        auto cell = cellAt(e.getPosition());

        // The name column is not a cell, but hovering it still lights the row.
        const bool inNameColumn = e.getPosition().x < nameColumnWidth()
                               && e.getPosition().y >= headerHeight();
        if (cell.row < 0 && inNameColumn)
            cell.row = rowAtY(e.getPosition().y);

        // The colour strips live in the header, which no cell covers, so they are their own
        // hit test rather than a row of column 0.
        const int colourColumn = colourColumnAt(e.getPosition());
        if (colourColumn != hoveredColourColumn)
        {
            hoveredColourColumn = colourColumn;
            repaint();
        }

        // The X belongs to the name, not the row: reaching across a row of cells is not
        // asking about deleting the track, and an X that appears out there is one stray
        // click from taking the track with it.
        const int nameRow = inNameColumn ? cell.row : -1;

        const bool overDelete = canDeleteRow(nameRow)
                             && deleteButtonBounds(nameRow).contains(e.getPosition());

        if (cell != hoverCell || nameRow != hoveredNameRow || overDelete != hoveredOverDelete)
        {
            hoverCell = cell;
            hoveredNameRow = nameRow;
            hoveredOverDelete = overDelete;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverCell != Cell{} || hoveredNameRow >= 0 || hoveredOverDelete)
        {
            hoverCell = {};
            hoveredNameRow = -1;
            hoveredOverDelete = false;
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        // A wheel with no sideways axis still has to reach a column off the right edge, so a
        // vertical wheel scrolls sideways once there is nowhere left to go down.
        const float dx = w.deltaX != 0.0f ? w.deltaX : (maxScrollY() == 0 ? w.deltaY : 0.0f);

        scrollY = juce::jlimit(0, maxScrollY(), scrollY - juce::roundToInt(w.deltaY * m.wheelStep));
        scrollX = juce::jlimit(0, maxScrollX(), scrollX - juce::roundToInt(dx * m.wheelStep));
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Colour strips sit in the header, above every cell, so they answer first.
        const int colourColumn = colourColumnAt(e.getPosition());
        if (colourColumn >= 0)
        {
            if (onEditColours) onEditColours(partForColumn(colourColumn));
            return;
        }

        // The X sits in the name column, outside any cell, so it is checked before them.
        if (hoveredOverDelete && canDeleteRow(hoveredNameRow)
            && deleteButtonBounds(hoveredNameRow).contains(e.getPosition()))
        {
            const auto& track = visibleTracks[(size_t)hoveredNameRow];
            if (onDeleteTrack) onDeleteTrack(track.sourceTrackIndex, track.name);
            return;
        }

        const auto cell = cellAt(e.getPosition());
        if (cell.row < 0 || cell.column < 0 || map == nullptr) return;

        if (isAddRow(cell.row))
        {
            if (canCreateForColumn(cell.column) && onCreateTrackForPart)
                onCreateTrackForPart(partForColumn(cell.column));
            return;
        }

        if (!cellIsLive(cell.row, cell.column)) return;

        const int trackIndex = visibleTracks[(size_t)cell.row].sourceTrackIndex;
        // Right click only ever takes a mapping off, whatever the cell holds.
        const bool clearOnly = e.mods.isPopupMenu();

        if (isWaveformColumn(cell.column))
        {
            const bool held = map->waveformTrack() == trackIndex;
            if (held)            { if (onUnpinWaveform) onUnpinWaveform(); }
            else if (!clearOnly) { if (onPinWaveform)   onPinWaveform(trackIndex); }
            refresh();
            return;
        }

        const auto part = partForColumn(cell.column);

        // Right click turns the part off outright, guess included, which is the only way to
        // say no to something the app decided on its own.
        const auto origin = cellState(trackIndex, part);

        // A guess left-clicks into a pin: the app worked it out from the name and clicking it
        // is you saying yes, which is the whole point of the cell being clickable. Clearing a
        // guess from the left button read as the click undoing the thing it was agreeing to.
        // Only a pin toggles off, and right click still clears whatever is there.
        if (clearOnly || origin == PartMapping::Origin::Pin) { if (onClearPart) onClearPart(part); }
        else if (onPin)                                        onPin(part, trackIndex);

        refresh();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey && onDismiss) { onDismiss(); return true; }
        return false;
    }

private:
    struct Cell
    {
        int row = -1, column = -1;
        bool operator== (const Cell& o) const { return row == o.row && column == o.column; }
        bool operator!= (const Cell& o) const { return !(*this == o); }
    };

    // Everything the table measures with, scaled off the panel's own width the way the utility
    // strip scales off its. Held as one struct so a size change is one recompute, not a dozen
    // multiplications scattered through the paint.
    struct Metrics
    {
        int rowH = 26, headerH = 64, nameColumnW = 180, cellW = 56, pad = 8, numberColumnW = 26;
        int stripH = 22, scrollBarW = 4, wheelStep = 60, iconSize = 28, bottomPad = 12;
        int colourStripH = 8;   // the lane-colour bar under each part icon
        float fontH = 12.0f, stripFontH = 11.0f, hoverFontH = 13.0f, corner = 4.0f;
    };

    // What the toolbar sizes its own circles to (ToolbarComponent.cpp:615). Matching the
    // number is what makes the icons match; if that one moves, this one has to follow.
    static constexpr float toolbarControlHeight = 28.0f;
    // Sits a size under the header. The toolbar holds a handful of controls and this holds a
    // table, so the same size reads as oversized once there are fourteen of them in a row.
    // Applied to the whole panel, not the icons alone, so the proportions hold.
    static constexpr float panelDensity = 0.8f;
    static constexpr float panelAlpha   = 0.94f;     // just short of solid, so the board reads through
    static constexpr float outlineAlpha = 0.3f;      // matches the steppers and segmented buttons
    static constexpr float fadedAlpha   = 0.35f;     // unmapped column, and the guess it holds
    static constexpr float hoverGrowth  = 1.25f;     // the hovered column's name against the rest

    Metrics m;
    std::vector<TrackRow> tracks;
    std::vector<TrackRow> visibleTracks;
    std::vector<Part>     columns;
    std::vector<PartMapping::Assignment> resolved;
    const PartMapping::Map* map = nullptr;
    Cell hoverCell;
    int hoveredNameRow = -1;      // the row whose name is under the cursor, which is what shows the X
    int hoveredColourColumn = -1; // the part whose colour strip is under the cursor
    int scrollX = 0, scrollY = 0;
    float chromeScale = 1.0f;
    bool hoveredOverDelete = false;

    void updateMetrics()
    {
        const float s = (chromeScale > 0.0f ? chromeScale : 1.0f) * panelDensity;
        auto px = [s](float base) { return juce::roundToInt(base * s); };

        m = Metrics{};
        // The header's own control height, so a part icon here is the icon up there.
        m.iconSize    = px(toolbarControlHeight);
        m.cellW       = m.iconSize + px(10);
        m.rowH        = px(24);
        m.stripH      = px(20);
        m.colourStripH = px(8);
        m.headerH      = m.stripH + m.iconSize + px(8) + m.colourStripH;
        m.nameColumnW = px(170);
        m.numberColumnW = px(24);
        m.pad         = px(8);
        m.bottomPad   = px(12);
        m.scrollBarW  = px(4);
        m.wheelStep   = px(60);
        m.fontH       = 12.0f * s;
        m.stripFontH  = 11.0f * s;
        m.hoverFontH  = 11.0f * s * hoverGrowth;
        m.corner      = 4.0f * s;
    }

    // The three regions that scroll differently: icons follow x only, cells follow both, and
    // the name column follows y so it stays put while the parts slide past it.
    juce::Rectangle<int> gridViewport() const
    { return { m.nameColumnW, m.stripH, juce::jmax(0, getWidth() - m.nameColumnW), m.headerH - m.stripH }; }
    juce::Rectangle<int> cellViewport() const
    { return { m.nameColumnW, m.headerH, juce::jmax(0, getWidth() - m.nameColumnW),
               juce::jmax(0, getHeight() - m.headerH) }; }
    juce::Rectangle<int> nameViewport() const
    { return { 0, m.headerH, m.nameColumnW, juce::jmax(0, getHeight() - m.headerH) }; }

    // Display column 0 is the waveform, and the parts follow it. Everything that walks columns
    // walks display indices, so the offset lives here and nowhere else.
    int displayColumns() const { return 1 + (int)columns.size(); }
    static bool isWaveformColumn(int displayColumn) { return displayColumn == 0; }
    Part partForColumn(int displayColumn) const { return columns[(size_t)displayColumn - 1]; }

    // The trailing "add track" row, which is not a track and never scrolls out of reach of
    // being the last thing in the list.
    int addRowIndex() const { return (int)visibleTracks.size(); }
    bool isAddRow(int row) const { return row == addRowIndex(); }

    int contentW() const { return displayColumns() * m.cellW; }
    // The add row counts, and a gutter under it: the plus buttons sitting hard against the
    // panel edge read as a list that got cut off rather than one that ended.
    int contentH() const { return ((int)visibleTracks.size() + 1) * m.rowH + m.bottomPad; }
    int maxScrollX() const { return juce::jmax(0, contentW() - cellViewport().getWidth()); }
    int maxScrollY() const { return juce::jmax(0, contentH() - cellViewport().getHeight()); }

    int columnX(int column) const { return m.nameColumnW + column * m.cellW - scrollX; }
    int rowY(int row) const       { return m.headerH + row * m.rowH - scrollY; }

    // For the hit tests that live outside the grid, in the pinned name column.
    int nameColumnWidth() const { return m.nameColumnW; }
    int headerHeight() const    { return m.headerH; }
    int rowAtY(int y) const
    {
        const int row = (y - m.headerH + scrollY) / m.rowH;
        return (row >= 0 && row <= addRowIndex()) ? row : -1;
    }

    Cell cellAt(juce::Point<int> p) const
    {
        if (p.x < m.nameColumnW || p.y < m.stripH) return {};
        const int column = (p.x - m.nameColumnW + scrollX) / m.cellW;
        if (column < 0 || column >= displayColumns()) return {};

        // Over the icon rather than the grid: the column is known and no row is, which is
        // what names the part on hover without offering a cell to click.
        if (p.y < m.headerH) return { -1, column };

        const int row = (p.y - m.headerH + scrollY) / m.rowH;
        if (row < 0 || row > addRowIndex()) return { -1, column };
        return { row, column };
    }

    std::optional<PartMapping::Origin> cellState(int trackIndex, Part part) const
    {
        for (const auto& a : resolved)
            if (a.part == part && a.sourceTrackIndex == trackIndex) return a.origin;
        return {};
    }

    bool columnIsMapped(Part part) const
    {
        for (const auto& a : resolved)
            if (a.part == part) return true;
        return false;
    }

    // Whether this track could take this column at all. A part needs notes to read and the
    // waveform needs audio to draw, so pointing either at a track without them maps to
    // nothing. Checked per cell because the two columns want opposite things.
    // Anything can be clicked into anything. REAPER has no MIDI track, only a track that
    // happens to hold MIDI right now, so every rule about what a track "is" ends up locking
    // out the empty one you meant to fill. The user's click is the answer; ours is a guess.
    bool cellIsLive(int row, int column) const
    {
        return isWaveformColumn(column) || PartMapping::isMappable(partForColumn(column));
    }

    // -1 utility, 0 active, 1 idle but available, 2 not supported yet. The order the columns
    // read in. Utility sorts first regardless of whether it is mapped, so it lands beside the
    // waveform, which is the other column that is not a part: "which track is my guitar" and
    // "which track is my EVENTS" are different questions, and the two non-instrument lanes
    // answer the second one together.
    int columnGroup(Part part) const
    {
        if (isUtilityPart(part)) return -1;
        if (!PartMapping::isMappable(part)) return 2;
        return columnIsMapped(part) ? 0 : 1;
    }

    // How many leading display columns are not parts: the waveform, plus the utility parts
    // sorted in beside it. Everything from here on is an instrument.
    int utilityColumnCount() const
    {
        int count = 1;   // the waveform
        while (count < displayColumns() && isUtilityPart(partForColumn(count)))
            ++count;
        return count;
    }

    void rebuildLayout()
    {
        updateMetrics();

        // Every track, always. A part wants MIDI and the waveform wants audio, so no single
        // row filter serves both, and hiding rows only ever loses the one you were looking
        // for. What a track cannot do is said by its cells being dead, and the list scrolls.
        // Project order, untouched. The table is a view of the track list, so a row sits
        // where REAPER has it: re-sorting made the numbers jump around and disagree with
        // what you are looking at in the DAW.
        visibleTracks = tracks;

        // Resolved against the rows on screen, not every track in the project. A hidden track
        // whose name reads as a part would otherwise light that column with no row under it to
        // show for it, which is a column claiming a mapping you cannot see or undo.
        // Resolved once per change, not once per cell: painting asks about every track against
        // every part, so resolving inside the cell would sort and allocate a few hundred times
        // a frame for an answer that only moves when a pin does.
        resolved.clear();
        if (map != nullptr)
        {
            std::vector<InstrumentTrackInfo> info;
            for (const auto& t : visibleTracks)
                if (PartMapping::canBeGuessedAsPart(t.hasMidiItems, t.hasAudioItems, t.isUtility))
                    info.push_back({ Part::GUITAR, t.sourceTrackIndex, t.name });
            resolved = map->resolve(info);
        }

        // Three groups left to right: what you are using, what you could use, and what is not
        // supported yet. Inside each group the app's usual part order, so Keys sits left of
        // every struck-through column even though GHL sorts ahead of it overall.
        columns.clear();
        for (auto p : PartMapping::columnParts())
            columns.push_back(p);

        std::sort(columns.begin(), columns.end(), [this](Part a, Part b)
        {
            const int ga = columnGroup(a), gb = columnGroup(b);
            if (ga != gb) return ga < gb;
            return getPartSortOrder(a) < getPartSortOrder(b);
        });

        scrollX = juce::jlimit(0, maxScrollX(), scrollX);
        scrollY = juce::jlimit(0, maxScrollY(), scrollY);
    }

    void paintTopStrip(juce::Graphics& g)
    {
        paintHoveredPartName(g);


    }

    // The column under the cursor, named in full and a size up from the rest of the chrome:
    // five icons stand in for fourteen parts, so the badge alone does not say which one.
    void paintHoveredPartName(juce::Graphics& g)
    {
        if (hoverCell.column < 0 || hoverCell.column >= displayColumns()) return;

        const auto name = isWaveformColumn(hoverCell.column)
                        ? juce::String("Waveform")
                        : getPartDisplayName(partForColumn(hoverCell.column));
        // Starts where the columns do, clear of the scope toggle that sits over the track
        // names, and runs to the panel edge.
        const auto strip = juce::Rectangle<int>(m.nameColumnW, 0,
                                                getWidth() - m.nameColumnW - m.pad, m.stripH);
        if (strip.getWidth() <= 0) return;

        const auto font = juce::Font(juce::FontOptions().withHeight(m.hoverFontH).withStyle("Bold"));
        g.setFont(font);

        // Sized to the text, not to a fixed span of columns. A wide box centred on the column
        // has to be clamped at the edges, and clamping a box that wide slides the text it holds
        // a whole column inwards, which is what put an edge label over its neighbour.
        const int textW = juce::jmin(juce::GlyphArrangement::getStringWidthInt(font, name) + m.pad,
                                     strip.getWidth());
        auto box = juce::Rectangle<int>(columnX(hoverCell.column) + m.cellW / 2 - textW / 2,
                                        strip.getY(), textW, m.stripH);
        box.setX(juce::jlimit(strip.getX(), strip.getRight() - textW, box.getX()));

        g.setColour(juce::Colour(Theme::textWhite));
        g.drawText(name, box, juce::Justification::centred, false);
    }

    void paintTrackColumnHeader(juce::Graphics& g)
    {
        const auto area = juce::Rectangle<int>(m.pad, m.stripH,
                                               m.nameColumnW - m.pad, m.headerH - m.stripH);
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(juce::FontOptions().withHeight(m.stripFontH));
        g.drawText("Track", area, juce::Justification::centredLeft, false);
    }

    void paintColumnHeader(juce::Graphics& g, int column)
    {
        // The icon is its own size, not the column's: the column can be wider without the art
        // growing past what the header draws.
        const auto area = juce::Rectangle<int>(columnX(column), m.stripH, m.cellW,
                                               m.headerH - m.stripH)
                              .withSizeKeepingCentre(m.iconSize, m.iconSize);

        if (isWaveformColumn(column))
        {
            paintWaveformIcon(g, area, map != nullptr && map->waveformTrack().has_value());
            return;
        }

        const auto part = partForColumn(column);
        const bool mappable = PartMapping::isMappable(part);
        const bool lit      = mappable && columnIsMapped(part);

        auto image = PartIcon::get(part, m.iconSize);
        if (image.isValid())
        {
            g.setOpacity(lit ? 1.0f : fadedAlpha);
            g.drawImage(image, area.toFloat(), juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        }
        else
        {
            // Five icons cover the instruments and none of them means EVENTS. Rather than
            // invent art for a track that is not an instrument, the header turns the name on
            // its side, which is the one direction a column this narrow has room in.
            paintSidewaysName(g, column, part, lit);
        }

        paintColourStrip(g, column, part);

        // Nothing renders these, so a mapping would point into a dead end. Struck through
        // rather than hidden, so the table still says the part exists.
        if (!mappable)
        {
            g.setColour(juce::Colour(Theme::coral).withAlpha(0.8f));
            const auto box = area.reduced(m.pad);
            g.drawLine((float)box.getX(), (float)box.getY(),
                       (float)box.getRight(), (float)box.getBottom(), 1.6f);
            g.drawLine((float)box.getRight(), (float)box.getY(),
                       (float)box.getX(), (float)box.getBottom(), 1.6f);
        }
    }

    // The column's name rotated to read bottom-to-top, for the parts with no icon. The text
    // box is laid out in the unrotated space, so its width is the header's height and its
    // height is the column's width, then the whole thing turns about the centre.
    void paintSidewaysName(juce::Graphics& g, int column, Part part, bool lit)
    {
        const auto band = juce::Rectangle<int>(columnX(column), m.stripH, m.cellW,
                                               m.headerH - m.stripH - m.colourStripH);
        if (band.getHeight() <= 0) return;

        juce::Graphics::ScopedSaveState save(g);
        const auto centre = band.toFloat().getCentre();
        const auto box = juce::Rectangle<float>(0.0f, 0.0f,
                                                (float)band.getHeight(), (float)band.getWidth())
                             .withCentre(centre);

        g.addTransform(juce::AffineTransform::rotation(
            -juce::MathConstants<float>::halfPi, centre.x, centre.y));

        g.setColour(juce::Colour(Theme::textDim).withAlpha(lit ? 1.0f : fadedAlpha));
        g.setFont(juce::FontOptions().withHeight(m.stripFontH));
        g.drawText(getPartDisplayName(part).toUpperCase(), box.toNearestInt(),
                   juce::Justification::centred, false);
    }

    // The part's lane colours as a bar under its icon: both what this part is currently drawn
    // with and the way in to changing it. A strip rather than a button because the colours
    // are the label -- a paint icon would say less about this part than the colours do.
    juce::Rectangle<int> colourStripBounds(int column) const
    {
        const auto band = juce::Rectangle<int>(columnX(column), m.headerH - m.colourStripH,
                                               m.cellW, m.colourStripH);
        return band.reduced(m.pad / 2, m.colourStripH / 4);
    }

    void paintColourStrip(juce::Graphics& g, int column, Part part)
    {
        const auto lanes = PaletteLanes::forPart(part);
        if (lanes.empty()) return;   // nothing renders this part, so it has no colours to show

        const auto box = colourStripBounds(column);
        const auto& palette = PartPalettes::global().forPart(part);
        const float w = (float)box.getWidth() / (float)lanes.size();

        for (int i = 0; i < (int)lanes.size(); ++i)
        {
            g.setColour(LaneColours::bright(PaletteLanes::colourOf(palette, lanes[(size_t)i])));
            g.fillRect(juce::Rectangle<float>(box.getX() + w * i, (float)box.getY(),
                                              w, (float)box.getHeight()));
        }

        // Lit only under the cursor, like the row delete: a permanent outline on every column
        // would draw the eye to the chrome rather than to the colours.
        if (hoveredColourColumn == column)
        {
            g.setColour(juce::Colour(Theme::textWhite));
            g.drawRect(box.expanded(1), 1);
        }
    }

    // Drawn rather than an asset: one sine cycle, which is what the waveform view will show.
    // No PNG exists for it and a letter would not read as audio next to the instruments.
    void paintWaveformIcon(juce::Graphics& g, juce::Rectangle<int> box, bool lit)
    {
        const auto area = box.reduced(box.getWidth() / 5).toFloat();
        const float midY = area.getCentreY();
        const float amp  = area.getHeight() * 0.42f;

        juce::Path wave;
        const int steps = 32;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float)i / (float)steps;
            const float x = area.getX() + area.getWidth() * t;
            const float y = midY - amp * std::sin(t * juce::MathConstants<float>::twoPi);
            if (i == 0) wave.startNewSubPath(x, y);
            else        wave.lineTo(x, y);
        }

        g.setColour(juce::Colour(Theme::textWhite).withAlpha(lit ? 1.0f : fadedAlpha));
        g.strokePath(wave, juce::PathStrokeType(juce::jmax(1.5f, (float)box.getWidth() * 0.06f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    void paintAddRowName(juce::Graphics& g)
    {
        const auto area = juce::Rectangle<int>(0, rowY(addRowIndex()), m.nameColumnW, m.rowH);
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(juce::FontOptions().withHeight(m.stripFontH));
        g.drawText("ADD TRACK", area.reduced(m.pad, 0), juce::Justification::centredLeft, false);
    }

    // A plus per part, so the row answers "there is no track for this yet" in the same place
    // the question is asked.
    void paintAddRowCells(juce::Graphics& g)
    {
        const int row = addRowIndex();
        for (int c = 0; c < displayColumns(); ++c)
        {
            if (!canCreateForColumn(c)) continue;

            const auto area = juce::Rectangle<int>(columnX(c), rowY(row), m.cellW, m.rowH);
            const bool hovered = hoverCell == Cell{ row, c };

            if (hovered)
            {
                g.setColour(juce::Colour(Theme::darkBgHover));
                g.fillRect(area.reduced(2));
            }

            const auto plus = area.withSizeKeepingCentre(m.rowH / 3, m.rowH / 3).toFloat();
            g.setColour(juce::Colour(hovered ? Theme::textWhite : Theme::textDim));
            g.drawLine(plus.getCentreX(), plus.getY(), plus.getCentreX(), plus.getBottom(), 1.4f);
            g.drawLine(plus.getX(), plus.getCentreY(), plus.getRight(), plus.getCentreY(), 1.4f);
        }
    }

    // The waveform has no name to give a new track, and a part nothing renders has nowhere
    // to put one, so neither offers a plus.
    //
    // Neither does a part that already has a track. The plus names the new track after the
    // part and pins the part to it, so on an assigned column it would make a second track
    // called PART GUITAR and move the mapping to the empty one, leaving the track with your
    // notes in it sitting there unread. Clear the column first and the plus comes back.
    bool canCreateForColumn(int column) const
    {
        if (isWaveformColumn(column)) return false;
        const auto part = partForColumn(column);
        return PartMapping::isMappable(part) && trackNameFor(part).isNotEmpty()
            && !columnIsMapped(part);
    }

    void paintRowName(juce::Graphics& g, int row)
    {
        const auto& track = visibleTracks[(size_t)row];
        const auto area = juce::Rectangle<int>(0, rowY(row), m.nameColumnW, m.rowH);
        if (row % 2 == 1)
        {
            g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.4f));
            g.fillRect(area);
        }

        g.setFont(juce::FontOptions().withHeight(m.fontH));

        // REAPER's own number rather than the row's, so it still reads right if the table
        // ever shows a subset.
        auto text = area.reduced(m.pad, 0);
        g.setColour(juce::Colour(Theme::textDim));
        g.drawText(juce::String(track.sourceTrackIndex + 1),
                   text.removeFromLeft(m.numberColumnW), juce::Justification::centredLeft, false);

        // The plugin's own row says so instead, always, in the slot the X would have taken:
        // the one row you cannot delete is also the one worth pointing at.
        if (track.hostsPlugin)
            paintHostMarker(g, deleteButtonBounds(row));
        // Only while the cursor is on this row's name: a column of X's invites a mis-click on
        // the one control here that destroys work.
        else if (hoveredNameRow == row)
            paintDeleteButton(g, deleteButtonBounds(row), hoveredOverDelete);

        // Nothing maps to a utility track, so its name sits back with the dead cells.
        g.setColour(juce::Colour(track.isUtility ? Theme::textDim : Theme::textWhite));
        g.drawText(track.name, text.withTrimmedRight(m.rowH), juce::Justification::centredLeft, true);
    }

    // Which part's colour strip is under this point, or -1. Clipped to the scrolling grid the
    // icons live in, so a strip scrolled off the side cannot be clicked through the edge.
    int colourColumnAt(juce::Point<int> p) const
    {
        if (!gridViewport().contains(p)) return -1;

        for (int c = 1; c < displayColumns(); ++c)   // column 0 is the waveform, which has no palette
            if (colourStripBounds(c).expanded(1).contains(p)
                && !PaletteLanes::forPart(partForColumn(c)).empty())
                return c;

        return -1;
    }

    // Deleting the track this plugin sits on takes the plugin, this component and the
    // callback running through it down together, so that row never offers the X.
    bool canDeleteRow(int row) const
    {
        return row >= 0 && row < (int)visibleTracks.size()
            && !visibleTracks[(size_t)row].hostsPlugin;
    }

    juce::Rectangle<int> deleteButtonBounds(int row) const
    {
        return juce::Rectangle<int>(m.nameColumnW - m.rowH, rowY(row), m.rowH, m.rowH)
                   .reduced(m.rowH / 4);
    }

    void paintDeleteButton(juce::Graphics& g, juce::Rectangle<int> box, bool hovered)
    {
        // Red only under the cursor. At rest it is another piece of dim chrome, so a row of
        // them does not read as a warning.
        const auto x = box.reduced(box.getWidth() / 4).toFloat();
        g.setColour(juce::Colour(hovered ? Theme::coral : Theme::textDim));
        g.drawLine(x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.4f);
        g.drawLine(x.getRight(), x.getY(), x.getX(), x.getBottom(), 1.4f);
    }

    // A filled dot inside a ring, in the app's own colour: the plugin is on this track. Drawn
    // rather than an asset so it stays crisp at whatever the row height works out to.
    void paintHostMarker(juce::Graphics& g, juce::Rectangle<int> box)
    {
        const auto ring = box.reduced(box.getWidth() / 5).toFloat();
        g.setColour(juce::Colour(Theme::coral));
        g.drawEllipse(ring, 1.2f);
        g.fillEllipse(ring.reduced(ring.getWidth() * 0.3f));
    }

    void paintRowCells(juce::Graphics& g, int row)
    {
        const auto strip = juce::Rectangle<int>(m.nameColumnW, rowY(row),
                                                getWidth() - m.nameColumnW, m.rowH);
        if (row % 2 == 1)
        {
            g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.4f));
            g.fillRect(strip);
        }

        for (int c = 0; c < displayColumns(); ++c)
            paintCell(g, row, c, visibleTracks[(size_t)row].sourceTrackIndex);
    }

    void paintCell(juce::Graphics& g, int row, int column, int trackIndex)
    {
        const auto area = juce::Rectangle<int>(columnX(column), rowY(row), m.cellW, m.rowH);
        const bool waveform = isWaveformColumn(column);
        const bool live = cellIsLive(row, column);

        if (live && hoverCell == Cell{ row, column })
        {
            g.setColour(juce::Colour(Theme::darkBgHover));
            g.fillRect(area.reduced(2));
        }

        // The waveform is only ever chosen by hand, so it has no guess state to show.
        std::optional<PartMapping::Origin> state;
        if (waveform)
        {
            if (map != nullptr && map->waveformTrack() == trackIndex)
                state = PartMapping::Origin::Pin;
        }
        else state = cellState(trackIndex, partForColumn(column));

        if (!state) return;

        // A pin is the user's answer, so it reads as solid white. A guess is the app's, so it
        // sits back: dimmer, and hollow rather than filled, to say it is not settled.
        const bool pinned = (*state == PartMapping::Origin::Pin);
        const int dotSize = juce::jmax(6, m.rowH / 3);
        const auto dot = area.withSizeKeepingCentre(dotSize, dotSize).toFloat();

        g.setColour(juce::Colour(Theme::textWhite).withAlpha(pinned ? 1.0f : fadedAlpha + 0.25f));
        if (pinned) g.fillEllipse(dot);
        else        g.drawEllipse(dot, 1.4f);
    }

    // A column is a claim on one part, and without a line between them a dot reads as
    // belonging to whichever icon it is nearest.
    // The tinted ground behind every utility column, from the headers to the bottom of the
    // list. Clipped to the grid so it never runs under the pinned name column.
    void paintUtilityBand(juce::Graphics& g)
    {
        const int count = utilityColumnCount();

        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion({ m.nameColumnW, m.stripH,
                             juce::jmax(0, getWidth() - m.nameColumnW),
                             juce::jmax(0, getHeight() - m.stripH) });

        g.setColour(juce::Colour(Theme::panelGroupBg));
        g.fillRect(columnX(0), m.stripH, count * m.cellW,
                   juce::jmax(0, getHeight() - m.stripH));
    }

    // The rule where the instruments stop. Drawn after the grid so it reads as a boundary
    // rather than one more cell edge.
    void paintUtilityDivider(juce::Graphics& g)
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion({ m.nameColumnW, m.stripH,
                             juce::jmax(0, getWidth() - m.nameColumnW),
                             juce::jmax(0, getHeight() - m.stripH) });

        // Where the utility lanes stop and the parts begin.
        g.setColour(juce::Colour(Theme::panelGroupLine));
        g.fillRect((float)columnX(utilityColumnCount()) - 1.0f, (float)m.stripH,
                   2.0f, (float)juce::jmax(0, getHeight() - m.stripH));
    }

    void paintGridLines(juce::Graphics& g)
    {
        g.setColour(juce::Colour(Theme::panelNeutralLine));
        const float top    = (float)m.headerH;
        const float bottom = (float)juce::jmin(getHeight(), rowY((int)visibleTracks.size()));

        for (int c = 0; c <= displayColumns(); ++c)
            g.drawLine((float)columnX(c), top, (float)columnX(c), bottom);

        for (size_t r = 0; r <= visibleTracks.size(); ++r)
            g.drawLine((float)m.nameColumnW, (float)rowY((int)r), (float)getWidth(), (float)rowY((int)r));
    }

    void paintScrollBars(juce::Graphics& g)
    {
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.45f));
        const auto view = cellViewport();

        if (maxScrollY() > 0)
        {
            const float frac = (float)view.getHeight() / (float)contentH();
            const float h = view.getHeight() * frac;
            const float y = view.getY() + (view.getHeight() - h)
                                        * ((float)scrollY / (float)maxScrollY());
            g.fillRoundedRectangle((float)(getWidth() - m.scrollBarW - 2), y,
                                   (float)m.scrollBarW, h, (float)m.scrollBarW * 0.5f);
        }

        if (maxScrollX() > 0)
        {
            const float frac = (float)view.getWidth() / (float)contentW();
            const float w = view.getWidth() * frac;
            const float x = view.getX() + (view.getWidth() - w)
                                        * ((float)scrollX / (float)maxScrollX());
            g.fillRoundedRectangle(x, (float)(getHeight() - m.scrollBarW - 2),
                                   w, (float)m.scrollBarW, (float)m.scrollBarW * 0.5f);
        }
    }
};
