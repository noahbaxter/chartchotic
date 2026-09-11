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

// Numbers only appear where they mean something: section starts, and wherever the cursor
// is. Numbering every chunk was the obnoxious version.
void UtilityStripComponent::drawMeasureNumber(juce::Graphics& g, float y, int measure,
                                              bool emphasised) const
{
    if (!filterOn(2)) return;

    g.setFont(juce::Font(juce::FontOptions(10.0f, emphasised ? juce::Font::bold
                                                             : juce::Font::plain)));
    auto text = juce::String(measure);
    int tw = g.getCurrentFont().getStringWidth(text) + 8;
    int tx = getWidth() - tw - 3;
    int ty = (int)y + 2;

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

    drawMeasureNumber(g, y, measureAtQN(barQN), true);
}

void UtilityStripComponent::drawChunkLines(juce::Graphics& g, const View& v) const
{
    double len = barLengthQN(tempoMap, v.startQN);
    double pxPerBar = trackHeight() * len / std::max(1.0e-6, v.endQN - v.startQN);
    int chunk = chunkBars(pxPerBar);

    double firstBar = std::floor(v.startQN / len) * len;
    const float w = (float)getWidth();

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

juce::Rectangle<int> UtilityStripComponent::sectionHeaderBounds(size_t index, const View& v) const
{
    if (sections == nullptr || index >= sections->getSections().size()) return {};
    const auto& s = sections->getSections()[index];
    float y = (float)yForQN(s.position.toDouble(), v);

    juce::Font f(juce::FontOptions(16.0f, juce::Font::bold));
    int tw = std::min(getWidth() - 12, f.getStringWidth(s.name) + 10);
    return { 3, (int)y - 21, tw + 6, 19 };
}

int UtilityStripComponent::sectionHeaderAt(juce::Point<int> p) const
{
    if (sections == nullptr) return -1;
    auto v = visibleRange();
    const auto& list = sections->getSections();

    for (size_t i = 0; i < list.size(); ++i)
    {
        double qn = list[i].position.toDouble();
        if (qn < v.startQN || qn > v.endQN) continue;
        if (sectionHeaderBounds(i, v).contains(p)) return (int)i;
    }
    return -1;
}

void UtilityStripComponent::drawSections(juce::Graphics& g, const View& v) const
{
    if (sections == nullptr) return;

    const float w = (float)getWidth();
    const auto& list = sections->getSections();
    const float top = (float)trackTop();

    // The section the cursor is inside gets a stronger fill, so hovering tells you which
    // region you are in, not just which bar.
    const SongSections::Section* hovered = nullptr;
    if (hoverY >= 0)
        hovered = sections->sectionAt(PPQ(qnForY(hoverY, v)));

    for (size_t i = 0; i < list.size(); ++i)
    {
        double qn = list[i].position.toDouble();
        double nextQN = (i + 1 < list.size()) ? list[i + 1].position.toDouble() : v.endQN;
        if (nextQN < v.startQN || qn > v.endQN) continue;

        float y = (float)yForQN(qn, v);
        float yTop = (float)yForQN(std::min(nextQN, v.endQN), v);

        bool isHovered = (hovered != nullptr && &list[i] == hovered);
        g.setColour(isHovered ? UtilityColours::sectionBandHover : UtilityColours::sectionBand);
        g.fillRect(0.0f, std::max(yTop, top), w, std::max(1.0f, y - std::max(yTop, top)));

        if (qn < v.startQN || qn > v.endQN) continue;

        drawMeasureNumber(g, y, measureAtQN(qn), false);

        bool selected = isSelected((int)i);

        // A move drag previews by shifting the selected markers rather than committing.
        if (selected && dragActive && dragCommand == StripCommand::UpdateMove)
            y = (float)yForQN(qn + dragMoveDeltaQN, v);

        g.setColour(selected ? UtilityColours::selectedLine : UtilityColours::sectionLine);
        g.fillRect(0.0f, y, w, selected ? 3.0f : 2.0f);

        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        int textW = std::min((int)w - 12, (int)g.getCurrentFont().getStringWidth(list[i].name) + 10);
        // Plate behind the label so it stays readable where the note map runs under it.
        g.setColour(UtilityColours::background.withAlpha(0.82f));
        g.fillRect(3, (int)y - 21, textW + 6, 19);
        if (selected)
        {
            g.setColour(UtilityColours::selectedLine);
            g.drawRect(3, (int)y - 21, textW + 6, 19, 1);
        }
        g.setColour(UtilityColours::sectionText);
        g.drawText(list[i].name, 6, (int)y - 21, (int)w - 12, 19,
                   juce::Justification::centredLeft, true);

        // Rename is an affordance, not a gesture, so it shows on hover in every mode.
        if ((int)i == hoverSection && renameIndex < 0)
        {
            auto pb = pencilBounds((int)i, v);
            g.setColour(UtilityColours::background.withAlpha(0.9f));
            g.fillRect(pb);
            g.setColour(UtilityColours::sectionText);
            g.drawRect(pb, 1);
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.drawText("edit", pb, juce::Justification::centred, false);
        }
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
        g.fillRect(0.0f, py - 3.0f, w, 7.0f);
        g.setColour(UtilityColours::playhead);
        g.fillRect(0.0f, py - 1.0f, w, 2.5f);

        juce::Path nub;
        nub.addTriangle(0.0f, py - 5.0f, 0.0f, py + 5.0f, 6.0f, py);
        nub.addTriangle(w, py - 5.0f, w, py + 5.0f, w - 6.0f, py);
        g.fillPath(nub);
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
        double stepQN = chunkBars(pxPerBar) * len * (e.mods.isShiftDown() ? 4.0 : 1.0);

        // Snap to the chunk grid first, so a scroll from anywhere lands on a line rather
        // than carrying an arbitrary offset along with it.
        double from = std::round(playheadPPQ.toDouble() / stepQN) * stepQN;
        onSeekTo(std::max(0.0, from + (delta > 0.0 ? stepQN : -stepQN)));
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

juce::Rectangle<int> UtilityStripComponent::pencilBounds(int sectionIndex, const View& v) const
{
    auto plate = sectionHeaderBounds((size_t)sectionIndex, v);
    if (plate.isEmpty()) return {};
    return { plate.getRight() + 2, plate.getY() + 3, 13, 13 };
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

    // The pencil is an affordance rather than a gesture, so it works in every mode and
    // never competes with click or double click.
    if (hoverSection >= 0 && pencilBounds(hoverSection, visibleRange()).contains(p))
    {
        beginRename(hoverSection);
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

    // Keep the pencil up while the pointer is on it, not just on the name plate.
    if (section < 0 && hoverSection >= 0
        && pencilBounds(hoverSection, visibleRange()).contains(e.getPosition()))
        section = hoverSection;

    if (y == hoverY && section == hoverSection) return;
    hoverY = y;
    hoverSection = section;
    repaint();
}

void UtilityStripComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverY < 0 && hoverSection < 0) return;
    hoverY = -1;
    hoverSection = -1;
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
