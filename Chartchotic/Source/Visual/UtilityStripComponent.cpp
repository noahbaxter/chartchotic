#include "UtilityStripComponent.h"

namespace
{
    // Quarter notes in `seconds` at the tempo in force at `qn`.
    double secondsToQN(const TempoTimeSignatureMap* map, double qn, double seconds)
    {
        double bpm = 120.0;
        if (map != nullptr && !map->empty())
        {
            // Already falls back to 120 itself where the map has no tempo before this point.
            auto state = TempoTimeSignatureEventHelper::getEffectiveStateAtPPQ(*map, PPQ(qn));
            if (state.bpm > 0.0) bpm = state.bpm;
        }
        return seconds * bpm / 60.0;
    }

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

// The one place that decides what a legal view is. Clamped to the project and to nothing
// narrower: a chart is a region the window may sit inside, never a cage it sits in.
double UtilityStripComponent::barQN() const { return barLengthQN(tempoMap, viewStartQN); }

double UtilityStripComponent::minSpanQN() const { return MIN_ZOOM_BARS * barQN(); }

std::vector<double> UtilityStripComponent::zoomSpans() const
{
    return StripViewMath::zoomSpansQN(projectEndQN(), barQN(), MIN_ZOOM_BARS);
}

void UtilityStripComponent::setView(double startQN, double spanQN)
{
    auto v = StripViewMath::clamp(startQN, spanQN, projectEndQN(), minSpanQN());
    if (v.startQN == viewStartQN && v.spanQN == viewSpanQN) return;

    viewStartQN = v.startQN;
    viewSpanQN = v.spanQN;
    repaint();
}

void UtilityStripComponent::stepSpanAbout(double anchor, int direction)
{
    double span = StripViewMath::nextSpanQN(zoomSpans(), viewSpanQN, direction);
    auto v = StripViewMath::spanAbout({ viewStartQN, viewSpanQN }, anchor, span,
                                      projectEndQN(), minSpanQN());

    setView(v.startQN, v.spanQN);
}

void UtilityStripComponent::fitChart()
{
    const auto* song = nearestSong(playheadPPQ.toDouble());
    if (song == nullptr) { fitProject(); return; }

    setView(song->startQN, song->endQN - song->startQN);
}

void UtilityStripComponent::fitProject()
{
    setView(0.0, projectEndQN());
}

// Seeks rather than scrolls: a chart is a place on the timeline, so going to the next one is
// moving the cursor there and letting follow and the view do what they always do.
void UtilityStripComponent::gotoAdjacentChart(int direction)
{
    if (!hasSongs()) return;

    double now = playheadPPQ.toDouble();
    const auto& list = songs->getSongs();
    const SongList::Song* target = nullptr;

    if (direction > 0)
    {
        for (const auto& song : list)
            if (GridMath::after(song.startQN, now)) { target = &song; break; }
    }
    else
    {
        // Landing on the current chart's own start counts once, the way stepping back
        // through sections does, so the first press parks you at the top of the chart you
        // are in rather than skipping over it.
        for (const auto& song : list)
            if (GridMath::before(song.startQN, now)) target = &song;
    }

    if (target == nullptr) return;

    if (onSeekTo) onSeekTo(clampSeek(target->startQN));
    centreViewOn(target->startQN);
}

// Answers whether the view actually moved, which is what says a scroll counted as one.
bool UtilityStripComponent::scrollViewBy(double deltaQN)
{
    double before = viewStartQN;
    setView(viewStartQN + deltaQN, viewSpanQN);
    return viewStartQN != before;
}

void UtilityStripComponent::centreViewOn(double qn)
{
    setView(qn - viewSpanQN * FOLLOW_POSITION, viewSpanQN);
}

void UtilityStripComponent::zoomAbout(int y, int direction)
{
    // The point under the pointer is the one held still, in both directions: zooming is
    // looking, so it moves the view and never the playhead. During playback the pointer is
    // aimed at a moment that is sliding past, so the playhead is the only sensible anchor.
    double anchor = playing ? FOLLOW_POSITION
                            : (trackTop() + trackHeight() - y) / (double)trackHeight();

    if (playing)
    {
        double span = StripViewMath::nextSpanQN(zoomSpans(), viewSpanQN, direction);
        setView(playheadPPQ.toDouble() - anchor * span, span);
        return;
    }

    stepSpanAbout(anchor, direction);
}

UtilityStripComponent::View UtilityStripComponent::visibleRange() const
{
    return { viewStartQN, viewStartQN + viewSpanQN };
}

// Numbers only appear where they mean something: section starts, and wherever the cursor
// is. Numbering every chunk was the obnoxious version.
void UtilityStripComponent::drawMeasureNumber(juce::Graphics& g, float y, int measure,
                                              bool emphasised) const
{
    if (!filterOn(2)) return;

    auto font = Theme::getUIFont(MEASURE_PT);
    g.setFont(emphasised ? font.boldened() : font);
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

// How many bars a drawn grid line is worth at this zoom. Snapping and the ruler read the
// same number, so you can only ever land on a line you can see.
int UtilityStripComponent::visibleChunk(const View& v) const
{
    double len = barLengthQN(tempoMap, v.startQN);
    double pxPerBar = trackHeight() * len / std::max(1.0e-6, v.endQN - v.startQN);
    return chunkBars(pxPerBar);
}

// Walked from zero like every other grid question here, because chunk lines are counted in
// measures from the section they are in, not in QN from the origin.
double UtilityStripComponent::chunkLineAtOrBefore(double qn, const View& v) const
{
    int chunk = visibleChunk(v);
    double cur = 0.0;
    double best = 0.0;

    for (int guard = 0; guard < MAX_BARS_WALKED && cur <= qn; ++guard)
    {
        if ((measureAtQN(cur) - sectionStartMeasure(cur)) % chunk == 0) best = cur;
        cur += std::max(0.25, barLengthQN(tempoMap, cur));
    }
    return best;
}

// The chunk you are in, not the nearest one. Rounding to nearest puts the indicator above
// the cursor for the top half of every chunk, which reads as pointing at the wrong thing
// and is worst at the bottom edge where it collides with the pinned label.
//
// Snapping is to the drawn chunk grid rather than to every bar: the chunk doubles each
// time the visible span does, so the strip holds roughly the same number of snap points
// at any zoom instead of thousands of bar lines a pixel apart. Section starts always snap
// whatever the chunk, since they are the thing you are usually aiming for.
double UtilityStripComponent::snappedBarQN(int y, const View& v) const
{
    double qn = std::max(0.0, qnForY(y, v));
    double snapped = chunkLineAtOrBefore(qn, v);

    if (sections != nullptr)
        if (const auto* s = sections->sectionAt(PPQ(qn)))
            snapped = std::max(snapped, s->position.toDouble());

    return snapped;
}

void UtilityStripComponent::drawHover(juce::Graphics& g, const View& v) const
{
    if (hoverY < 0) return;

    // Mid-move the section being dragged is already drawn at the bar it would land on, with
    // its own line and number. Adding the cursor's own pair on top of it leaves you reading
    // which of two markers a few pixels apart is the one you are placing.
    if (dragActive && dragCommand == StripCommand::UpdateMove) return;

    // Over the frozen label, that section's own line and number stand in for this one.
    // Two numbers a few pixels apart, one for the bar under the cursor and one for the
    // section it would jump to, reads as a glitch rather than as two facts.
    if (hoverSection >= 0 && hoverSection == stickyIndex(v)) return;

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
    int chunk = visibleChunk(v);

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
    float y = (float)yForQN(s.position.toDouble() + movePreviewQN(index), v);

    int plateW = std::min(getWidth() - LABEL_INSET_X * 2,
                          sectionLabelFont().getStringWidth(s.name) + LABEL_TEXT_PAD);
    int top = (int)y - LABEL_H - LABEL_LINE_GAP;

    // The section you are currently inside pins to the bottom once its own line has
    // scrolled past, so its name is always readable and still clickable. The next section
    // pushes it off from above rather than overlapping it, which is what lets it leave
    // exactly when you cross into that section.
    if (index == (size_t)stickyIndex(v))
    {
        float bottom = (float)(trackTop() + trackHeight());
        float pinned = bottom - LABEL_H - LABEL_LINE_GAP;

        if (index + 1 < list.size())
        {
            // Clear of the next section's plate, not just of its line. That plate ends 2px
            // above the line, so pinning to the line leaves the two exactly touching, the
            // collision pass counts that as an overlap, and one of them is dropped: the next
            // name blinks out and only comes back once this one has scrolled away. The name
            // you are scrolling towards is the one that must survive.
            float nextY = (float)yForQN(list[index + 1].position.toDouble(), v);
            pinned = std::max(pinned, nextY - (float)LABEL_LINE_GAP + (float)LABEL_MIN_GAP);
        }
        top = std::min(top, (int)pinned);
    }

    return { LABEL_INSET_X, top, plateW, LABEL_H };
}

// Where the charted MIDI stops: the last note end on any mapped part, the end of the last
// clip, or an explicit [end] / [music_end] marker, whichever is later. 0 when none known.
double UtilityStripComponent::contentEndQN() const
{
    double end = sections != nullptr ? sections->getEndPPQ().toDouble() : 0.0;

    if (clipSpans != nullptr && !clipSpans->empty())
        end = std::max(end, clipSpans->back().endQN);

    // Last note rather than the whole sweep: this runs per paint and per mouse move, and
    // cached notes are in time order, so the tail of each part is one lookup.
    for (const auto& src : noteSources)
        if (src.notes != nullptr && !src.notes->empty())
            end = std::max(end, src.notes->back().endPPQ.toDouble());

    return end;
}

// End of the MIDI clip `qn` falls in, or 0 when there are no clips or it sits in a gap
// between them. Spans arrive merged and in order, so the first one that covers it wins.
double UtilityStripComponent::clipEndAt(double qn) const
{
    if (clipSpans == nullptr) return 0.0;

    for (const auto& span : *clipSpans)
    {
        if (span.startQN > qn) break;
        if (qn < span.endQN) return span.endQN;
    }
    return 0.0;
}

// The project, not the chart. Seeking is free to cross between charts now, so the only thing
// left to hold a position inside is the timeline itself.
double UtilityStripComponent::clampSeek(double qn) const
{
    double last = std::max(0.0, projectEndQN() - GridMath::TICK);
    return juce::jlimit(0.0, last, qn);
}

// Where section `i` stops. Normally the next section, but the end of its own clip cuts it
// short: with several charts on one timeline, the last section of one would otherwise run
// across the gap and into the start of the next. The final section of all falls back to
// the end of the MIDI, so it stops somewhere rather than owning the rest of the timeline.
double UtilityStripComponent::sectionEndQN(size_t i, const View& v) const
{
    const auto& list = sections->getSections();
    double qn = list[i].position.toDouble();
    bool last = i + 1 >= list.size();

    // Nothing after it, so it runs to wherever the MIDI itself stops.
    double stop = last ? std::max(contentEndQN(), v.endQN) : list[i + 1].position.toDouble();

    // Its own song's end cuts it short: the gap to the next chart is not part of any
    // section. The song rather than the merged clip spans, because those merge across every
    // track, and a guitar item reaching over a gap would hide it.
    const auto* song = songAt(qn);
    double clipEnd = song != nullptr ? song->endQN : clipEndAt(qn);
    if (clipEnd > qn) stop = std::min(stop, clipEnd);

    double marker = sections->endMarkerBetween(PPQ(qn), PPQ(stop)).toDouble();
    if (marker > qn) stop = std::min(stop, marker);

    return stop;
}

// Section containing the bottom of the view, ie the one the strip is currently inside.
int UtilityStripComponent::stickyIndex(const View& v) const
{
    if (sections == nullptr) return -1;
    const auto& list = sections->getSections();

    // Inside a section's own span, not merely after its line: past the end of a clip there
    // is nothing to be inside, so nothing pins until the next chart starts.
    for (int i = 0; i < (int)list.size(); ++i)
        if (list[(size_t)i].position <= PPQ(v.startQN) && v.startQN < sectionEndQN((size_t)i, v))
            return i;

    return -1;
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
    auto plates = labelPlates(visibleRange());

    for (size_t i = 0; i < plates.size(); ++i)
        if (!plates[i].isEmpty() && plates[i].contains(p)) return (int)i;
    return -1;
}

// Where each section's name plate goes, empty for the ones that do not get one. Zoomed
// out far enough, sections are a few pixels apart and every plate lands on the one below
// it: a column of half-covered names is less readable than no names at all. So a plate is
// dropped when it would collide with the last one kept, walking up from the bottom of the
// view. Lines and bands still mark every boundary, and the pinned label always survives,
// so the structure is intact and the one name you need is always there. Hit testing reads
// the same list, so a dropped name cannot be clicked where it is not drawn.
std::vector<juce::Rectangle<int>> UtilityStripComponent::labelPlates(const View& v) const
{
    std::vector<juce::Rectangle<int>> plates;
    if (sections == nullptr) return plates;

    const auto& list = sections->getSections();
    plates.assign(list.size(), juce::Rectangle<int>());

    const int bottom = trackTop() + trackHeight();

    auto clear = [&](const juce::Rectangle<int>& plate)
    {
        for (const auto& kept : plates)
            if (!kept.isEmpty()
                && plate.getBottom() + LABEL_MIN_GAP > kept.getY()
                && kept.getBottom() + LABEL_MIN_GAP > plate.getY())
                return false;
        return true;
    };

    auto place = [&](int i, bool claim)
    {
        if (i < 0 || i >= (int)list.size() || !plates[(size_t)i].isEmpty()) return;

        auto plate = sectionHeaderBounds((size_t)i, v);
        if (plate.isEmpty() || plate.getY() > bottom) return;
        if (!claim && !clear(plate)) return;

        plates[(size_t)i] = plate;
    };

    // The section the playhead is in claims its name before anything else can crowd it out.
    // In a cluster too tight for every label, the one you are inside is the one worth
    // reading, and it is the only one whose absence you would actually notice.
    place(litSectionIndex(v), true);

    // The pinned label is the same argument once its own line has scrolled below the view,
    // but it yields to the playhead's section on the rare frame where they differ.
    place(stickyIndex(v), false);

    // Everything else, bottom up, first come first served.
    for (int i = 0; i < (int)list.size(); ++i)
        place(i, false);

    return plates;
}

// The section the playhead is inside, for the banner over the highway. Empty in the gap
// between two charts, and empty when sections are filtered off: the strip's own filter is
// what says whether sections are being shown at all.
juce::String UtilityStripComponent::currentSectionName() const
{
    if (sections == nullptr || !filterOn(0)) return {};

    auto v = visibleRange();
    double now = playheadPPQ.toDouble();
    const auto& list = sections->getSections();

    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].position <= PPQ(now) && now < sectionEndQN(i, v))
            return list[i].name;

