#include "UtilityStripComponent.h"

namespace
{
    double barLengthQN(const TempoTimeSignatureMap* map, double qn)
    {
        if (map == nullptr || map->empty()) return 4.0;
        auto state = TempoTimeSignatureEventHelper::getEffectiveStateAtPPQ(*map, PPQ(qn));
        int num = std::max(1, state.timeSigNumerator);
        int den = std::max(1, state.timeSigDenominator);
        return num * (4.0 / den);
    }

    // Lines are drawn per chunk, not per bar. A bar line every bar is a grey wash at any
    // useful zoom; 4 or 8 bars reads as a ruler. The chunk grows until it has room.
    constexpr float CHUNK_MIN_GAP_PX = 26.0f;

    int chunkBars(double pxPerBar)
    {
        for (int chunk : { 1, 2, 4, 8, 16, 32, 64 })
            if (chunk * pxPerBar >= CHUNK_MIN_GAP_PX)
                return chunk;
        return 128;
    }

    MouseButton buttonOf(const juce::MouseEvent& e)
    {
        if (e.mods.isRightButtonDown())  return MouseButton::Right;
        if (e.mods.isMiddleButtonDown()) return MouseButton::Middle;
        return MouseButton::Left;
    }

    ModifierFlags modsOf(const juce::MouseEvent& e)
    {
        auto f = ModifierFlags::None;
        if (e.mods.isShiftDown()) f = f | ModifierFlags::Shift;
        if (e.mods.isCtrlDown())  f = f | ModifierFlags::Ctrl;
        if (e.mods.isAltDown())   f = f | ModifierFlags::Alt;
        return f;
    }

    // Column for a note, or -1 for anything that is not a playable gem at this skill.
    // The mappers signal that with uint(-1), which is private to InstrumentMapper.
    // 2x kicks are always resolved so the map has a column for them.
    int noteColumn(uint pitch, Part part, SkillLevel skill)
    {
        uint col;
        if (getRenderType(part) == RenderType::ELITE_DRUMS)
            col = InstrumentMapper::getEliteDrumColumn(pitch, skill, true);
        else if (isDrumLike(part))
            col = InstrumentMapper::getDrumColumn(pitch, skill, true);
        else
            col = InstrumentMapper::getGuitarColumn(pitch, skill);

        return col == (uint)-1 ? -1 : (int)col;
    }

    // Columns the part actually has, left to right. Gem column indices are neither
    // contiguous nor in reading order (4-lane skips 5 and puts the 2x kick at 6), so the
    // map needs an explicit order rather than a count. Kicks lead, loudest on the left.
    const std::vector<int>& displayColumns(Part part)
    {
        static const std::vector<int> guitar { 0, 1, 2, 3, 4, 5 };                  // open GRYBO
        static const std::vector<int> drums  { DRUM_KICK_2X_COLUMN, 0, 1, 2, 3, 4 }; // 2x 1x R Y B G
        static const std::vector<int> elite  { ELITE_KICK_2X_COLUMN, 0, 1, 2, 3,
                                               4, 5, 6, 7, 8 };

        if (getRenderType(part) == RenderType::ELITE_DRUMS) return elite;
        if (isDrumLike(part)) return drums;
        return guitar;
    }
}

UtilityStripComponent::View UtilityStripComponent::visibleRange() const
{
    double now = playheadPPQ.toDouble();
    int bars = barsVisible();

    if (bars == 0)
    {
        double end = sections ? sections->getEndPPQ().toDouble() : 0.0;
        if (end <= 0.0) end = std::max(now, 1.0) + 64 * 4.0;
        return { 0.0, end };
    }

    double span = bars * barLengthQN(tempoMap, now);
    double start = now - span * PLAYHEAD_FROM_BOTTOM;
    return { start, start + span };
}

// Numbers only appear where they mean something: section starts, and wherever the cursor
// is. Numbering every chunk was the obnoxious version.
void UtilityStripComponent::drawMeasureNumber(juce::Graphics& g, float y, int measure,
                                              bool emphasised) const
{
    if (!filterOn(2)) return;

    g.setFont(juce::Font(juce::FontOptions(10.0f, emphasised ? juce::Font::bold
                                                             : juce::Font::plain)));
    // Above its line, like every other label here: time runs upward, so a number sitting
    // below its line reads as belonging to the bar before it.
    auto text = juce::String(measure);
    int tw = g.getCurrentFont().getStringWidth(text) + 8;
    int tx = getWidth() - tw - 3;
    int ty = (int)y - 14;

    g.setColour(UtilityColours::background.withAlpha(0.8f));
    g.fillRect(tx, ty, tw, 12);
    g.setColour(emphasised ? UtilityColours::uiText : UtilityColours::measureText);
    g.drawText(text, tx, ty, tw, 12, juce::Justification::centred, false);
}

