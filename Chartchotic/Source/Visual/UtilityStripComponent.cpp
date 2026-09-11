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

double UtilityStripComponent::snappedBarQN(int y, const View& v) const
{
    double qn = qnForY(y, v);
    double len = barLengthQN(tempoMap, qn);
    return std::max(0.0, std::round(qn / len) * len);
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

    int measure = tempoMap ? TempoTimeSignatureEventHelper::pPqToMeasureNumber(PPQ(barQN), *tempoMap)
                           : (int)(barQN / 4.0) + 1;

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    auto text = juce::String(measure);
    int tw = g.getCurrentFont().getStringWidth(text) + 10;
    int tx = (int)w - tw - 4;
    int ty = (int)y - 16;

    g.setColour(UtilityColours::background.withAlpha(0.9f));
    g.fillRect(tx, ty, tw, 15);
    g.setColour(UtilityColours::hoverLine);
    g.drawRect(tx, ty, tw, 15, 1);
    g.setColour(UtilityColours::uiText);
    g.drawText(text, tx, ty, tw, 15, juce::Justification::centred, false);
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

        // Counted from the section that contains this bar, so the 4/8/16 groupings land on
        // musical boundaries instead of wherever bar 1 happened to put them.
        int barsIn = measure - sectionStartMeasure(qn);
        if (barsIn % chunk != 0) continue;

        // Every fourth chunk is heavier, so the eye can count groups without reading numbers.
        bool major = (barsIn / chunk) % 4 == 0;
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

        int lanes = getRenderType(src.part) == RenderType::ELITE_DRUMS ? 9 : 5;
        // Guitar-likes share the 5-fret column layout, the same normalisation the renderers use.
        Part colourPart = isGuitarLike(src.part) ? Part::GUITAR : src.part;

        const float x0 = inset + s * sourceW;
        const float laneW = sourceW / (float)lanes;

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
            if (col < 0 || col >= lanes) continue;

            float y = (float)yForQN(qn, v);
            if (y < top || y > bottom) continue;

            g.setColour(assetManager != nullptr
                        ? assetManager->getLaneColour((uint)col, colourPart, false)
                        : UtilityColours::noteMark);
            g.fillRoundedRectangle(x0 + col * laneW + 0.5f, y - markH * 0.5f,
                                   std::max(1.5f, laneW - 1.5f), markH, 1.2f);
        }
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

        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        int textW = std::min((int)w - 12, (int)g.getCurrentFont().getStringWidth(list[i].name) + 10);
        // Plate behind the label so it stays readable where the note map runs under it.
        g.setColour(UtilityColours::background.withAlpha(0.82f));
        g.fillRect(3, (int)y - 21, textW + 6, 19);
        g.setColour(UtilityColours::sectionText);
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

    drawHover(g, v);

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
    double delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (delta == 0.0) return;

    if (e.mods.isCommandDown())
    {
        if (delta > 0.0) zoomIn(); else zoomOut();
        return;
    }

    // A bare wheel seeks. The jump is a fraction of what is on screen, so a notch covers
    // the same proportion of the strip whether you are at 8 bars or the whole song.
    if (onSeek)
    {
        auto v = visibleRange();
        double step = (v.endQN - v.startQN) * SEEK_FRACTION_PER_NOTCH;
        if (e.mods.isShiftDown()) step *= 4.0;
        onSeek(delta * step);
    }
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

    // Empty time navigates. Objects will be editable here once authoring lands, which is
    // what the object-vs-empty-space rule in the design doc is about.
    if (inTrackArea(p) && onSeekTo)
        onSeekTo(snappedBarQN(p.y, visibleRange()));
}

void UtilityStripComponent::mouseMove(const juce::MouseEvent& e)
{
    int y = inTrackArea(e.getPosition()) ? e.getPosition().y : -1;
    if (y == hoverY) return;
    hoverY = y;
    repaint();
}

void UtilityStripComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverY < 0) return;
    hoverY = -1;
    repaint();
}
