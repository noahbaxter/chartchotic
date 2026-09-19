#pragma once

#include <JuceHeader.h>
#include <vector>
#include "ControlConstants.h"
#include "Theme.h"
#include "../Midi/Discovery/PartMapping.h"

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
    };

    // Takes focus so Escape closes it. Not opaque: the panel sits just short of solid so the
    // highway reads through it, the way the toolbar's own chrome does.
    PartsMenuComponent() { setWantsKeyboardFocus(true); }

    // Told, not asked: the host owns discovery and the map, and hands over what changed.
    std::function<void(Part, int)> onPin;      // part, track index
    // Turn the part off: no pin, and no guess either.
    std::function<void(Part)>      onClearPart;
    // The waveform view is not a part, so it gets its own pair rather than a fake Part
    // threaded through every switch that takes one.
    std::function<void(int)>       onPinWaveform;
    std::function<void()>          onUnpinWaveform;
    std::function<void()>          onDismiss;

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

    void setTracks(std::vector<TrackRow> rows) { tracks = std::move(rows); refresh(); }
    void setMap(const PartMapping::Map* m)     { map = m; refresh(); }
    void refresh() { rebuildLayout(); repaint(); }

    // What the table needs if nothing constrained it. The host gives what it can and the
    // difference is what scrolls.
    juce::Point<int> preferredSize() const
    {
        return { m.nameColumnW + displayColumns() * m.cellW + m.pad,
                 m.headerH + (int)visibleTracks.size() * m.rowH + m.pad };
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
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(gridViewport());
            for (int c = 0; c < displayColumns(); ++c) paintColumnHeader(g, c);
        }
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(cellViewport());
            for (size_t r = 0; r < visibleTracks.size(); ++r) paintRowCells(g, (int)r);
            paintGridLines(g);
        }
        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(nameViewport());
            for (size_t r = 0; r < visibleTracks.size(); ++r) paintRowName(g, (int)r);
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
        const auto cell = cellAt(e.getPosition());
        if (cell != hoverCell) { hoverCell = cell; repaint(); }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverCell != Cell{}) { hoverCell = {}; repaint(); }
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
        const auto cell = cellAt(e.getPosition());
        if (cell.row < 0 || cell.column < 0 || map == nullptr) return;

        if (!cellIsLive(cell.row, cell.column)) return;

        const int trackIndex = visibleTracks[(size_t)cell.row].sourceTrackIndex;
        // Right click only ever takes a mapping off, whatever the cell holds. Left click
        // toggles, which is quicker but means the same click both sets and clears.
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
        // say no to something the app decided on its own. Left click on the cell that already
        // holds it does the same; anywhere else claims it from whichever track had it.
        const bool held = cellState(trackIndex, part).has_value();

        if (clearOnly || held) { if (onClearPart) onClearPart(part); }
        else if (onPin)          onPin(part, trackIndex);

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
        int stripH = 22, scrollBarW = 4, wheelStep = 60, iconSize = 28;
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
    static constexpr float badgeScale   = 0.42f;     // of the icon box
    static constexpr float fadedAlpha   = 0.35f;     // unmapped column, and the guess it holds
    static constexpr float hoverGrowth  = 1.25f;     // the hovered column's name against the rest

    Metrics m;
    std::vector<TrackRow> tracks;
    std::vector<TrackRow> visibleTracks;
    std::vector<Part>     columns;
    std::vector<PartMapping::Assignment> resolved;
    const PartMapping::Map* map = nullptr;
    Cell hoverCell;
    int scrollX = 0, scrollY = 0;
    float chromeScale = 1.0f;

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
        m.headerH     = m.stripH + m.iconSize + px(8);
        m.nameColumnW = px(170);
        m.numberColumnW = px(24);
        m.pad         = px(8);
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

    int contentW() const { return displayColumns() * m.cellW; }
    int contentH() const { return (int)visibleTracks.size() * m.rowH; }
    int maxScrollX() const { return juce::jmax(0, contentW() - cellViewport().getWidth()); }
    int maxScrollY() const { return juce::jmax(0, contentH() - cellViewport().getHeight()); }

    int columnX(int column) const { return m.nameColumnW + column * m.cellW - scrollX; }
    int rowY(int row) const       { return m.headerH + row * m.rowH - scrollY; }

    Cell cellAt(juce::Point<int> p) const
    {
        if (p.x < m.nameColumnW || p.y < m.stripH) return {};
        const int column = (p.x - m.nameColumnW + scrollX) / m.cellW;
        if (column < 0 || column >= displayColumns()) return {};

        // Over the icon rather than the grid: the column is known and no row is, which is
        // what names the part on hover without offering a cell to click.
        if (p.y < m.headerH) return { -1, column };

        const int row = (p.y - m.headerH + scrollY) / m.rowH;
        if (row < 0 || row >= (int)visibleTracks.size()) return { -1, column };
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

    // 0 active, 1 idle but available, 2 not supported yet. The order the columns read in.
    int columnGroup(Part part) const
    {
        if (!PartMapping::isMappable(part)) return 2;
        return columnIsMapped(part) ? 0 : 1;
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

        const auto icon = getPartIcon(part);
        if (icon.data != nullptr)
        {
            auto image = juce::ImageCache::getFromMemory(icon.data, icon.size);
            if (image.isValid())
            {
                g.setOpacity(lit ? 1.0f : fadedAlpha);
                g.drawImage(image, area.toFloat(), juce::RectanglePlacement::centred);
                g.setOpacity(1.0f);
                paintBadge(g, area, part, lit);
            }
        }

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

    void paintBadge(juce::Graphics& g, juce::Rectangle<int> iconBox, Part part, bool lit)
    {
        const auto badge = getPartBadge(part);
        if (badge.isEmpty()) return;

        const int side = juce::roundToInt(iconBox.getWidth() * badgeScale);
        const auto box = juce::Rectangle<int>(iconBox.getRight() - side, iconBox.getBottom() - side,
                                              side, side);
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(lit ? 0.95f : fadedAlpha));
        g.fillEllipse(box.toFloat());
        g.setColour(juce::Colour(Theme::textWhite).withAlpha(lit ? 1.0f : fadedAlpha));
        g.setFont(juce::Font(juce::FontOptions().withHeight((float)side * 0.8f).withStyle("Bold")));
        g.drawText(badge, box, juce::Justification::centred, false);
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

        // Nothing maps to a utility track, so its name sits back with the dead cells.
        g.setColour(juce::Colour(track.isUtility ? Theme::textDim : Theme::textWhite));
        g.drawText(track.name, text, juce::Justification::centredLeft, true);
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