    return {};
}

// Which section is filled: the one the playhead is inside, and only that. It says where the
// song is, so it has to stay put while you read the rest of the strip; following the pointer
// made the fill chase the mouse and stop meaning anything. Pointing at a section still
// shows, through the hover band and the underline, which are separate.
//
// Only inside its own span though: in the gap between two charts there is nothing to light,
// even though the section before it is still the last one behind you.
int UtilityStripComponent::litSectionIndex(const View& v) const
{
    if (sections == nullptr) return -1;

    double referenceQN = playheadPPQ.toDouble();
    const auto& list = sections->getSections();

    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].position <= PPQ(referenceQN) && referenceQN < sectionEndQN(i, v))
            return (int)i;

    return -1;
}

// Full width each, because items cannot overlap: the blocks are a stack up the strip rather
// than a layout problem.
std::vector<juce::Rectangle<int>> UtilityStripComponent::songChunkBounds(const View& v) const
{
    std::vector<juce::Rectangle<int>> bounds;
    if (songs == nullptr) return bounds;

    for (const auto& song : songs->getSongs())
    {
        int top = (int)yForQN(song.endQN, v);
        int bot = (int)yForQN(song.startQN, v);
        bounds.push_back({ 0, top, getWidth(), std::max(1, bot - top) });
    }
    return bounds;
}

