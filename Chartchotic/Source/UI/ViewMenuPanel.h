#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>
#include "Theme.h"
#include "../Visual/ViewLayers.h"

// The View menu's layer list. Collapsed it is one row per group with one toggle; a group opens
// in place to its layers, and only then, with the utility strip showing, does it split into a
// highway and a strip column. Chart rules sit under their own row the toolbar lays out below.
class ViewMenuPanel : public juce::Component
{
public:
    using Layer = ViewLayers::Layer;
    using Group = ViewLayers::Group;
    using Surface = ViewLayers::Surface;

    std::function<void(const ViewOptions&)> onViewChanged;
    std::function<void(bool)> onUtilityStripToggled;
    // Height changed: a group or the chart rules opened or closed.
    std::function<void()> onLayoutChanged;

    void setView(const ViewOptions& v) { view = v; repaint(); }
    void setUtilityStripVisible(bool on) { stripVisible = on; relayout(); }
    // Called from inside the toolbar's panel layout, so it must not ask for another layout.
    void setScale(float s) { if (s != scale) { scale = s; repaint(); } }

    bool rulesExpanded() const { return rulesOpen; }

    int preferredHeight() const
    {
        return (int)buildRows().size() * rowH() + dividerH();
    }

    void paint(juce::Graphics& g) override
    {
        const auto rows = buildRows();
        int y = 0;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& row = rows[i];
            if (row.kind == RowKind::StripToggle)
            {
                g.setColour(juce::Colour(Theme::textDim).withAlpha(dividerAlpha));
                g.fillRect(0.0f, (float)y + dividerH() * 0.5f, (float)getWidth(), 1.0f);
                y += dividerH();
            }

            const auto area = juce::Rectangle<int>(0, y, getWidth(), rowH());
            if ((int)i == hoveredRow && row.kind != RowKind::ColumnHeader)
            {
                g.setColour(juce::Colour(Theme::darkBgHover));
                g.fillRoundedRectangle(area.toFloat(), Theme::cornerRadius);
            }
            paintRow(g, row, area);
            y += rowH();
        }
    }

    void mouseMove(const juce::MouseEvent& e) override { setHovered(rowAt(e.y)); }
    void mouseExit(const juce::MouseEvent&) override { setHovered(-1); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition())) return;
        const auto rows = buildRows();
        const int index = rowAt(e.y);
        if (index < 0) return;
        const auto& row = rows[(size_t)index];

        switch (row.kind)
        {
            case RowKind::Group:
            {
                if (auto s = surfaceAt(e.x, row); s.has_value())
                    toggleGroup(row.group, surfacesFor(row, *s));
                else if (expandable(row.group))
                {
                    expanded = expanded == (int)row.group ? -1 : (int)row.group;
                    relayout();
                }
                else
                    toggleGroup(row.group, surfacesFor(row, std::nullopt));
                break;
            }
            case RowKind::Layer:
            {
                auto s = surfaceAt(e.x, row).value_or(Surface::Highway);
                if (!ViewLayers::applies(row.layer, s)) return;
                view.set(row.layer, s, !view.has(row.layer, s));
                changed();
                break;
            }
            case RowKind::StripToggle:
                stripVisible = !stripVisible;
                if (onUtilityStripToggled) onUtilityStripToggled(stripVisible);
                relayout();
                break;
            case RowKind::Rules:
                rulesOpen = !rulesOpen;
                relayout();
                break;
            case RowKind::ColumnHeader:
                break;
        }
    }

    // Every time the menu opens it opens simple: whatever was expanded last time is closed.
    void parentHierarchyChanged() override
    {
        expanded = -1;
        rulesOpen = false;
        hoveredRow = -1;
    }