int UtilityStripComponent::measureAtQN(double qn) const
{
    return tempoMap ? TempoTimeSignatureEventHelper::pPqToMeasureNumber(PPQ(qn), *tempoMap)
                    : (int)(qn / 4.0) + 1;
}

int UtilityStripComponent::sectionStartMeasure(double qn) const
{
    auto measureAt = [this](double q)
    {
        return tempoMap ? TempoTimeSignatureEventHelper::pPqToMeasureNumber(PPQ(q), *tempoMap)
                        : (int)(q / 4.0) + 1;
    };

    if (sections == nullptr) return 1;
    const auto* s = sections->sectionAt(PPQ(qn));
    return s != nullptr ? measureAt(s->position.toDouble()) : 1;
}

// Bar lines are walked rather than divided. Bar length is not constant (Embedded has a 6/4
// bar at QN 136), so qn/barLength drifts after any time signature change and lands off the
// downbeat. Everything that snaps on the strip goes through here, so it always hits beat 1.
double UtilityStripComponent::barLineAtOrBefore(double qn) const
{
    double cur = 0.0;
    for (int guard = 0; guard < MAX_BARS_WALKED; ++guard)
    {
        double len = std::max(0.25, barLengthQN(tempoMap, cur));
        if (cur + len > qn) return cur;
        cur += len;
    }
    return cur;
}

// Walked by index rather than by subtracting epsilons: stepping back used to ask for the
// bar before `cur - epsilon`, which returns `cur` again whenever cur is already on a line,
// so backwards never moved.
double UtilityStripComponent::barLineStep(double fromQN, int bars) const
{
    std::vector<double> lines;
    double cur = 0.0;
    int here = 0;

    for (int guard = 0; guard < MAX_BARS_WALKED; ++guard)
    {
        lines.push_back(cur);
        if (cur <= fromQN + kBarEpsilon) here = (int)lines.size() - 1;
        // One bar past the target is enough to step forward from.
        if (cur > fromQN + kBarEpsilon && (int)lines.size() > here + std::abs(bars) + 1) break;
        cur += std::max(0.25, barLengthQN(tempoMap, cur));
    }

    int target = juce::jlimit(0, (int)lines.size() - 1, here + bars);
    return lines[(size_t)target];
}

// The line under `fromQN` on the grid that is actually drawn, stepped by `direction`.
// Chunk lines are section-relative, so stepping a fixed number of bars from an arbitrary
// position lands between them rather than on them.
double UtilityStripComponent::chunkLineStep(double fromQN, int direction, int chunk) const
{
    auto onChunk = [this, chunk](double barQN)
    {
        return ((measureAtQN(barQN) - sectionStartMeasure(barQN)) % chunk) == 0;
    };

    double cur = barLineAtOrBefore(fromQN);
    bool onGrid = onChunk(cur) && std::abs(cur - fromQN) <= kBarEpsilon;

    if (!onGrid)
    {
        // Off the drawn grid: find the line behind us. Scrolling back lands on it;
        // scrolling forward continues from it to the next one.
        for (int i = 0; i < MAX_CHUNK_SEARCH && !onChunk(cur); ++i)
        {
            double prev = barLineStep(cur, -1);
            if (prev >= cur) break;
            cur = prev;
        }
        if (direction < 0) return cur;
    }

    for (int i = 0; i < MAX_CHUNK_SEARCH; ++i)
    {
        double next = barLineStep(cur, direction);
        if (std::abs(next - cur) <= kBarEpsilon) return cur;   // hit either end
        cur = next;
        if (onChunk(cur)) return cur;
    }
    return std::max(0.0, cur);
}

// The bar you are in, not the nearest one. Rounding to nearest puts the indicator above
// the cursor for the top half of every bar, which reads as pointing at the wrong thing and
// is worst at the bottom edge where it collides with the pinned label.
double UtilityStripComponent::snappedBarQN(int y, const View& v) const
{
    return barLineAtOrBefore(std::max(0.0, qnForY(y, v)));
}

void UtilityStripComponent::drawHover(juce::Graphics& g, const View& v) const
{
    if (hoverY < 0) return;

    double barQN = snappedBarQN(hoverY, v);
    float y = (float)yForQN(barQN, v);
    if (y < (float)trackTop() || y > (float)(trackTop() + trackHeight())) return;

    const float w = (float)getWidth();
    g.setColour(UtilityColours::hoverLine);
    g.fillRect(0.0f, y, w, 1.5f);

    drawMeasureNumber(g, y, measureAtQN(barQN), true);
}