int UtilityStripComponent::songChunkAt(juce::Point<int> p) const
{
    if (!songOverview()) return -1;

    // Items cannot overlap, so nothing is stacked and the first match is the only match.
    auto bounds = songChunkBounds(visibleRange());
    for (int i = 0; i < (int)bounds.size(); ++i)
        if (bounds[(size_t)i].contains(p)) return i;

    return -1;
}

int UtilityStripComponent::songNameAt(juce::Point<int> p) const
{
    int i = songChunkAt(p);
    if (i < 0) return -1;

    auto r = songChunkBounds(visibleRange())[(size_t)i];
    return p.y <= r.getY() + LABEL_H ? i : -1;
}

// Laid down before anything else, so every line, band and note is drawn onto it. Clipped to
// the track rather than the whole component: the mode and layer rows above are chrome, and a
// chart reaching up into them would read as one of them being lit.
void UtilityStripComponent::drawChartGround(juce::Graphics& g, const View& v) const
{
    if (songs == nullptr) return;

    const float top = (float)trackTop();
    const float bottom = top + (float)trackHeight();

    g.setColour(UtilityColours::chartGround);

    for (const auto& song : songs->getSongs())
    {
        float hi = std::max((float)yForQN(song.endQN, v), top);
        float lo = std::min((float)yForQN(song.startQN, v), bottom);

        if (lo > hi) g.fillRect(0.0f, hi, (float)getWidth(), lo - hi);
    }
}

// At Session, zoomed out, the strip stops being one chart's sections and becomes the shelf
// the charts sit on: a block per EVENTS item, named, and the way into any of them.
void UtilityStripComponent::drawSongChunks(juce::Graphics& g, const View& v) const
{
    auto bounds = songChunkBounds(v);
    const auto& list = songs->getSongs();

    // Which chart the playhead is in, so the shelf says where you are rather than only what
    // is on it. Hover sits brighter still: one is where you are, the other where you are
    // about to go, and they have to be told apart when they are the same block.
    const auto* current = songAtPlayhead();

    for (size_t i = 0; i < bounds.size(); ++i)
    {
        auto r = bounds[i].reduced(1, 0);
        bool isCurrent = current != nullptr && &list[i] == current;

        g.setColour((int)i == hoverSongChunk ? UtilityColours::sectionGrabHover
                  : isCurrent                ? UtilityColours::sectionBandCurrent
                                             : UtilityColours::sectionBand);
        g.fillRect(r);
        g.setColour(isCurrent ? UtilityColours::sectionText : UtilityColours::sectionLine);
        g.drawRect(r, 1);

        g.setColour(UtilityColours::sectionText);
        g.setFont(sectionLabelFont());
        g.drawText(list[i].name,
                   r.reduced(SONG_CHUNK_INSET, 0).withTop(r.getY() + SONG_CHUNK_INSET)
                    .withHeight(LABEL_H),
                   juce::Justification::topLeft, true);
    }
}

