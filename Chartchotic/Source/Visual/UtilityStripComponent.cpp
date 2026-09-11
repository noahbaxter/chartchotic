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

    // Column for a note, or -1 for anything that is not a playable gem at this skill.
    // The mappers signal that with uint(-1), which is private to InstrumentMapper.
    int noteColumn(uint pitch, Part part, SkillLevel skill)
    {
        uint col;
        if (getRenderType(part) == RenderType::ELITE_DRUMS)
            col = InstrumentMapper::getEliteDrumColumn(pitch, skill, false);
        else if (isDrumLike(part))
            col = InstrumentMapper::getDrumColumn(pitch, skill, false);
        else
            col = InstrumentMapper::getGuitarColumn(pitch, skill);

        return col == (uint)-1 ? -1 : (int)col;
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

void UtilityStripComponent::drawChunkLines(juce::Graphics& g, const View& v) const
{
    double len = barLengthQN(tempoMap, v.startQN);
    double pxPerBar = trackHeight() * len / std::max(1.0e-6, v.endQN - v.startQN);
    int chunk = chunkBars(pxPerBar);

    double firstBar = std::floor(v.startQN / len) * len;
    const float w = (float)getWidth();
    bool showNumbers = filterOn(2);

    g.setFont(juce::Font(juce::FontOptions(10.0f)));

    for (double qn = firstBar; qn <= v.endQN; qn += len)
    {
        len = barLengthQN(tempoMap, qn);
        float y = (float)yForQN(qn, v);
        if (y < (float)trackTop() || y > (float)(trackTop() + trackHeight())) continue;

        int measure = tempoMap ? TempoTimeSignatureEventHelper::pPqToMeasureNumber(PPQ(qn), *tempoMap)
                               : (int)(qn / 4.0) + 1;
        if ((measure - 1) % chunk != 0) continue;

        // Every fourth chunk is heavier, so the eye can count groups without reading numbers.
        bool major = ((measure - 1) / chunk) % 4 == 0;
        g.setColour(major ? UtilityColours::chunkLineBig : UtilityColours::chunkLine);
        g.fillRect(0.0f, y, w, major ? 2.0f : 1.0f);

        if (showNumbers)
        {
            g.setColour(UtilityColours::measureText);
            g.drawText(juce::String(measure), 0, (int)y + 2, (int)w - 5, 11,
                       juce::Justification::centredRight, false);
        }
    }
}

void UtilityStripComponent::drawNotes(juce::Graphics& g, const View& v) const
{
    if (notes == nullptr) return;

    int lanes = isDrumLike(notesPart) ? 5 : 5;
    if (getRenderType(notesPart) == RenderType::ELITE_DRUMS) lanes = 9;

    // Narrow strip on the right so the note map never sits under the section names.
    const float mapW = std::min(64.0f, getWidth() * 0.34f);
    const float x0 = getWidth() - mapW - 4.0f;
    const float laneW = mapW / (float)lanes;
    const float markH = 2.0f;
    const float top = (float)trackTop();
    const float bottom = top + trackHeight();

    g.setColour(UtilityColours::noteMark);

    for (const auto& n : *notes)
    {
        double qn = n.startPPQ.toDouble();
        if (qn < v.startQN || qn > v.endQN) continue;

        int col = noteColumn(n.pitch, notesPart, notesSkill);
        if (col < 0 || col >= lanes) continue;

        float y = (float)yForQN(qn, v);
        if (y < top || y > bottom) continue;

        g.fillRoundedRectangle(x0 + col * laneW + 0.5f, y - markH * 0.5f,
                               std::max(1.5f, laneW - 1.5f), markH, 1.0f);
    }
}

void UtilityStripComponent::drawSections(juce::Graphics& g, const View& v) const
{
    if (sections == nullptr) return;

    const float w = (float)getWidth();
    const auto& list = sections->getSections();
    const float top = (float)trackTop();

    for (size_t i = 0; i < list.size(); ++i)
    {
        double qn = list[i].position.toDouble();
        double nextQN = (i + 1 < list.size()) ? list[i + 1].position.toDouble() : v.endQN;
        if (nextQN < v.startQN || qn > v.endQN) continue;

        float y = (float)yForQN(qn, v);
        float yTop = (float)yForQN(std::min(nextQN, v.endQN), v);

        g.setColour(UtilityColours::sectionBand);
        g.fillRect(0.0f, std::max(yTop, top), w, std::max(1.0f, y - std::max(yTop, top)));

        if (qn < v.startQN || qn > v.endQN) continue;

        g.setColour(UtilityColours::sectionLine);
        g.fillRect(0.0f, y, w, 2.0f);

        g.setColour(UtilityColours::sectionText);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        g.drawText(list[i].name, 6, (int)y - 21, (int)w - 12, 19,
                   juce::Justification::centredLeft, true);
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
            g.setColour(UtilityColours::sectionText);
            g.fillRect(box.reduced(2));
        }

        g.setColour(filters[i].on ? UtilityColours::sectionText : UtilityColours::label);
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
        g.setColour(enabled ? UtilityColours::sectionText : UtilityColours::label);
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

    float py = (float)yForQN(playheadPPQ.toDouble(), v);
    if (py >= (float)trackTop() && py <= (float)(trackTop() + trackHeight()))
    {
        g.setColour(UtilityColours::playhead);
        g.fillRect(0.0f, py, (float)getWidth(), 1.0f);
    }

    drawFilters(g);
    drawFooter(g);

    g.setColour(UtilityColours::border);
    g.drawRect(getLocalBounds(), 1);
}

void UtilityStripComponent::mouseWheelMove(const juce::MouseEvent& e,
                                           const juce::MouseWheelDetails& wheel)
{
    // Cmd owns zoom here; a bare wheel belongs to whatever scrolls the chart.
    if (!e.mods.isCommandDown())
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }

    if (wheel.deltaY > 0.0f)      zoomIn();
    else if (wheel.deltaY < 0.0f) zoomOut();
}

void UtilityStripComponent::mouseDown(const juce::MouseEvent& e)
{
    auto p = e.getPosition();

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
}