void UtilityStripComponent::drawChunkLines(juce::Graphics& g, const View& v) const
{
    double len = barLengthQN(tempoMap, v.startQN);
    double pxPerBar = trackHeight() * len / std::max(1.0e-6, v.endQN - v.startQN);
    int chunk = chunkBars(pxPerBar);

    const float w = (float)getWidth();

    for (double qn = barLineAtOrBefore(std::max(0.0, v.startQN)); qn <= v.endQN;
         qn += std::max(0.25, barLengthQN(tempoMap, qn)))
    {
        float y = (float)yForQN(qn, v);
        if (y < (float)trackTop() || y > (float)(trackTop() + trackHeight())) continue;

        int measure = tempoMap ? TempoTimeSignatureEventHelper::pPqToMeasureNumber(PPQ(qn), *tempoMap)
                               : (int)(qn / 4.0) + 1;

        // Counted from the section that contains this bar, so the 4/8/16 groupings land on
        // musical boundaries instead of wherever bar 1 happened to put them.
        int barsIn = measure - sectionStartMeasure(qn);
        if (barsIn % chunk != 0) continue;

        // Every fourth chunk is heavier, so the eye can count groups without reading numbers.
        bool major = (barsIn / chunk) % 4 == 0;
        g.setColour(major ? UtilityColours::chunkLineBig : UtilityColours::chunkLine);
        g.fillRect(0.0f, y, w, major ? 2.0f : 1.0f);
    }
}

void UtilityStripComponent::drawNotes(juce::Graphics& g, const View& v) const
{
    if (noteSources.empty()) return;

    const float top = (float)trackTop();
    const float bottom = top + trackHeight();
    const float markH = 3.0f;
    const float inset = 3.0f;
    const float sourceW = (getWidth() - inset * 2.0f) / (float)noteSources.size();

    for (size_t s = 0; s < noteSources.size(); ++s)
    {
        const auto& src = noteSources[s];
        if (src.notes == nullptr) continue;

        const auto& columns = displayColumns(src.part);
        // Guitar-likes share the 5-fret column layout, the same normalisation the renderers use.
        Part colourPart = isGuitarLike(src.part) ? Part::GUITAR : src.part;

        const float x0 = inset + s * sourceW;
        const float laneW = sourceW / (float)columns.size();

        if (s > 0)
        {
            g.setColour(UtilityColours::chunkLine);
            g.fillRect(x0, top, 1.0f, bottom - top);
        }

        for (const auto& n : *src.notes)
        {
            double qn = n.startPPQ.toDouble();
            if (qn < v.startQN || qn > v.endQN) continue;

            int col = noteColumn(n.pitch, src.part, src.skill);
            if (col < 0) continue;

            auto slot = std::find(columns.begin(), columns.end(), col);
            if (slot == columns.end()) continue;
            int lane = (int)std::distance(columns.begin(), slot);

            float y = (float)yForQN(qn, v);
            if (y < top || y > bottom) continue;

            // Muted hard: the map is a glance reference for "oh right, that part", not a
            // surface you reach for. Full lane colour reads as interactive.
            g.setColour(assetManager != nullptr
                        ? assetManager->getLaneColour((uint)col, colourPart, false)
                              .withMultipliedSaturation(NOTE_MAP_SATURATION)
                              .withAlpha(NOTE_MAP_ALPHA)
                        : UtilityColours::noteMark);
            g.fillRoundedRectangle(x0 + lane * laneW + 0.5f, y - markH * 0.5f,
                                   std::max(1.5f, laneW - 1.5f), markH, 1.2f);
        }
    }
}

juce::Rectangle<int> UtilityStripComponent::sectionHeaderBounds(size_t index, const View& v) const
{
    if (sections == nullptr || index >= sections->getSections().size()) return {};
    const auto& list = sections->getSections();
    const auto& s = list[index];
    float y = (float)yForQN(s.position.toDouble(), v);

    juce::Font f(juce::FontOptions(16.0f, juce::Font::bold));
    int tw = std::min(getWidth() - 12, f.getStringWidth(s.name) + 10);
    int top = (int)y - LABEL_H - 2;

    // The section you are currently inside pins to the bottom once its own line has
    // scrolled past, so its name is always readable and still clickable. The next section
    // pushes it off from above rather than overlapping it, which is what lets it leave
    // exactly when you cross into that section.
    if (index == (size_t)stickyIndex(v))
    {
        float bottom = (float)(trackTop() + trackHeight());
        float pinned = bottom - LABEL_H - 2;

        if (index + 1 < list.size())
        {
            float nextY = (float)yForQN(list[index + 1].position.toDouble(), v);
            pinned = std::max(pinned, nextY - 2.0f);
        }
        top = std::min(top, (int)pinned);
    }

    return { 3, top, tw + 6, LABEL_H };
}