// Bands go down before the note map so the notes keep their own colour: drawn over them,
// even a faint blue wash drags every lane towards blue.
void UtilityStripComponent::drawSectionBands(juce::Graphics& g, const View& v) const
{
    if (sections == nullptr) return;

    const float w = (float)getWidth();
    const float top = (float)trackTop();
    const float bottom = top + (float)trackHeight();
    const auto& list = sections->getSections();
    const int lit = litSectionIndex(v);

    for (size_t i = 0; i < list.size(); ++i)
    {
        double qn = list[i].position.toDouble();
        double endQN = sectionEndQN(i, v);
        if (endQN < v.startQN || qn > v.endQN) continue;

        // A section owns everything from its own line up to where it stops. Clamped at both
        // ends, so a section running off either edge fills what is visible of it rather
        // than bleeding past the track into the footer.
        float bandTop = juce::jlimit(top, bottom, (float)yForQN(std::min(endQN, v.endQN), v));
        float bandBottom = juce::jlimit(top, bottom, (float)yForQN(qn, v));
        if (bandBottom <= bandTop) continue;

        g.setColour((int)i == lit ? UtilityColours::sectionBandCurrent
                                  : UtilityColours::sectionBand);
        g.fillRect(0.0f, bandTop, w, bandBottom - bandTop);
    }
}

// Where a MIDI clip starts and stops, for the timeline either side of the charts on it.
// A section line at the same spot says the same thing better, so it wins.
void UtilityStripComponent::drawClipEdges(juce::Graphics& g, const View& v) const
{
    if (clipSpans == nullptr) return;

    const float w = (float)getWidth();
    const float dashes[] = { CLIP_EDGE_DASH, CLIP_EDGE_DASH };

    auto onSection = [this](double qn)
    {
        if (sections == nullptr) return false;
        for (const auto& s : sections->getSections())
            if (std::abs(s.position.toDouble() - qn) <= CLIP_EDGE_TOLERANCE_QN) return true;
        return false;
    };

    for (const auto& span : *clipSpans)
        for (double qn : { span.startQN, span.endQN })
        {
            if (qn < v.startQN || qn > v.endQN || onSection(qn)) continue;

            float y = (float)yForQN(qn, v);
            if (y < (float)trackTop() || y > (float)(trackTop() + trackHeight())) continue;

            g.setColour(UtilityColours::clipEdge);
            g.drawDashedLine(juce::Line<float>(0.0f, y, w, y), dashes, 2, 1.0f);
        }
}

void UtilityStripComponent::drawSections(juce::Graphics& g, const View& v) const
{
    if (sections == nullptr) return;

    const float w = (float)getWidth();
    const auto& list = sections->getSections();
    const float top = (float)trackTop();

    auto plates = labelPlates(v);

    // The bar the cursor snapped to. A section sitting on it is the thing a click would
    // land on, so it answers in the hover's own colour rather than staying structure blue.
    double hoverQN = hoverY >= 0 ? snappedBarQN(hoverY, v) : -1.0;

    for (size_t i = 0; i < list.size(); ++i)
    {
        // Where it is drawn, which during a move drag is not where it is stored. Everything
        // below works in preview space so the line, the name and the bar number travel as
        // one object under the cursor.
        double qn = list[i].position.toDouble() + movePreviewQN(i);
        double endQN = sectionEndQN(i, v);
        if (endQN < v.startQN || qn > v.endQN) continue;

        float y = (float)yForQN(qn, v);

        bool sticky = ((int)i == stickyIndex(v));
        if ((qn < v.startQN || qn > v.endQN) && !sticky) continue;

        // The number belongs to the name beside it, so it goes when the name goes.
        bool labelled = !plates[i].isEmpty();
        if (labelled && qn >= v.startQN && qn <= v.endQN)
            drawMeasureNumber(g, y, measureAtQN(qn), false);

        bool selected = isSelected((int)i);
        bool underCursor = GridMath::same(qn, hoverQN);

        // Editing on draws the grab target instead of implying it: this is the exact
        // rectangle sectionHeaderAt hit-tests, so the thing you can drag is the thing you
        // can see. The line stays thin on top of it, because the band says what you can
        // grab and the line still has to say where the section actually starts.
        if (sectionEditing())
        {
            auto row = sectionRowBounds(i, v);
            if (!row.isEmpty())
            {
                g.setColour(selected      ? UtilityColours::sectionGrabSelected
                            : underCursor ? UtilityColours::sectionGrabHover
                                          : UtilityColours::sectionGrab);
                g.fillRect(row);
            }
        }

        if (y >= top && y <= top + trackHeight())
        {
            g.setColour(selected     ? UtilityColours::selectedLine
                        : underCursor ? UtilityColours::hoverLine
                                      : UtilityColours::sectionLine);

            g.fillRect(0.0f, y, w, SECTION_LINE_H
                                   + (selected ? SECTION_LINE_SELECTED_EXTRA : 0.0f));
        }

        if (!labelled) continue;

        g.setFont(sectionLabelFont());
        // Straight from the hit-test bounds, so a pinned label is clickable where it is
        // drawn rather than where its line would have put it.
        auto plate = plates[i];

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
        g.setColour(nameHot || underCursor ? UtilityColours::uiText
                                          : UtilityColours::sectionText);
        g.drawText(list[i].name, plate.getX() + 3, plate.getY(), plate.getWidth() - 6,
                   plate.getHeight(), juce::Justification::centredLeft, true);

        // A frozen label sits nowhere near its own line, so hovering it underlines the
        // label in the hover's own white and hangs the measure it really starts on off the
        // end. That number is a long way from the bar under the cursor, which is the point:
        // the click goes back to the start of the section, not to where you pointed.
        if (sticky && (int)i == hoverSection)
        {
            // Kept inside the track: a line below the last row is drawn over by the footer.
            float ly = std::min((float)plate.getBottom() + STICKY_HOVER_LINE_GAP,
                                top + (float)trackHeight() - STICKY_HOVER_LINE_H);
            g.setColour(UtilityColours::hoverLine);
            g.fillRect(0.0f, ly, w, STICKY_HOVER_LINE_H);
            drawMeasureNumber(g, ly, measureAtQN(qn), true);
        }
    }
}