private:
    enum class RowKind { Group, ColumnHeader, Layer, StripToggle, Rules };

    struct Row
    {
        RowKind kind;
        Group group = Group::Notes;
        Layer layer = Layer::Gems;
    };

    enum class Tri { None, Some, All };

    ViewOptions view = ViewOptions::defaults();
    bool stripVisible = true;
    bool rulesOpen = false;
    int expanded = -1;
    int hoveredRow = -1;
    float scale = 1.0f;

    static constexpr float rowHeightRef = 26.0f;
    static constexpr float dividerHeightRef = 9.0f;
    static constexpr float toggleSizeRef = 13.0f;
    static constexpr float columnWidthRef = 48.0f;
    static constexpr float indentRef = 14.0f;
    static constexpr float padRef = 6.0f;
    static constexpr float groupFontRef = 13.0f;
    static constexpr float layerFontRef = 12.0f;
    static constexpr float headerFontRef = 10.0f;
    static constexpr float dividerAlpha = 0.2f;
    static constexpr float offOutlineAlpha = 0.35f;
    static constexpr float layerTextAlpha = 0.85f;

    int rowH() const { return juce::roundToInt(rowHeightRef * scale); }
    int dividerH() const { return juce::roundToInt(dividerHeightRef * scale); }
    float px(float ref) const { return ref * scale; }

    std::vector<Layer> layersIn(Group g) const
    {
        std::vector<Layer> out;
        for (const auto& row : ViewLayers::table)
            if (row.group == g) out.push_back(row.id);
        return out;
    }

    bool expandable(Group g) const { return layersIn(g).size() > 1; }

    // Two columns only once someone has opened a group and the strip is there to aim at.
    bool splitColumns() const { return stripVisible && expanded >= 0; }

    std::vector<Row> buildRows() const
    {
        std::vector<Row> rows;
        for (int gi = 0; gi < ViewLayers::groupCount; ++gi)
        {
            const auto g = (Group)gi;
            rows.push_back({ RowKind::Group, g });
            if (expanded != gi) continue;
            if (splitColumns()) rows.push_back({ RowKind::ColumnHeader, g });
            for (auto l : layersIn(g))
                rows.push_back({ RowKind::Layer, g, l });
        }
        rows.push_back({ RowKind::StripToggle });
        rows.push_back({ RowKind::Rules });
        return rows;
    }

    int rowAt(int y) const
    {
        const auto rows = buildRows();
        int top = 0;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].kind == RowKind::StripToggle) top += dividerH();
            if (y >= top && y < top + rowH()) return (int)i;
            top += rowH();
        }
        return -1;
    }

    // Toggle cells hug the right edge: the strip column outermost, the highway inside it.
    juce::Rectangle<int> cellFor(const juce::Rectangle<int>& area, int columnFromRight) const
    {
        const int w = juce::roundToInt(px(columnWidthRef));
        return { area.getRight() - w * (columnFromRight + 1), area.getY(), w, area.getHeight() };
    }

    bool rowSplits(const Row& row) const
    {
        return splitColumns() && (int)row.group == expanded
            && (row.kind == RowKind::Layer || row.kind == RowKind::Group
                || row.kind == RowKind::ColumnHeader);
    }

    std::optional<Surface> surfaceAt(int x, const Row& row) const
    {
        const auto area = juce::Rectangle<int>(0, 0, getWidth(), rowH());
        if (rowSplits(row))
        {
            if (cellFor(area, 0).getX() <= x) return Surface::Strip;
            if (cellFor(area, 1).getX() <= x) return Surface::Highway;
            return std::nullopt;
        }
        if (cellFor(area, 0).getX() <= x) return Surface::Highway;
        return std::nullopt;
    }

    // A collapsed group, or one clicked on its name, drives every surface that is showing.
    std::vector<Surface> surfacesFor(const Row& row, std::optional<Surface> picked) const
    {
        if (picked.has_value() && rowSplits(row)) return { *picked };
        if (stripVisible) return { Surface::Highway, Surface::Strip };
        return { Surface::Highway };
    }

    Tri groupState(Group g, const std::vector<Surface>& surfaces) const
    {
        int on = 0, total = 0;
        for (auto l : layersIn(g))
            for (auto s : surfaces)
                if (ViewLayers::applies(l, s)) { ++total; on += view.has(l, s) ? 1 : 0; }
        if (on == 0) return Tri::None;
        return on == total ? Tri::All : Tri::Some;
    }

    void toggleGroup(Group g, const std::vector<Surface>& surfaces)
    {
        const bool turnOn = groupState(g, surfaces) != Tri::All;
        for (auto l : layersIn(g))
            for (auto s : surfaces)
                view.set(l, s, turnOn);
        changed();
    }

    void changed()
    {
        repaint();
        if (onViewChanged) onViewChanged(view);
    }

    void relayout()
    {
        repaint();
        if (onLayoutChanged) onLayoutChanged();
    }

    void setHovered(int row)
    {
        if (row == hoveredRow) return;
        hoveredRow = row;
        repaint();
    }

    void paintRow(juce::Graphics& g, const Row& row, juce::Rectangle<int> area) const
    {
        const int pad = juce::roundToInt(px(padRef));
        auto text = area.withTrimmedLeft(pad);

        switch (row.kind)
        {
            case RowKind::Group:
            {
                paintChevron(g, text.removeFromLeft(juce::roundToInt(px(indentRef))),
                             expandable(row.group), expanded == (int)row.group);
                g.setColour(juce::Colour(Theme::textWhite));
                g.setFont(Theme::getUIFont(px(groupFontRef)));
                g.drawText(ViewLayers::groupLabels[(int)row.group], text,
                           juce::Justification::centredLeft, false);

                if (rowSplits(row))
                {
                    paintToggle(g, cellFor(area, 1), groupState(row.group, { Surface::Highway }));
                    paintToggle(g, cellFor(area, 0), groupState(row.group, { Surface::Strip }));
                }
                else
                    paintToggle(g, cellFor(area, 0), groupState(row.group, surfacesFor(row, std::nullopt)));
                break;
            }
            case RowKind::ColumnHeader:
            {
                g.setColour(juce::Colour(Theme::textDim));
                g.setFont(Theme::getUIFont(px(headerFontRef)));
                g.drawText("Highway", cellFor(area, 1), juce::Justification::centred, false);
                g.drawText("Strip", cellFor(area, 0), juce::Justification::centred, false);
                break;
            }
            case RowKind::Layer:
            {
                text.removeFromLeft(juce::roundToInt(px(indentRef) * 2.0f));
                g.setColour(juce::Colour(Theme::textWhite).withAlpha(layerTextAlpha));
                g.setFont(Theme::getUIFont(px(layerFontRef)));
                g.drawText(ViewLayers::info(row.layer).label, text,
                           juce::Justification::centredLeft, false);

                auto state = [this, &row](Surface s)
                { return view.has(row.layer, s) ? Tri::All : Tri::None; };

                if (rowSplits(row))
                {
                    paintToggle(g, cellFor(area, 1), state(Surface::Highway));
                    if (ViewLayers::applies(row.layer, Surface::Strip))
                        paintToggle(g, cellFor(area, 0), state(Surface::Strip));
                }
                else
                    paintToggle(g, cellFor(area, 0), state(Surface::Highway));
                break;
            }
            case RowKind::StripToggle:
            {
                text.removeFromLeft(juce::roundToInt(px(indentRef)));
                g.setColour(juce::Colour(Theme::textWhite));
                g.setFont(Theme::getUIFont(px(groupFontRef)));
                g.drawText("Utility strip", text, juce::Justification::centredLeft, false);
                paintToggle(g, cellFor(area, 0), stripVisible ? Tri::All : Tri::None);
                break;
            }
            case RowKind::Rules:
            {
                paintChevron(g, text.removeFromLeft(juce::roundToInt(px(indentRef))), true, rulesOpen);
                g.setColour(juce::Colour(Theme::textWhite));
                g.setFont(Theme::getUIFont(px(groupFontRef)));
                g.drawText("Chart rules", text, juce::Justification::centredLeft, false);
                break;
            }
        }
    }

    void paintChevron(juce::Graphics& g, juce::Rectangle<int> box, bool show, bool open) const
    {
        if (!show) return;
        const auto c = box.toFloat().getCentre();
        const float r = px(3.0f);
        juce::Path p;
        if (open) p.addTriangle(c.x - r, c.y - r * 0.5f, c.x + r, c.y - r * 0.5f, c.x, c.y + r * 0.7f);
        else      p.addTriangle(c.x - r * 0.5f, c.y - r, c.x - r * 0.5f, c.y + r, c.x + r * 0.7f, c.y);
        g.setColour(juce::Colour(Theme::textDim));
        g.fillPath(p);
    }

    // Neutral on purpose: accent colours mean write mode, and a layer is not a mode.
    void paintToggle(juce::Graphics& g, juce::Rectangle<int> cell, Tri state) const
    {
        const float size = px(toggleSizeRef);
        const auto box = juce::Rectangle<float>(size, size).withCentre(cell.toFloat().getCentre());
        const float corner = size * 0.25f;

        if (state == Tri::All)
        {
            g.setColour(juce::Colour(Theme::textWhite));
            g.fillRoundedRectangle(box, corner);
            juce::Path tick;
            tick.startNewSubPath(box.getX() + size * 0.24f, box.getCentreY());
            tick.lineTo(box.getX() + size * 0.43f, box.getBottom() - size * 0.27f);
            tick.lineTo(box.getRight() - size * 0.22f, box.getY() + size * 0.28f);
            g.setColour(juce::Colour(Theme::darkBg));
            g.strokePath(tick, juce::PathStrokeType(px(1.8f), juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }
        else if (state == Tri::Some)
        {
            g.setColour(juce::Colour(Theme::textWhite));
            g.drawRoundedRectangle(box.reduced(0.5f), corner, 1.0f);
            g.fillRect(box.reduced(size * 0.27f, size * 0.44f));
        }
        else
        {
            g.setColour(juce::Colour(Theme::textDim).withAlpha(offOutlineAlpha));
            g.drawRoundedRectangle(box.reduced(0.5f), corner, 1.0f);
        }
    }
};