// Section containing the bottom of the view, ie the one the strip is currently inside.
int UtilityStripComponent::stickyIndex(const View& v) const
{
    if (sections == nullptr) return -1;
    const auto& list = sections->getSections();
    int found = -1;
    for (int i = 0; i < (int)list.size(); ++i)
    {
        if (list[(size_t)i].position.toDouble() > v.startQN) break;
        found = i;
    }
    return found;
}

juce::Rectangle<int> UtilityStripComponent::sectionRowBounds(size_t index, const View& v) const
{
    auto plate = sectionHeaderBounds(index, v);
    if (plate.isEmpty()) return {};
    // Full width: the whole band at a section's line is that section, so you do not have to
    // hit a short name to act on it.
    return { 0, plate.getY(), getWidth(), plate.getHeight() + 3 };
}

int UtilityStripComponent::sectionHeaderAt(juce::Point<int> p) const
{
    if (sections == nullptr) return -1;
    auto v = visibleRange();
    const auto& list = sections->getSections();

    // No range check: the pinned section's line sits outside the view while its label is
    // still on screen, and skipping it would make that label unclickable.
    for (size_t i = 0; i < list.size(); ++i)
        if (sectionRowBounds(i, v).contains(p)) return (int)i;
    return -1;
}

int UtilityStripComponent::sectionNameAt(juce::Point<int> p) const
{
    if (sections == nullptr) return -1;
    auto v = visibleRange();
    const auto& list = sections->getSections();

    for (size_t i = 0; i < list.size(); ++i)
        if (sectionHeaderBounds(i, v).contains(p)) return (int)i;
    return -1;
}

void UtilityStripComponent::drawSections(juce::Graphics& g, const View& v) const
{
    if (sections == nullptr) return;

    const float w = (float)getWidth();
    const auto& list = sections->getSections();
    const float top = (float)trackTop();

    // Follows the hover cursor's snapped bar, the same bar the hover line draws on, and
    // falls back to the playhead when the pointer is away. So a section is always lit:
    // where you are pointing, or where you are.
    double referenceQN = hoverY >= 0 ? snappedBarQN(hoverY, v) : playheadPPQ.toDouble();
    const SongSections::Section* hovered = sections->sectionAt(PPQ(referenceQN));

    for (size_t i = 0; i < list.size(); ++i)
    {
        double qn = list[i].position.toDouble();
        double nextQN = (i + 1 < list.size()) ? list[i + 1].position.toDouble() : v.endQN;
        if (nextQN < v.startQN || qn > v.endQN) continue;

        float y = (float)yForQN(qn, v);
        float yTop = (float)yForQN(std::min(nextQN, v.endQN), v);

        // A section owns everything from its own line up to the next one. Clamped at both
        // ends, so a section running off either edge fills what is visible of it rather
        // than bleeding past the track into the footer.
        const float bottom = top + (float)trackHeight();
        float bandTop = juce::jlimit(top, bottom, yTop);
        float bandBottom = juce::jlimit(top, bottom, y);

        bool isHovered = (hovered != nullptr && &list[i] == hovered);
        if (bandBottom > bandTop)
        {
            g.setColour(isHovered ? UtilityColours::sectionBandHover
                                  : UtilityColours::sectionBand);
            g.fillRect(0.0f, bandTop, w, bandBottom - bandTop);
        }

        bool sticky = ((int)i == stickyIndex(v));
        if ((qn < v.startQN || qn > v.endQN) && !sticky) continue;

        if (qn >= v.startQN && qn <= v.endQN)
            drawMeasureNumber(g, y, measureAtQN(qn), false);

        bool selected = isSelected((int)i);

        // A move drag previews by shifting the selected markers rather than committing.
        if (selected && dragActive && dragCommand == StripCommand::UpdateMove)
            y = (float)yForQN(qn + dragMoveDeltaQN, v);

        if (y >= top && y <= top + trackHeight())
        {
            g.setColour(selected ? UtilityColours::selectedLine : UtilityColours::sectionLine);
            g.fillRect(0.0f, y, w, selected ? 3.0f : 2.0f);
        }

        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        // Straight from the hit-test bounds, so a pinned label is clickable where it is
        // drawn rather than where its line would have put it.
        auto plate = sectionHeaderBounds(i, v);
        if (plate.getY() > top + trackHeight()) continue;

        // The name lighting up IS the rename affordance: hovering it fills behind the text,
        // so the thing you click is the thing that changes.
        bool nameHot = ((int)i == hoverName && renameIndex < 0);

        g.setColour(nameHot ? UtilityColours::namePlateHover
                            : UtilityColours::background.withAlpha(0.82f));
        g.fillRect(plate);
        if (selected)
        {
            g.setColour(UtilityColours::selectedLine);
            g.drawRect(plate, 1);
        }
        g.setColour(nameHot ? UtilityColours::uiText : UtilityColours::sectionText);
        g.drawText(list[i].name, plate.getX() + 3, plate.getY(), plate.getWidth() - 6,
                   plate.getHeight(), juce::Justification::centredLeft, true);
    }

    if (sections->hasCoda())
    {
        double qn = sections->getCodaPPQ().toDouble();
        if (qn >= v.startQN && qn <= v.endQN)
        {
            float y = (float)yForQN(qn, v);
            g.setColour(UtilityColours::codaLine);
            g.fillRect(0.0f, y, w, 2.0f);
            g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
            g.drawText("coda", 6, (int)y - 21, (int)w - 12, 19,
                       juce::Justification::centredLeft, true);
        }
    }
}