// The mode row. Coloured apart from the layer toggles below it on purpose: one says what the
// strip draws, the other what the mouse does to it, and confusing the two costs you a chart.
void UtilityStripComponent::drawModes(juce::Graphics& g) const
{
    g.setColour(UtilityColours::background);
    g.fillRect(0, 0, getWidth(), MODE_ROW_H);

    g.setFont(Theme::getUIFont(FILTER_PT));

    for (int i = 0; i < (int)std::size(MODES); ++i)
    {
        auto seg = modeButtonBounds(i).reduced(FILTER_GAP, FILTER_GAP);
        bool on = editMode == MODES[i].mode;

        g.setColour(on ? UtilityColours::modeActiveFill : UtilityColours::panelFill);
        g.fillRect(seg);
        g.setColour(on ? UtilityColours::modeActiveLine : UtilityColours::chunkLine);
        g.drawRect(seg, 1);

        g.setColour(on ? UtilityColours::modeActiveText : UtilityColours::label);
        g.drawFittedText(juce::String("edit ") + MODES[i].label, seg.reduced(3, 0),
                         juce::Justification::centred, 1, 0.7f);
    }

    g.setColour(UtilityColours::border);
    g.fillRect(0, MODE_ROW_H - 1, getWidth(), 1);
}

// A segmented row of toggles, each one lit when its layer is on, so what the strip is
// showing is readable without opening anything.
void UtilityStripComponent::drawFilters(juce::Graphics& g) const
{
    g.setColour(UtilityColours::panelFill);
    g.fillRect(0, MODE_ROW_H, getWidth(), FILTER_ROW_H);

    g.setFont(Theme::getUIFont(FILTER_PT));

    for (int i = 0; i < (int)std::size(filters); ++i)
    {
        auto seg = filterButtonBounds(i).reduced(FILTER_GAP, FILTER_GAP);
        bool on = filters[i].on;

        // Follow has three states rather than two: off, on and driving the view, and on but
        // taken out of gear by a scroll. The third needs its own colour, or a view that has
        // quietly stopped tracking looks exactly like one that is tracking.
        bool held = on && i == FILTER_FOLLOW && !followEngaged;

        g.setColour(held ? UtilityColours::followHeldFill
                         : on ? UtilityColours::sectionBandCurrent
                              : UtilityColours::background);
        g.fillRect(seg);
        g.setColour(held ? UtilityColours::followHeldLine
                         : on ? UtilityColours::sectionLine
                              : UtilityColours::chunkLine);
        g.drawRect(seg, 1);

        // Fitted rather than plain: at four across a 200px strip "measures" is wider than
        // its segment, and a shrunk word reads where a clipped one does not.
        g.setColour(held ? UtilityColours::followHeldText
                         : on ? UtilityColours::uiText
                              : UtilityColours::label);
        g.drawFittedText(filters[i].label, seg.reduced(3, 0),
                         juce::Justification::centred, 1, 0.7f);
    }

    // Where the track starts, not how tall the filter block is, so anything added above the
    // filters moves this line with it rather than leaving it stranded.
    g.setColour(UtilityColours::border);
    g.fillRect(0, trackTop() - 1, getWidth(), 1);
}

void UtilityStripComponent::drawFooter(juce::Graphics& g) const
{
    auto minus = minusBounds();
    auto plus = plusBounds();

    g.setColour(UtilityColours::panelFill);
    g.fillRect(0, zoomRowTop(), getWidth(), footerAreaHeight());

    g.setFont(Theme::getUIFont(FOOTER_BUTTON_PT));
    bool canOut = viewSpanQN < projectEndQN() - GridMath::TICK;
    bool canIn  = viewSpanQN > minSpanQN() + GridMath::TICK;

    for (auto [r, text, enabled] : { std::tuple{ minus, "-", canOut },
                                     std::tuple{ plus,  "+", canIn } })
    {
        g.setColour(UtilityColours::chunkLineBig);
        g.drawRect(r, 1);
        g.setColour(enabled ? UtilityColours::uiText : UtilityColours::label);
        g.drawText(text, r, juce::Justification::centred, false);
    }

    g.setColour(UtilityColours::uiText);
    g.setFont(Theme::getUIFont(FOOTER_LABEL_PT));
    // Counted in bars at every width now that there are no named rungs left, except the one
    // that covers everything: "project" says more there than a bar count nobody can picture.
    double bars = viewSpanQN / std::max(1.0e-6, barLengthQN(tempoMap, viewStartQN));
    juce::String extent = viewSpanQN >= projectEndQN() - GridMath::TICK
                        ? juce::String("project")
                        : juce::String(juce::roundToInt(bars)) + " bars";

    g.drawText(extent,
               plus.getRight() + 6, plus.getY(), getWidth() - plus.getRight() - 10,
               plus.getHeight(), juce::Justification::centredLeft, false);

    // Greyed rather than hidden at the ends of the project, so the pair keeps its shape and
    // you can see there is nothing that way rather than watching a button vanish.
    double now = playheadPPQ.toDouble();
    bool hasPrev = false, hasNext = false;
    if (songs != nullptr)
        for (const auto& song : songs->getSongs())
        {
            if (GridMath::before(song.startQN, now)) hasPrev = true;
            if (GridMath::after(song.startQN, now))  hasNext = true;
        }

    // Between two charts there is no name to give, and "nearest" would be a different claim
    // from "the one you are in", so it says nothing rather than something not quite true.
    const auto* inChart = songAt(now);
    g.setColour(inChart != nullptr ? UtilityColours::sectionText : UtilityColours::label);
    g.setFont(Theme::getUIFont(FOOTER_LABEL_PT));
    g.drawText(inChart != nullptr ? inChart->name : juce::String("no chart"),
               4, navNameTop(), getWidth() - 8, NAV_NAME_H,
               juce::Justification::centred, true);

    g.setFont(Theme::getUIFont(FOOTER_LABEL_PT));
    for (auto [r, text, enabled] : { std::tuple{ prevChartBounds(), "< prev chart", hasPrev },
                                     std::tuple{ nextChartBounds(), "next chart >", hasNext } })
    {
        g.setColour(UtilityColours::chunkLineBig);
        g.drawRect(r, 1);
        g.setColour(enabled ? UtilityColours::uiText : UtilityColours::label);
        g.drawText(text, r, juce::Justification::centred, false);
    }
}