void UtilityStripComponent::drawFilters(juce::Graphics& g) const
{
    auto header = filterHeaderBounds();
    g.setColour(UtilityColours::panelFill);
    g.fillRect(header);
    g.setColour(UtilityColours::border);
    g.fillRect(0, header.getBottom() - 1, getWidth(), 1);

    g.setColour(UtilityColours::label);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(juce::String(filtersExpanded ? "v" : ">") + "  filters",
               header.reduced(6, 0), juce::Justification::centredLeft, false);

    if (!filtersExpanded) return;

    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    for (int i = 0; i < (int)std::size(filters); ++i)
    {
        auto row = filterRowBounds(i);
        g.setColour(UtilityColours::panelFill);
        g.fillRect(row);

        auto box = juce::Rectangle<int>(row.getX() + 8, row.getY() + 4, 10, 10);
        g.setColour(UtilityColours::chunkLineBig);
        g.drawRect(box, 1);
        if (filters[i].on)
        {
            g.setColour(UtilityColours::uiText);
            g.fillRect(box.reduced(2));
        }

        g.setColour(filters[i].on ? UtilityColours::uiText : UtilityColours::label);
        g.drawText(filters[i].label, box.getRight() + 7, row.getY(),
                   row.getWidth() - box.getRight() - 10, row.getHeight(),
                   juce::Justification::centredLeft, false);
    }

    g.setColour(UtilityColours::border);
    g.fillRect(0, filterAreaHeight() - 1, getWidth(), 1);
}

void UtilityStripComponent::drawFooter(juce::Graphics& g) const
{
    auto minus = minusBounds();
    auto plus = plusBounds();

    g.setColour(UtilityColours::panelFill);
    g.fillRect(0, getHeight() - FOOTER_H, getWidth(), FOOTER_H);

    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    for (auto [r, text, enabled] : { std::tuple{ minus, "-", zoomIndex < (int)std::size(ZOOM_STEPS) - 1 },
                                     std::tuple{ plus,  "+", zoomIndex > 0 } })
    {
        g.setColour(UtilityColours::chunkLineBig);
        g.drawRect(r, 1);
        g.setColour(enabled ? UtilityColours::uiText : UtilityColours::label);
        g.drawText(text, r, juce::Justification::centred, false);
    }

    g.setColour(UtilityColours::label);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText(barsVisible() == 0 ? "whole song" : juce::String(barsVisible()) + " bars",
               plus.getRight() + 6, plus.getY(), getWidth() - plus.getRight() - 10,
               plus.getHeight(), juce::Justification::centredLeft, false);
}

void UtilityStripComponent::paint(juce::Graphics& g)
{
    g.fillAll(UtilityColours::background);

    auto v = visibleRange();
    drawChunkLines(g, v);
    if (filterOn(1)) drawNotes(g, v);
    if (filterOn(0)) drawSections(g, v);

    drawMarquee(g);
    drawHover(g, v);

    // The current moment has to win against everything else drawn here, so it gets a glow,
    // a solid bar and a nub on each edge rather than a hairline.
    float py = (float)yForQN(playheadPPQ.toDouble(), v);
    if (py >= (float)trackTop() && py <= (float)(trackTop() + trackHeight()))
    {
        const float w = (float)getWidth();
        g.setColour(UtilityColours::playheadGlow);
        g.fillRect(0.0f, py - 4.0f, w, 9.0f);
        g.setColour(UtilityColours::playhead);
        g.fillRect(0.0f, py - 1.0f, w, 2.0f);

        juce::Path nub;
        nub.addTriangle(0.0f, py - 6.0f, 0.0f, py + 6.0f, 7.0f, py);
        g.fillPath(nub);

        // The current bar hangs off the right edge, always visible, so "where am I" never
        // needs a hover. Sits above the line, the direction time runs.
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        auto text = juce::String(measureAtQN(playheadPPQ.toDouble()));
        int tw = g.getCurrentFont().getStringWidth(text) + 12;
        int th = 16;
        auto tab = juce::Rectangle<int>(getWidth() - tw, (int)py - th - 3, tw, th);

        juce::Path tabShape;
        tabShape.addRoundedRectangle((float)tab.getX(), (float)tab.getY(),
                                     (float)tab.getWidth(), (float)tab.getHeight(),
                                     3.0f, 3.0f, true, false, true, false);
        g.setColour(UtilityColours::playhead);
        g.fillPath(tabShape);
        g.setColour(UtilityColours::background);
        g.drawText(text, tab, juce::Justification::centred, false);
    }

    drawFilters(g);
    drawFooter(g);

    g.setColour(UtilityColours::border);
    g.drawRect(getLocalBounds(), 1);
}