void UtilityStripComponent::paint(juce::Graphics& g)
{
    g.fillAll(UtilityColours::background);

    auto v = visibleRange();
    drawChartGround(g, v);
    drawChunkLines(g, v);

    // One block per song instead of every section in the project, which at this zoom is a
    // wash of names nobody can read. The clip edges are the blocks now, so they go too.
    if (songOverview())
    {
        drawSongChunks(g, v);
    }
    else
    {
        if (filterOn(0)) drawSectionBands(g, v);
        if (filterOn(1)) drawNotes(g, v);
        if (filterOn(0)) drawSections(g, v);
        drawClipEdges(g, v);
    }

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
        g.setFont(Theme::getUIFont(PLAYHEAD_TAB_PT).boldened());
        auto text = juce::String(measureAtQN(playheadPPQ.toDouble()));
        int tw = g.getCurrentFont().getStringWidth(text) + 12;
        int th = 16;
        auto tab = juce::Rectangle<int>(getWidth() - tw, (int)py - th - 3, tw, th);

        juce::Path tabShape;
        tabShape.addRoundedRectangle((float)tab.getX(), (float)tab.getY(),
                                     (float)tab.getWidth(), (float)tab.getHeight(),
                                     3.0f, 3.0f, true, false, true, false);
        g.setColour(UtilityColours::playheadTab);
        g.fillPath(tabShape);
        g.setColour(UtilityColours::background);
        g.drawText(text, tab, juce::Justification::centred, false);
    }

    drawModes(g);
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

    double spanBefore = viewSpanQN;
    double viewBefore = viewStartQN;

    if (e.mods.isCommandDown())
    {
        zoomAbout(e.getPosition().y, delta > 0.0 ? -1 : 1);
    }
    // Shift skips structure: one notch, one moment worth stopping at, which is the jump you
    // actually want when reading a chart rather than scrubbing through it. The one wheel
    // gesture that moves the playhead, and it brings the view with it rather than leaving you
    // to find where it went. Leaving the chart you are in takes a second notch, since that is
    // the one jump that is easy to make by accident and annoying to undo.
    //
    // Deliberately does not drop the follow latch: it is a jump to somewhere specific, not
    // the reading-ahead that follow gets out of the way of, so playback carries on as it was.
    else if (e.mods.isShiftDown())
    {
        double target = clampSeek(resistChartCross(snapStep(delta > 0.0 ? 1 : -1)));
        if (onSeekTo) onSeekTo(target);
        centreViewOn(target);
    }
    // A bare wheel scrolls the view free of the grid: the wheel's own travel, scaled, so a
    // trackpad glides instead of clacking between bar lines. Snapping lives where it is
    // aimed, on hover and on click. The playhead does not come along.
    //
    // Only a scroll that moved something takes follow out of gear, and only while the
    // transport is running. Spinning the wheel on a fixed-span view, or against the end of a
    // chart, is not you asking to read elsewhere. Neither is scrolling while paused: follow
    // does nothing then and play re-arms it, so the latch would read out of gear over a view
    // nothing was chasing.
    else
    {
        auto v = visibleRange();
        double qnPerPx = (v.endQN - v.startQN) / trackHeight();

        if (scrollViewBy(delta * WHEEL_PIXELS_PER_UNIT * qnPerPx) && playing)
            followEngaged = false;
    }

    // A notch that moved nothing moved nothing under the pointer either, so the hover it is
    // already showing is still aimed at what it says. Same test the follow latch uses.
    if (viewSpanQN == spanBefore && viewStartQN == viewBefore) return;

    // The pointer has not moved, but everything under it has. Keeping the hover where the
    // cursor happens to sit means the highlight jumps between sections as they slide past,
    // which reads as the strip flickering rather than scrolling.
    //
    // Held down by position rather than by event: a scroll is followed by a mouseMove at
    // the same place, so simply waiting for the next move brings the hover straight back.
    // It stays away until the pointer is somewhere it was not when the scroll began.
    hoverHeldFrom = e.getPosition();
    hoverHeld = true;
    clearHover();
}

// The next section line from the playhead in `direction`, or where we already are when
// there is none that way. Stepping back from inside a section lands on its own start
// first, the way a transport's previous-marker key does.
double UtilityStripComponent::nearestBarLine(double qn) const
{
    double before = barLineAtOrBefore(qn);
    double after = before + std::max(0.25, barLengthQN(tempoMap, before));

    return (qn - before) <= (after - qn) ? before : after;
}

// Every moment worth stopping at, which is a different question from every section start.
// A chart's edges matter, a section's end matters where it is not just the next one's start,
// and a scrap of MIDI sitting on its own out past the charts matters because finding it any
// other way means scrubbing blind through empty timeline.
std::vector<double> UtilityStripComponent::snapPoints() const
{
    std::vector<double> points { 0.0, projectEndQN() };

    if (songs != nullptr)
        for (const auto& song : songs->getSongs())
        {
            points.push_back(song.startQN);
            points.push_back(song.endQN);
        }

    if (sections != nullptr)
    {
        const auto& list = sections->getSections();
        for (size_t i = 0; i < list.size(); ++i)
        {
            double start = list[i].position.toDouble();
            points.push_back(start);

            // Where the next section starts this is a duplicate and merges away, which is
            // the common case. It earns its place on the last section of a chart, whose end
            // is the chart's rather than another section's.
            const auto* song = songAt(start);
            points.push_back(i + 1 < list.size() ? list[i + 1].position.toDouble()
                           : song != nullptr    ? song->endQN
                                                : projectEndQN());
        }
    }

    // Only out past the charts. Inside one the sections already say where everything is, and
    // clustering there would bury them under a stop per phrase.
    std::vector<StripViewMath::NoteSpan> loose;
    for (const auto& src : noteSources)
    {
        if (src.notes == nullptr) continue;

        for (const auto& note : *src.notes)
        {
            double start = note.startPPQ.toDouble();
            if (songAt(start) != nullptr) continue;

            loose.push_back({ start, note.endPPQ.toDouble() });
        }
    }

    std::sort(loose.begin(), loose.end(),
              [](const StripViewMath::NoteSpan& a, const StripViewMath::NoteSpan& b)
              { return a.startQN < b.startQN; });

    double gapQN = CLUSTER_GAP_BARS * std::max(0.25, barLengthQN(tempoMap, 0.0));
    for (const auto& span : StripViewMath::clusterSpans(loose, gapQN))
    {
        points.push_back(nearestBarLine(span.startQN));
        points.push_back(nearestBarLine(span.endQN));
    }

    return StripViewMath::mergedPoints(std::move(points), GridMath::TICK);
}