void UtilityStripComponent::mouseWheelMove(const juce::MouseEvent& e,
                                           const juce::MouseWheelDetails& wheel)
{
    double delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (delta == 0.0) return;

    if (e.mods.isCommandDown())
    {
        if (delta > 0.0) zoomIn(); else zoomOut();
        return;
    }

    // A bare wheel steps between the chunk lines that are actually drawn, so the map moves
    // in the same units it is ruled in. Fine positioning is the highway's job.
    if (onSeekTo)
    {
        auto v = visibleRange();
        double len = barLengthQN(tempoMap, playheadPPQ.toDouble());
        double pxPerBar = trackHeight() * len / std::max(1.0e-6, v.endQN - v.startQN);
        int chunk = chunkBars(pxPerBar);
        int direction = delta > 0.0 ? 1 : -1;

        // A notch should travel about the same distance on screen at every zoom, so the
        // number of chunk steps is chosen from how tall a chunk currently is. Zoomed in
        // that is a couple of bars, zoomed out it is many, but the sleeve moves the same.
        double chunkPx = std::max(1.0, chunk * pxPerBar);
        int steps = std::max(1, (int)std::lround(trackHeight() * SCROLL_TRAVEL_FRACTION / chunkPx));
        if (e.mods.isShiftDown()) steps *= 4;

        // Steps onto the lines that are actually drawn. Those are counted from each
        // section, so stepping a fixed number of bars instead lands between them.
        double target = playheadPPQ.toDouble();
        for (int i = 0; i < steps; ++i)
            target = chunkLineStep(target, direction, chunk);

        onSeekTo(target);
    }
}

std::vector<UtilityStripComponent::SectionEdit> UtilityStripComponent::currentEdits() const
{
    std::vector<SectionEdit> out;
    if (sections == nullptr) return out;
    for (const auto& s : sections->getSections())
        out.push_back({ s.position.toDouble(), s.name });
    return out;
}

void UtilityStripComponent::commitSections(std::vector<SectionEdit> edits)
{
    std::sort(edits.begin(), edits.end(),
              [](const SectionEdit& a, const SectionEdit& b) { return a.qn < b.qn; });
    if (onSectionsEdited) onSectionsEdited(edits);
    selection.clear();
    repaint();
}

void UtilityStripComponent::beginRename(int index)
{
    if (sections == nullptr || index < 0
        || index >= (int)sections->getSections().size()) return;

    renameIndex = index;
    auto v = visibleRange();
    auto plate = sectionHeaderBounds((size_t)index, v);

    renameEditor = std::make_unique<juce::TextEditor>();
    renameEditor->setText(sections->getSections()[(size_t)index].name, false);
    renameEditor->selectAll();
    renameEditor->setBounds(plate.withWidth(std::max(90, getWidth() - plate.getX() - 6)));
    renameEditor->onReturnKey = [this] { commitRename(); };
    renameEditor->onEscapeKey = [this] { renameEditor.reset(); renameIndex = -1; };
    renameEditor->onFocusLost = [this] { commitRename(); };

    addAndMakeVisible(*renameEditor);
    renameEditor->grabKeyboardFocus();
}

void UtilityStripComponent::commitRename()
{
    if (renameEditor == nullptr || renameIndex < 0) return;

    auto name = renameEditor->getText().trim();
    int index = renameIndex;
    renameEditor.reset();
    renameIndex = -1;

    // An empty name would write "[section ]", which reads back as no section at all.
    if (name.isEmpty()) { repaint(); return; }

    auto edits = currentEdits();
    if (index < (int)edits.size() && edits[(size_t)index].name != name)
    {
        edits[(size_t)index].name = name;
        commitSections(std::move(edits));
    }
    else repaint();
}