const SongList::Song* UtilityStripComponent::chartContaining(double qn) const
{
    if (songs == nullptr) return nullptr;

    for (const auto& song : songs->getSongs())
        if (qn >= song.startQN - GridMath::TICK && qn <= song.endQN + GridMath::TICK)
            return &song;

    return nullptr;
}

// Leaving a chart is the jump worth making you mean it. Held at the edge you are leaving by,
// which is itself a snap point, so the first notch parks you on the boundary and the second
// takes you off it. Resting re-arms, the same way every other detent here does, so a notch
// after a pause never springs you into the next chart.
double UtilityStripComponent::resistChartCross(double targetQN)
{
    double now = playheadPPQ.toDouble();
    const auto* from = chartContaining(now);

    // Not in a chart, so there is no edge of one to be held at. Wandering the empty timeline
    // between charts is not a crossing.
    if (from == nullptr) { crossDetent.clear(); return targetQN; }
    if (chartContaining(targetQN) == from) { crossDetent.clear(); return targetQN; }

    double edge = targetQN > now ? from->endQN : from->startQN;

    return crossDetent.resolveCounted(targetQN, edge, CHART_CROSS_NOTCHES,
                                      (long long)juce::Time::currentTimeMillis(),
                                      CHART_CROSS_RESET_MS);
}

double UtilityStripComponent::snapStep(int direction) const
{
    auto points = snapPoints();
    if (points.empty()) return playheadPPQ.toDouble();

    double now = playheadPPQ.toDouble();

    if (direction > 0)
    {
        for (double p : points)
            if (GridMath::after(p, now)) return p;

        return points.back();
    }

    // Landing on the point you are sitting just past is the right answer once, the way a
    // transport's previous-marker key works. Press again while still near it and you meant
    // the one before, which is also what stops playback carrying you back onto it every
    // notch.
    double from = now;
    for (auto it = points.rbegin(); it != points.rend(); ++it)
        if (GridMath::before(*it, now))
        {
            if (now - *it <= secondsToQN(tempoMap, now, SECTION_BACKSTEP_GRACE_S)) from = *it;
            break;
        }

    double best = points.front();
    for (double p : points)
    {
        if (!GridMath::before(p, from)) break;
        best = p;
    }
    return best;
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
    // Held by position rather than by index, because sorting renumbers everything after a
    // move. Without this a drag deselects the section you just dragged, so moving one and
    // then deleting it takes two separate clicks on the same object.
    std::vector<double> wasSelected;
    for (int i : selection)
        if (i >= 0 && i < (int)edits.size()) wasSelected.push_back(edits[(size_t)i].qn);

    std::sort(edits.begin(), edits.end(),
              [](const SectionEdit& a, const SectionEdit& b) { return a.qn < b.qn; });

    if (onSectionsEdited) onSectionsEdited(edits);

    selection.clear();
    for (double qn : wasSelected)
        for (int i = 0; i < (int)edits.size(); ++i)
            if (GridMath::same(edits[(size_t)i].qn, qn)) { selection.push_back(i); break; }

    repaint();
}

bool UtilityStripComponent::deleteSelection()
{
    if (selection.empty() || sections == nullptr) return false;

    auto edits = currentEdits();

    // Highest index first, so each erase leaves the lower ones where they were.
    auto doomed = selection;
    std::sort(doomed.begin(), doomed.end(), std::greater<int>());

    for (int i : doomed)
        if (i >= 0 && i < (int)edits.size()) edits.erase(edits.begin() + i);

    selection.clear();
    commitSections(std::move(edits));
    return true;
}

void UtilityStripComponent::beginRename(int index)
{
    if (sections == nullptr || index < 0
        || index >= (int)sections->getSections().size()) return;

    renameIndex = index;
    renameTarget = RenameTarget::Section;
    auto v = visibleRange();
    auto plate = sectionHeaderBounds((size_t)index, v);

    renameEditor = std::make_unique<juce::TextEditor>();
    // The same face and size as the label it sits on top of, so the name does not jump as
    // it goes from drawn to editable.
    renameEditor->setFont(sectionLabelFont());
    renameEditor->setText(sections->getSections()[(size_t)index].name, false);
    renameEditor->selectAll();
    renameEditor->setBounds(plate.withWidth(std::max(90, getWidth() - plate.getX() - 6)));
    renameEditor->onReturnKey = [this] { commitRename(); };
    renameEditor->onEscapeKey = [this] { renameEditor.reset(); renameIndex = -1; };
    renameEditor->onFocusLost = [this] { commitRename(); };

    addAndMakeVisible(*renameEditor);
    renameEditor->grabKeyboardFocus();
}