void UtilityStripComponent::dispatch(StripCommand cmd, const juce::MouseEvent& e)
{
    auto v = visibleRange();
    auto p = e.getPosition();
    int object = sectionHeaderAt(p);

    switch (cmd)
    {
        case StripCommand::SeekToBar:
            if (onSeekTo) onSeekTo(snappedBarQN(p.y, v));
            break;

        case StripCommand::JumpToSection:
            if (onSeekTo && object >= 0)
                onSeekTo(sections->getSections()[(size_t)object].position.toDouble());
            break;

        case StripCommand::SelectAt:
            selection.clear();
            if (object >= 0) selection.push_back(object);
            dragActive = true;
            dragAnchorY = p.y;
            dragCommand = StripCommand::UpdateMove;
            repaint();
            break;

        case StripCommand::AddToSelection:
            if (object >= 0 && !isSelected(object)) selection.push_back(object);
            repaint();
            break;

        case StripCommand::BeginMarquee:
            selection.clear();
            dragActive = true;
            dragAnchorY = dragCurrentY = p.y;
            dragCommand = StripCommand::UpdateMarquee;
            repaint();
            break;

        case StripCommand::UpdateMarquee:
        {
            dragCurrentY = p.y;
            double a = qnForY(std::min(dragAnchorY, dragCurrentY), v);
            double b = qnForY(std::max(dragAnchorY, dragCurrentY), v);
            selection.clear();
            if (sections != nullptr)
            {
                const auto& list = sections->getSections();
                for (int i = 0; i < (int)list.size(); ++i)
                {
                    double qn = list[(size_t)i].position.toDouble();
                    if (qn >= std::min(a, b) && qn <= std::max(a, b)) selection.push_back(i);
                }
            }
            repaint();
            break;
        }

        case StripCommand::UpdateMove:
        {
            dragCurrentY = p.y;
            dragMoveDeltaQN = snappedBarQN(p.y, v) - snappedBarQN(dragAnchorY, v);
            repaint();
            break;
        }

        case StripCommand::CommitMove:
        {
            if (std::abs(dragMoveDeltaQN) > 1.0e-9 && !selection.empty())
            {
                auto edits = currentEdits();
                for (int i : selection)
                    if (i >= 0 && i < (int)edits.size())
                        edits[(size_t)i].qn = std::max(0.0, edits[(size_t)i].qn + dragMoveDeltaQN);
                commitSections(std::move(edits));
            }
            dragMoveDeltaQN = 0.0;
            break;
        }

        case StripCommand::ToggleAt:
        {
            auto edits = currentEdits();
            if (object >= 0)
            {
                edits.erase(edits.begin() + object);
                commitSections(std::move(edits));
            }
            else
            {
                double qn = snappedBarQN(p.y, v);
                // Nothing to create if a section already starts on this bar.
                for (const auto& ed : edits)
                    if (std::abs(ed.qn - qn) < 1.0e-6) return;

                edits.push_back({ qn, "section" });
                std::sort(edits.begin(), edits.end(),
                          [](const SectionEdit& a, const SectionEdit& b) { return a.qn < b.qn; });
                if (onSectionsEdited) onSectionsEdited(edits);

                // Rename opens straight away: a chart full of "section" is the failure mode
                // the first attempt at this shipped with.
                for (int i = 0; i < (int)edits.size(); ++i)
                    if (std::abs(edits[(size_t)i].qn - qn) < 1.0e-6) { beginRename(i); break; }
            }
            break;
        }

        case StripCommand::DeleteAt:
            if (object >= 0)
            {
                auto edits = currentEdits();
                edits.erase(edits.begin() + object);
                commitSections(std::move(edits));
            }
            break;

        case StripCommand::BeginDeleteSpan:
            dragActive = true;
            dragAnchorY = dragCurrentY = p.y;
            dragCommand = StripCommand::UpdateDeleteSpan;
            repaint();
            break;

        case StripCommand::UpdateDeleteSpan:
            dragCurrentY = p.y;
            repaint();
            break;

        case StripCommand::CommitDeleteSpan:
        {
            double a = qnForY(std::min(dragAnchorY, dragCurrentY), v);
            double b = qnForY(std::max(dragAnchorY, dragCurrentY), v);
            auto edits = currentEdits();
            auto removed = std::remove_if(edits.begin(), edits.end(),
                [&](const SectionEdit& ed)
                { return ed.qn >= std::min(a, b) && ed.qn <= std::max(a, b); });
            if (removed != edits.end())
            {
                edits.erase(removed, edits.end());
                commitSections(std::move(edits));
            }
            break;
        }

        case StripCommand::BeginTimeSelection:
            dragActive = true;
            dragAnchorY = dragCurrentY = p.y;
            dragCommand = StripCommand::UpdateTimeSelection;
            repaint();
            break;

        case StripCommand::UpdateTimeSelection:
            dragCurrentY = p.y;
            repaint();
            break;

        case StripCommand::CommitTimeSelection:
        {
            double a = snappedBarQN(dragAnchorY, v);
            double b = snappedBarQN(dragCurrentY, v);
            if (onTimeSelection && std::abs(a - b) > 1.0e-9)
                onTimeSelection(std::min(a, b), std::max(a, b));
            break;
        }

        case StripCommand::Rename:
            if (object >= 0) beginRename(object);
            break;

        default: break;
    }
}

void UtilityStripComponent::mouseDown(const juce::MouseEvent& e)
{
    auto p = e.getPosition();

    if (renameEditor != nullptr) commitRename();

    if (filterHeaderBounds().contains(p))
    {
        filtersExpanded = !filtersExpanded;
        repaint();
        return;
    }

    if (filtersExpanded)
    {
        for (int i = 0; i < (int)std::size(filters); ++i)
            if (filterRowBounds(i).contains(p))
            {
                filters[i].on = !filters[i].on;
                repaint();
                return;
            }
    }

    if (minusBounds().contains(p)) { zoomOut(); return; }
    if (plusBounds().contains(p))  { zoomIn();  return; }
    if (!inTrackArea(p)) return;

    // Clicking the name renames it. The rest of the section's row belongs to the mapping,
    // which is what makes the name a distinct target rather than a mode.
    int name = sectionNameAt(p);
    if (name >= 0 && !e.mods.isRightButtonDown())
    {
        beginRename(name);
        return;
    }

    dispatch(mapper.resolve(stripMode(), EventType::Down, buttonOf(e),
                            modsOf(e), sectionHeaderAt(p) >= 0), e);
}

void UtilityStripComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragActive) return;
    dispatch(dragCommand, e);
}

void UtilityStripComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!dragActive) return;
    dragActive = false;

    StripCommand commit = StripCommand::None;
    switch (dragCommand)
    {
        case StripCommand::UpdateMove:          commit = StripCommand::CommitMove; break;
        case StripCommand::UpdateMarquee:       commit = StripCommand::CommitMarquee; break;
        case StripCommand::UpdateDeleteSpan:    commit = StripCommand::CommitDeleteSpan; break;
        case StripCommand::UpdateTimeSelection: commit = StripCommand::CommitTimeSelection; break;
        default: break;
    }

    dragCommand = StripCommand::None;
    if (commit != StripCommand::None) dispatch(commit, e);
    repaint();
}

void UtilityStripComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!inTrackArea(e.getPosition())) return;
    dispatch(mapper.resolve(stripMode(), EventType::DoubleClick, buttonOf(e),
                            modsOf(e), sectionHeaderAt(e.getPosition()) >= 0), e);
}

void UtilityStripComponent::mouseMove(const juce::MouseEvent& e)
{
    int y = inTrackArea(e.getPosition()) ? e.getPosition().y : -1;
    int section = y >= 0 ? sectionHeaderAt(e.getPosition()) : -1;
    int name = y >= 0 ? sectionNameAt(e.getPosition()) : -1;

    if (y == hoverY && section == hoverSection && name == hoverName) return;
    hoverY = y;
    hoverSection = section;
    hoverName = name;
    setMouseCursor(name >= 0 ? juce::MouseCursor::IBeamCursor
                             : juce::MouseCursor::NormalCursor);
    repaint();
}

void UtilityStripComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverY < 0 && hoverSection < 0 && hoverName < 0) return;
    hoverY = -1;
    hoverSection = -1;
    hoverName = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

// Marquee, delete span and time selection are all a band between two drag points; only the
// colour says which. Drawn over everything so it reads as a transient overlay.
void UtilityStripComponent::drawMarquee(juce::Graphics& g) const
{
    if (!dragActive) return;

    juce::Colour fill;
    switch (dragCommand)
    {
        case StripCommand::UpdateMarquee:       fill = UtilityColours::marqueeFill; break;
        case StripCommand::UpdateDeleteSpan:    fill = UtilityColours::deleteFill;  break;
        case StripCommand::UpdateTimeSelection: fill = UtilityColours::loopFill;    break;
        default: return;
    }

    float y0 = (float)std::min(dragAnchorY, dragCurrentY);
    float y1 = (float)std::max(dragAnchorY, dragCurrentY);
    g.setColour(fill);
    g.fillRect(0.0f, y0, (float)getWidth(), y1 - y0);
}