void UtilityStripComponent::beginSongRename(int index)
{
    const auto* song = songs != nullptr ? songs->at(index) : nullptr;
    if (song == nullptr) return;

    renameIndex = index;
    renameTarget = RenameTarget::Song;

    auto r = songChunkBounds(visibleRange())[(size_t)index];

    renameEditor = std::make_unique<juce::TextEditor>();
    renameEditor->setFont(sectionLabelFont());
    renameEditor->setText(song->name, false);
    renameEditor->selectAll();
    renameEditor->setBounds(r.reduced(SONG_CHUNK_INSET, 0)
                             .withTop(r.getY() + SONG_CHUNK_INSET).withHeight(LABEL_H));
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
    auto target = renameTarget;
    renameEditor.reset();
    renameIndex = -1;

    if (target == RenameTarget::Song)
    {
        // Clearing a song's name is meaningful, unlike a section's: an item with no name
        // falls back to its ordinal rather than disappearing.
        const auto* song = songs != nullptr ? songs->at(index) : nullptr;
        if (song != nullptr && name != song->name && onSongRenamed)
            onSongRenamed(song->startQN, name);
        else
            repaint();
        return;
    }

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
            if (onSeekTo) onSeekTo(clampSeek(snappedBarQN(p.y, v)));
            break;

        case StripCommand::JumpToSection:
            if (onSeekTo && object >= 0)
                onSeekTo(clampSeek(sections->getSections()[(size_t)object].position.toDouble()));
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
            if (!GridMath::same(dragMoveDeltaQN, 0.0) && !selection.empty())
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
                // Cleared first, or the commit tries to re-select by a position that is
                // about to belong to a different section.
                selection.clear();
                edits.erase(edits.begin() + object);
                commitSections(std::move(edits));
            }
            else
            {
                double qn = snappedBarQN(p.y, v);
                // Nothing to create if a section already starts on this bar.
                for (const auto& ed : edits)
                    if (GridMath::same(ed.qn, qn)) return;

                edits.push_back({ qn, "section" });
                std::sort(edits.begin(), edits.end(),
                          [](const SectionEdit& a, const SectionEdit& b) { return a.qn < b.qn; });
                if (onSectionsEdited) onSectionsEdited(edits);

                // Rename opens straight away: a chart full of "section" is the failure mode
                // the first attempt at this shipped with.
                for (int i = 0; i < (int)edits.size(); ++i)
                    if (GridMath::same(edits[(size_t)i].qn, qn)) { beginRename(i); break; }
            }
            break;
        }

        case StripCommand::DeleteAt:
            if (object >= 0)
            {
                auto edits = currentEdits();
                selection.clear();
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

    hoverHeld = false;   // clicking is aiming at something, whatever the scroll left behind

    if (renameEditor != nullptr) commitRename();

    // Clicking the mode you are already in leaves it, so the same button arms and disarms
    // rather than needing somewhere else to click to get back to plain navigation.
    for (int i = 0; i < (int)std::size(MODES); ++i)
        if (modeButtonBounds(i).contains(p))
        {
            setEditMode(editMode == MODES[i].mode ? EditMode::None : MODES[i].mode);
            return;
        }

    for (int i = 0; i < (int)std::size(filters); ++i)
        if (filterButtonBounds(i).contains(p))
        {
            // Yellow means follow is on but a scroll took it out of gear. Clicking there
            // obviously means start following again, so it re-engages rather than switching
            // follow off and making you click twice to get back to where you were.
            if (i == FILTER_FOLLOW && filters[i].on && !followEngaged)
            {
                followEngaged = true;
                repaint();
                return;
            }

            filters[i].on = !filters[i].on;
            if (onLayerToggled) onLayerToggled(filters[i].key, filters[i].on);

            // Switching follow back on puts it in gear too, so it never reads on while
            // doing nothing.
            if (i == FILTER_FOLLOW && filters[i].on) followEngaged = true;

            repaint();
            return;
        }

    if (minusBounds().contains(p)) { zoomOut(); return; }
    if (plusBounds().contains(p))  { zoomIn();  return; }

    if (prevChartBounds().contains(p)) { gotoAdjacentChart(-1); return; }
    if (nextChartBounds().contains(p)) { gotoAdjacentChart(1);  return; }
    if (!inTrackArea(p)) return;

    // In the overview the chunks are the only objects there are, so they take the click
    // before the section mapping sees it. Name renames, body jumps in.
    if (songOverview())
    {
        int songName = songNameAt(p);
        if (songName >= 0 && !e.mods.isRightButtonDown()) { beginSongRename(songName); return; }

        // A single click only takes you to the song, so the overview stays browsable:
        // entering one is the double click, the same as opening anything else.
        int chunk = songChunkAt(p);
        if (chunk >= 0)
        {
            if (onSeekTo) onSeekTo(songs->getSongs()[(size_t)chunk].startQN);
            return;
        }
    }

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

    // Opening a song lands the playhead at its start, because a click is the gesture that
    // moves the cursor, and frames the whole chart. The name is not part of the target:
    // double clicking there is two renames, not a way in.
    if (songOverview() && songNameAt(e.getPosition()) < 0)
    {
        int chunk = songChunkAt(e.getPosition());
        const auto* song = songs != nullptr ? songs->at(chunk) : nullptr;
        if (song != nullptr)
        {
            if (onSeekTo) onSeekTo(clampSeek(song->startQN));
            setView(song->startQN, song->endQN - song->startQN);
            return;
        }
    }
    dispatch(mapper.resolve(stripMode(), EventType::DoubleClick, buttonOf(e),
                            modsOf(e), sectionHeaderAt(e.getPosition()) >= 0), e);
}

void UtilityStripComponent::mouseMove(const juce::MouseEvent& e)
{
    // Still parked where the scroll left it, so this is the scroll's own trailing move
    // rather than the pointer going anywhere.
    if (hoverHeld)
    {
        if (e.getPosition() == hoverHeldFrom) return;
        hoverHeld = false;
    }

    int y = inTrackArea(e.getPosition()) ? e.getPosition().y : -1;
    int section = y >= 0 ? sectionHeaderAt(e.getPosition()) : -1;
    int name = y >= 0 ? sectionNameAt(e.getPosition()) : -1;
    int chunk = y >= 0 ? songChunkAt(e.getPosition()) : -1;
    int songName = y >= 0 ? songNameAt(e.getPosition()) : -1;

    if (y == hoverY && section == hoverSection && name == hoverName
        && chunk == hoverSongChunk) return;

    hoverY = y;
    hoverSection = section;
    hoverName = name;
    hoverSongChunk = chunk;
    setMouseCursor(name >= 0 || songName >= 0 ? juce::MouseCursor::IBeamCursor
                                              : juce::MouseCursor::NormalCursor);
    repaint();
}

// Forgets where the pointer was, so the lit section falls back to the playhead. Hover comes
// back on the next real mouseMove.
void UtilityStripComponent::clearHover()
{
    if (hoverY < 0 && hoverSection < 0 && hoverName < 0 && hoverSongChunk < 0) return;
    hoverY = -1;
    hoverSection = -1;
    hoverName = -1;
    hoverSongChunk = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void UtilityStripComponent::mouseExit(const juce::MouseEvent&)
{
    hoverHeld = false;   // leaving and coming back is pointing again
    clearHover();
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
