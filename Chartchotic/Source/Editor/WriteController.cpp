#include "WriteController.h"
#include "LaneMath.h"
#include "../Midi/Utils/MidiConstants.h"
#include "../Midi/Utils/GridMath.h"
#include <algorithm>
#include <limits>
#include <map>

namespace
{
    const juce::Identifier kWriteSubMode      { "writeSubMode" };
    const juce::Identifier kWriteStepDivision { "writeStepDivision" };
    const juce::Identifier kWriteTuplet       { "writeTuplet" };
    const juce::Identifier kWriteSnap         { "writeSnap" };
    const juce::Identifier kEditorView        { "editorView" };

    static const juce::String kSubModeDraw { "draw" };
    static const juce::String kSubModeEdit { "edit" };

    // Spelled out rather than saved as the enum's index, so reordering the views cannot
    // silently reopen a project on a different one.
    const struct { EditorView view; const char* name; } kViewNames[] = {
        { EditorView::Notes,     "notes" },
        { EditorView::StarPower, "sp" },
        { EditorView::Phrase,    "phrase" },
        { EditorView::Sections,  "sections" },
        { EditorView::Tempo,     "tempo" },
    };

    SubMode parseSubMode(const juce::String& s, SubMode fallback)
    {
        if (s == kSubModeEdit) return SubMode::Edit;
        if (s == kSubModeDraw) return SubMode::Draw;
        return fallback;
    }

    const juce::String& subModeToString(SubMode m)
    {
        return (m == SubMode::Edit) ? kSubModeEdit : kSubModeDraw;
    }

    EditorView parseView(const juce::String& s, EditorView fallback)
    {
        for (const auto& v : kViewNames)
            if (s == v.name)
                // A project saved on the old SP-only view reopens on Phrase, which covers it.
                return v.view == EditorView::StarPower ? EditorView::Phrase : v.view;
        return fallback;
    }

    juce::String viewToString(EditorView view)
    {
        for (const auto& v : kViewNames)
            if (view == v.view) return v.name;
        return kViewNames[0].name;
    }
}

//==============================================================================

WriteController::WriteController(juce::ValueTree& st)
    : state(st)
{
    loadPersistedState();
}

void WriteController::loadPersistedState()
{
    if (state.hasProperty(kWriteSubMode))
        currentSubMode = parseSubMode(state.getProperty(kWriteSubMode).toString(), currentSubMode);

    if (state.hasProperty(kEditorView))
        currentView = parseView(state.getProperty(kEditorView).toString(), currentView);

    if (state.hasProperty(kWriteStepDivision))
        currentStepDivision = juce::jlimit(1, 64, (int)state.getProperty(kWriteStepDivision));

    if (state.hasProperty(kWriteTuplet))
    {
        int t = (int)state.getProperty(kWriteTuplet);
        if (t == 0 || t == 3 || t == 5 || t == 7)
            currentTuplet = t;
    }

    if (state.hasProperty(kWriteSnap))
        snapEnabledFlag = (bool)state.getProperty(kWriteSnap);
}

//==============================================================================

bool WriteController::canWrite(const AuthoringPoint& p) const
{
    return writeModeActive()
        && currentView == EditorView::Notes
        && !isPlaying()
        && currentSubMode == SubMode::Draw
        && p.onHighway
        && (barModeFlag || p.laneIndex >= 0)
        && noteEditorAvailable()
        && instrumentSession != nullptr;
}

void WriteController::stateDidChange()
{
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

// One grid step at the snapped cursor, so hovering shows the block a click would place. Skipped
// while a drag owns the range, which draws its own preview.
void WriteController::recomputePhraseHover()
{
    overlayState.phraseHover = {};
    if (!laneDragActive) overlayState.laneHighlights.clear();

    // The lane tool hovers through the phrase path too, rather than the gem path: hovering with
    // it on showed the gem a click would place, which is not what a click places.
    if (!(phraseViewActive() || laneToolFlag))            return;
    if (phraseDragActive || laneDragActive)               return;
    if (!lastPointValid || !writeModeActive())            return;
    if (isPlaying() || !lastPoint.onHighway)              return;

    double qn = snapQN(lastPoint.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    const auto kind = kindForView();
    const double step = stepSpacingQN(currentStepDivision, currentTuplet);

    // The lane tool hovers the exact lane a click would write, resolved the same way the drag
    // is: a hover is just a gesture whose ends are both the cursor. So what is on screen is
    // always placeable, and a block at the cursor never advertises a lane that cannot be made.
    if (laneToolFlag)
    {
        // One column: a hover has not been pulled anywhere, so it offers the single lane that
        // is the default, and dragging sideways is what widens it from there.
        const auto spread = oneColumn(lastPoint.laneIndex);
        if (auto hover = resolveLaneWrite(lastPoint.rawProjectQN, lastPoint.rawProjectQN, spread))
        {
            overlayState.phraseHover = {
                true, hover->startQN, hover->endQN, OverlayState::PREVIEW_KIND_LANE,
                laneDrawColumns(hover->startQN, hover->endQN, spread) };

            for (const auto& n : laneNotesIn(hover->startQN, hover->endQN, spread))
                overlayState.laneHighlights.push_back({ n.column, n.qn });
        }
    }
    else
    {
        overlayState.phraseHover = { true, qn, qn + step, (int)kind, {} };
    }

    // Borrows the ghost cursor's label, the way the alternating-kick hint does, with no lane so
    // no gem is drawn with it.
    overlayState.ghostVisible   = true;
    overlayState.ghostQN        = qn;
    overlayState.ghostLane      = -1;
    overlayState.ghostModeLabel = laneToolFlag ? "LANE"
                                               : PhraseMath::phraseLabel(kind, activePart());
}

void WriteController::recomputeGhost()
{
    overlayState.ghostVisible    = false;
    overlayState.ghostLane       = -1;
    overlayState.ghostQN         = 0.0;
    overlayState.ghostShowsErase = false;
    overlayState.ghostGem        = GemWrapper();
    overlayState.ghostModeLabel  = {};
    overlayState.stampGhosts.clear();

    // After the clear, since the phrase hover sets the label fields it shares with the ghost.
    recomputePhraseHover();

    if (!lastPointValid)                                  return;
    if (!writeModeActive())                               return;
    if (currentView != EditorView::Notes)                 return;
    if (isPlaying())                                      return;
    if (!lastPoint.onHighway || lastPoint.laneIndex < 0)  return;

    double qn = snapQN(lastPoint.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostQN      = qn;

    // Outside the drag guard below: the mode is still in force while you are
    // painting, so the hint stays up for as long as shift is held.
    if (altKickArmed)
        overlayState.ghostModeLabel = "ALTERNATE KICKS";

    // The lane tool has its own span preview from recomputePhraseHover; a gem ghost on top of it
    // would advertise a note the click is not going to write.
    if (currentSubMode == SubMode::Draw && !laneToolFlag
        && !sustainDragActive && !paintDragActive && !eraseDragActive
        && !stampCaptureActive)
    {
        if (!stamp.empty())
        {
            // The stamp holds the lanes it was copied from. The mouse moves it
            // in time only; left/right arrows are the only way to move it
            // across lanes, via shiftStampLanes.
            for (const auto& sn : stamp)
                overlayState.stampGhosts.push_back({
                    sn.lane, sn.qnOffset, sn.duration,
                    resolveCapturedWrapper(sn.lane, sn.velocity, sn.markerMask, qn + sn.qnOffset) });
        }
        else if (altKickArmed)
        {
            // A shift-drag from here lays an alternating roll that always opens
            // on 1x, so preview the 1x lane rather than whichever side the
            // mouse is on.
            overlayState.ghostLane = DRUM_KICK_COLUMN;
            overlayState.ghostGem  = resolveGhostWrapper(DRUM_KICK_COLUMN, qn);
        }
        else
        {
            overlayState.ghostLane = lastPoint.laneIndex;
            overlayState.ghostGem = resolveGhostWrapper(lastPoint.laneIndex, qn);
        }
    }
}

void WriteController::setStamp(std::vector<StampNote> s)
{
    stamp = std::move(s);
    stateDidChange();
}

void WriteController::clearStamp()
{
    stamp.clear();
    stateDidChange();
}

void WriteController::shiftStampLanes(int delta)
{
    if (stamp.empty()) return;

    // maxLane() rather than a local count: arrows are now the only way to move
    // a stamp sideways, so they have to reach every lane the mouse used to,
    // kick lanes included when kick2x is on.
    for (const auto& sn : stamp)
        if (sn.lane + delta < 0 || sn.lane + delta > maxLane()) return;

    for (auto& sn : stamp)
        sn.lane += delta;

    stateDidChange();
}

void WriteController::beginStampCapture()
{
    if (stampCaptureActive) return;
    if (!lastPointValid || !lastPoint.onHighway || lastPoint.laneIndex < 0) return;

    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    stampCaptureActive   = true;
    stampCaptureTrackIdx = trackIdx;
    stampCaptureRect.begin(lastPoint.rawProjectQN, lastPoint.laneIndex);
    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = false;
    overlayState.marqueeRect    = stampCaptureRect;
    if (onStateChanged) onStateChanged();
}

void WriteController::enterSustainDrag(int trackIdx, double startQN, int lane, RangeTarget target)
{
    sustainDragActive    = true;
    sustainDragTrackIdx  = trackIdx;
    sustainDragStartQN   = startQN;
    sustainDragLane      = lane;
    sustainDragTarget    = std::move(target);
}

void WriteController::clearSustainDrag()
{
    sustainDragActive    = false;
    sustainDragTrackIdx  = -1;
    overlayState.drawPreviewVisible = false;
    overlayState.drawPreviewNotes.clear();
}

//==============================================================================
// Setters

void WriteController::setWriteModeActive(bool active)
{
    if (writeModeActiveFlag == active) return;
    writeModeActiveFlag = active;
    stateDidChange();
}

void WriteController::setSubMode(SubMode mode)
{
    if (currentSubMode == mode) return;
    currentSubMode = mode;
    state.setProperty(kWriteSubMode, subModeToString(mode), nullptr);
    stateDidChange();
}

void WriteController::setView(EditorView v)
{
    if (currentView == v) return;
    currentView = v;
    state.setProperty(kEditorView, viewToString(v), nullptr);
    stateDidChange();
}

void WriteController::setPhraseKind(PhraseMath::Kind k)
{
    if (currentPhraseKind == k) return;
    currentPhraseKind = k;
    stateDidChange();
}

void WriteController::setLaneTool(bool on)
{
    if (laneToolFlag == on) return;
    laneToolFlag = on;
    stateDidChange();
}

void WriteController::setStepDivision(int division)
{
    int clamped = juce::jlimit(1, 64, division);
    if (currentStepDivision == clamped) return;
    currentStepDivision = clamped;
    state.setProperty(kWriteStepDivision, clamped, nullptr);
    stateDidChange();
}

void WriteController::setTuplet(int t)
{
    if (t != 0 && t != 3 && t != 5 && t != 7) return;
    if (currentTuplet == t) return;
    currentTuplet = t;
    state.setProperty(kWriteTuplet, t, nullptr);
    stateDidChange();
}

void WriteController::toggleTuplet()
{
    if (currentTuplet != 0)
    {
        lastTuplet = currentTuplet;
        setTuplet(0);
        return;
    }
    setTuplet(lastTuplet);
}

void WriteController::cycleTuplet()
{
    // Always lands on a live tuplet, so this doubles as "turn it on" when off.
    switch (currentTuplet)
    {
        case 3:  lastTuplet = 5; break;
        case 5:  lastTuplet = 7; break;
        default: lastTuplet = 3; break;
    }
    setTuplet(lastTuplet);
}

void WriteController::setSnapEnabled(bool enabled)
{
    if (snapEnabledFlag == enabled) return;
    snapEnabledFlag = enabled;
    state.setProperty(kWriteSnap, enabled, nullptr);
    stateDidChange();
}

//==============================================================================
// Input dispatch

void WriteController::onPointerMove(const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    lastPoint = p;
    lastPointValid = true;

    if (stampCaptureActive && p.onHighway && p.laneIndex >= 0)
    {
        stampCaptureRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());
        overlayState.marqueeRect = stampCaptureRect;
        if (onStateChanged) onStateChanged();
    }

    recomputeGhost();
}

//==============================================================================
// Phrases (SP and Phrase views)

bool WriteController::phraseViewActive() const
{
    return currentView == EditorView::StarPower || currentView == EditorView::Phrase;
}

// Whichever kind the picker has armed. The SP view predates the picker and forces its own.
PhraseMath::Kind WriteController::kindForView() const
{
    return currentView == EditorView::StarPower ? PhraseMath::Kind::StarPower : currentPhraseKind;
}

std::optional<double> WriteController::codaQN() const
{
    if (instrumentSession == nullptr) return std::nullopt;
    const auto& sections = instrumentSession->getSongSections();
    if (!sections.hasCoda()) return std::nullopt;
    return sections.getCodaPPQ().toDouble();
}

std::vector<PhraseMath::PhraseSpan> WriteController::phrasesOfKind(PhraseMath::Kind kind) const
{
    if (instrumentSession == nullptr) return {};
    int idx = resolveSessionTrackIdx();
    if (idx < 0) return {};

    auto kindPitches = PhraseMath::pitches(kind, activePart());
    if (kindPitches.empty()) return {};

    auto spans = PhraseMath::rangesOf(instrumentSession->getNotes(idx), kindPitches.front());

    // Fill and BRE are both written on 120, so each has to drop the other's spans or a click on
    // a drum fill reads as a hit on the BRE and starts resizing it instead of writing one.
    if (kind == PhraseMath::Kind::Fill || kind == PhraseMath::Kind::Bre)
    {
        const bool wantBre = kind == PhraseMath::Kind::Bre;
        const auto coda = codaQN();
        spans.erase(std::remove_if(spans.begin(), spans.end(),
                                   [wantBre, coda](const PhraseMath::PhraseSpan& s) {
                                       return PhraseMath::isBre(s.startQN, coda) != wantBre;
                                   }),
                    spans.end());
    }

    return spans;
}

std::vector<PhraseMath::PhraseSpan> WriteController::phrasesOnPart() const
{
    return phrasesOfKind(kindForView());
}

void WriteController::handlePhraseDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    auto* writer = midiWriter();
    if (writer == nullptr || instrumentSession == nullptr) return;

    const double clickQN = snapQN(p.rawProjectQN);
    const auto spans = phrasesOnPart();
    const int hit = PhraseMath::phraseAt(spans, p.rawProjectQN);

    PhraseWriter phraseWriter(*writer, *instrumentSession, activePart());

    // Right on a phrase removes it; right anywhere else does nothing, so a stray right click
    // in empty time cannot wipe the phrase you were aiming at.
    if (ctx.rightButton)
    {
        if (hit >= 0)
        {
            phraseWriter.erase(kindForView(), spans[(size_t)hit].startQN);
            if (onChartWritten) onChartWritten();
        }
        return;
    }

    phraseDragActive = true;

    if (hit >= 0)
    {
        // On a phrase, the drag moves its end, the way a sustain's tail moves.
        phraseDragResizing = true;
        phraseDragStartQN = spans[(size_t)hit].startQN;
        phraseDragEndQN = spans[(size_t)hit].endQN;
    }
    else
    {
        phraseDragResizing = false;
        phraseDragStartQN = clickQN;
        phraseDragEndQN = clickQN;
    }
    recomputePhrasePreview();
}

void WriteController::handlePhraseDrag(const AuthoringPoint& p)
{
    if (!phraseDragActive) return;

    // Not held short of the neighbours any more: a drag run into one merges with it on release.
    phraseDragEndQN = snapQN(p.rawProjectQN);
    recomputePhrasePreview();
}

void WriteController::handlePhraseUp()
{
    if (!phraseDragActive) return;

    auto* writer = midiWriter();
    const double startQN = phraseDragStartQN;
    const double endQN = phraseDragEndQN;
    const bool resizing = phraseDragResizing;
    clearPhraseDrag();

    if (writer == nullptr || instrumentSession == nullptr) return;
    if (!GridMath::atLeast(endQN - startQN, double(MIDI_MIN_SUSTAIN_LENGTH))) return;

    PhraseWriter phraseWriter(*writer, *instrumentSession, activePart());
    if (resizing)
    {
        auto resized = phraseWriter.resize(kindForView(), startQN, endQN);
        if (resized.isNotEmpty() && onPhraseRefused) onPhraseRefused(resized);
        if (onStateChanged) onStateChanged();
        if (onChartWritten) onChartWritten();
        return;
    }

    // Either a refusal or a note that a BRE elsewhere in the song was replaced; both belong in
    // the footer, since both are things that happened without being asked for.
    auto outcome = phraseWriter.create(kindForView(), startQN, endQN);
    if (outcome.isNotEmpty() && onPhraseRefused) onPhraseRefused(outcome);

    if (onStateChanged) onStateChanged();
    if (onChartWritten) onChartWritten();
}

void WriteController::clearPhraseDrag()
{
    phraseDragActive = false;
    phraseDragResizing = false;
    overlayState.phrasePreview = {};
    recomputePhraseHover();
    if (onStateChanged) onStateChanged();
}

void WriteController::recomputePhrasePreview()
{
    overlayState.phrasePreview = { phraseDragActive, phraseDragStartQN, phraseDragEndQN,
                                   laneToolFlag ? OverlayState::PREVIEW_KIND_LANE
                                                : (int)kindForView() };
    // Suppresses itself while the drag owns the range, so the two never draw at once.
    recomputePhraseHover();
    if (onStateChanged) onStateChanged();
}

//==============================================================================
// Lanes (roll / trill markers, the lane tool inside the Notes view)
//
// LaneMath.h holds the rules these follow and the spec lines they come from. The short of
// it: elite carries a lane's column in its pitch, everyone else shares one pair of pitches
// whose columns the game reads off the notes underneath.

std::vector<WriteController::LaneNote> WriteController::laneCandidateNotes(LaneSpread spread) const
{
    const auto* cfg = authoring();
    if (cfg == nullptr || instrumentSession == nullptr) return {};

    int idx = resolveSessionTrackIdx();
    if (idx < 0) return {};

    // Pitch -> column for this instrument and skill, so the session's own notes can be read
    // without a second copy of the per-instrument mapping.
    std::map<int, int> columnOf;
    for (int lane = 0; lane <= maxLane(); ++lane)
    {
        // Elite's lanes belong to the columns the drag reached, kicks included: it has a kick
        // roll lane of its own. The shared markers take the whole board but leave kicks out,
        // because the game leaves them out when working out where a marker lands.
        if (cfg->lanesPerColumn && !spread.covers(lane)) continue;
        int pitch = resolvePitch(lane);
        if (pitch < 0) continue;
        if (!cfg->lanesPerColumn && cfg->isKickPitch((uint)pitch, currentActiveSkill)) continue;
        columnOf.emplace(pitch, lane);
    }

    std::vector<LaneNote> notes;
    for (const auto& n : instrumentSession->getNotes(idx))
    {
        auto it = columnOf.find((int)n.pitch);
        if (it != columnOf.end()) notes.push_back({ n.startPPQ.toDouble(), it->second });
    }
    std::sort(notes.begin(), notes.end(),
              [](const LaneNote& a, const LaneNote& b) { return a.qn < b.qn; });
    return notes;
}

std::vector<int> WriteController::laneColumnsUnder(double startQN, double endQN,
                                                   LaneSpread spread) const
{
    // First-struck order, because a 1- or 2-lane marker reaches the first one or two columns
    // played under it, not the lowest-numbered ones. TrackResolver::resolveLanes reads them
    // back the same way.
    std::vector<int> columns;
    for (const auto& n : laneCandidateNotes(spread))
    {
        if (!GridMath::within(n.qn, startQN, endQN)) continue;
        if (std::find(columns.begin(), columns.end(), n.column) == columns.end())
            columns.push_back(n.column);
    }
    return columns;
}

std::vector<int> WriteController::laneDrawColumns(double startQN, double endQN,
                                                  LaneSpread spread) const
{
    const auto* cfg = authoring();
    if (cfg == nullptr || !spread.valid()) return {};

    // Elite's lanes carry their own columns, so a drag pulled sideways draws one per column it
    // reached. Each still needs notes of its own to be a roll over.
    if (cfg->lanesPerColumn)
    {
        std::vector<int> columns;
        for (int c = spread.lo; c <= spread.hi; ++c)
            if (!laneNotesIn(startQN, endQN, oneColumn(c)).empty()) columns.push_back(c);
        return columns;
    }

    auto columns = laneColumnsUnder(startQN, endQN, spread);

    // A marker only reaches as far as the drag asked for: a 1-lane roll draws on the first
    // column struck even where three are struck under it.
    const size_t reach = LaneMath::reachOf(LaneMath::targetForDragSpan(spread.count()));
    if (columns.size() > reach) columns.resize(reach);

    // Nothing underneath draws nothing: the marker has nowhere to land, and a preview in the
    // dragged column would advertise a lane that cannot be written there.
    return columns;
}

std::vector<int> WriteController::laneWritePitches(double startQN, double endQN,
                                                   LaneSpread spread) const
{
    const auto* cfg = authoring();
    if (cfg == nullptr || cfg->phrasePitch == nullptr || !spread.valid()) return {};

    // Elite: one roll-lane pitch per column the drag reached, which is how it carries the
    // arbitrary combinations of lanes its spec allows. Only columns with notes under them.
    if (cfg->lanesPerColumn)
    {
        std::vector<int> pitches;
        for (int c : laneDrawColumns(startQN, endQN, spread))
        {
            int pitch = cfg->phrasePitch(PhraseTarget::Lane1, currentActiveSkill, c);
            if (pitch >= 0) pitches.push_back(pitch);
        }
        return pitches;
    }

    // Shared: one lane until the drag was pulled sideways. The marker has no column of its own,
    // so the spread says only how many columns it should reach once the game places it.
    auto target = LaneMath::targetForDragSpan(spread.count());
    int pitch = cfg->phrasePitch(target, currentActiveSkill, 0);
    return pitch >= 0 ? std::vector<int>{ pitch } : std::vector<int>{};
}

// The note nearest a point in time, in either direction. Snapping only forwards had a cursor
// sitting just past a note jump to the next one along however far away that was.
std::optional<WriteController::LaneNote> WriteController::nearestLaneNote(double rawQN,
                                                                         LaneSpread spread) const
{
    std::optional<LaneNote> best;
    double bestDistance = 0.0;
    for (const auto& n : laneCandidateNotes(spread))
    {
        const double distance = std::abs(n.qn - rawQN);
        if (!best.has_value() || distance < bestDistance) { best = n; bestDistance = distance; }
    }
    return best;
}

// A lane needs two notes to have any length. Given one, reach to its neighbour so a click
// always has something to place: forwards by preference, since a lane usually opens on the note
// under the cursor and runs on from it, and backwards at the end of a track where there is
// nothing ahead to reach.
std::optional<PhraseMath::PhraseSpan> WriteController::laneAround(double qn,
                                                                  LaneSpread spread) const
{
    const auto notes = laneCandidateNotes(spread);

    for (const auto& n : notes)
        if (GridMath::after(n.qn, qn)) return PhraseMath::PhraseSpan{ qn, n.qn };

    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
        if (GridMath::before(it->qn, qn)) return PhraseMath::PhraseSpan{ it->qn, qn };

    return std::nullopt;
}

// The notes a lane over this range captures, for the highlight that shows what it will apply to.
std::vector<WriteController::LaneNote> WriteController::laneNotesIn(double startQN, double endQN,
                                                                   LaneSpread spread) const
{
    std::vector<LaneNote> hit;
    for (const auto& n : laneCandidateNotes(spread))
        if (GridMath::within(n.qn, startQN, endQN)) hit.push_back(n);
    return hit;
}

// The lane a gesture would commit: each end snapped to the note nearest it, then merged with
// every lane it runs into. Hover, drag preview and commit all come through here, a hover being
// nothing more than a gesture whose two ends are the same point. So what is drawn is always
// exactly what lands, and anything visible can be placed with a click.
std::optional<PhraseMath::PhraseSpan> WriteController::resolveLaneWrite(double rawStartQN,
                                                                       double rawEndQN,
                                                                       LaneSpread spread) const
{
    const auto first = nearestLaneNote(std::min(rawStartQN, rawEndQN), spread);
    const auto last  = nearestLaneNote(std::max(rawStartQN, rawEndQN), spread);
    if (!first.has_value() || !last.has_value()) return std::nullopt;

    PhraseMath::PhraseSpan span { std::min(first->qn, last->qn), std::max(first->qn, last->qn) };

    // A click, or a drag whose ends both snapped to the same note. Reaching to the neighbour is
    // what makes a click place the lane it was showing rather than nothing at all.
    if (!GridMath::after(span.endQN, span.startQN))
    {
        const auto grown = laneAround(span.startQN, spread);
        if (!grown.has_value()) return std::nullopt;
        span = *grown;
    }

    const auto merged = PhraseMath::mergeSpan(span, laneSpansOnPart(spread));
    if (!LaneMath::isWritableLength(merged.startQN, merged.endQN)) return std::nullopt;
    return merged;
}

std::vector<int> WriteController::laneCandidatePitches(LaneSpread spread) const
{
    const auto* cfg = authoring();
    if (cfg == nullptr || cfg->phrasePitch == nullptr) return {};

    // On elite a lane belongs to a column, so only the columns reached are candidates. On the
    // shared markers there is no column to narrow by, so both pitches are.
    if (cfg->lanesPerColumn)
    {
        std::vector<int> pitches;
        if (!spread.valid()) return pitches;
        for (int c = spread.lo; c <= spread.hi; ++c)
        {
            int pitch = cfg->phrasePitch(PhraseTarget::Lane1, currentActiveSkill, c);
            if (pitch >= 0) pitches.push_back(pitch);
        }
        return pitches;
    }

    std::vector<int> pitches;
    for (auto target : { PhraseTarget::Lane1, PhraseTarget::Lane2 })
    {
        int pitch = cfg->phrasePitch(target, currentActiveSkill, 0);
        if (pitch >= 0) pitches.push_back(pitch);
    }
    return pitches;
}

// Drawn free of the grid and reconciled on release, so the range you see while dragging is the
// one the cursor is describing rather than the one the last grid line allows. Nothing is
// written on the way down: a lane is a range, and a zero-length one is not a thing.
void WriteController::handleBeginLane(const AuthoringPoint& p, int trackIdx)
{
    laneDragActive     = true;
    laneDragTrackIdx   = trackIdx;
    laneDragColumnLo   = p.laneIndex;
    laneDragColumnHi   = p.laneIndex;
    laneDragRawStartQN = p.rawProjectQN;
    laneDragRawEndQN   = p.rawProjectQN;
    recomputeLanePreview();
}

void WriteController::handleUpdateLane(const AuthoringPoint& p)
{
    if (!laneDragActive) return;
    laneDragRawEndQN = p.rawProjectQN;

    // Widest sideways reach of the whole drag, not wherever the cursor sits now: pulling out to
    // a second column and drifting back should not quietly drop the lane you just asked for.
    if (p.laneIndex >= 0)
    {
        if (laneDragColumnLo < 0) { laneDragColumnLo = laneDragColumnHi = p.laneIndex; }
        laneDragColumnLo = std::min(laneDragColumnLo, p.laneIndex);
        laneDragColumnHi = std::max(laneDragColumnHi, p.laneIndex);
    }

    recomputeLanePreview();
}

void WriteController::handleCommitLane()
{
    if (!laneDragActive) return;

    // The same resolve the preview drew from, so what lands is the shape that was on screen.
    const auto spread  = laneDragSpread();
    const auto merged  = resolveLaneWrite(laneDragRawStartQN, laneDragRawEndQN, spread);
    const int trackIdx = laneDragTrackIdx;

    clearLaneDrag();

    if (trackIdx < 0) return;

    // A lane is a marker over a run of notes, so with no run under the drag there is nothing to
    // write. Said out loud rather than silently dropped: a lane that just fails to appear reads
    // as a broken tool.
    if (!merged.has_value())
    {
        if (onPhraseRefused) onPhraseRefused("A lane needs notes to sit on");
        return;
    }

    // Lanes merge with the ones they run into, so a drag that extends an existing lane is the
    // whole union, which resolveLaneWrite already folded in. The absorbed spans go before the
    // merged one is written in their place. On the shared markers both pitches count as the
    // same lane: they are variants of it, and leaving a tremolo stacked under a trill is never
    // what redrawing over one meant.
    const auto absorbed = PhraseMath::spansWithin(*merged, laneSpansOnPart(spread));

    // One lane unless the drag was pulled sideways, which is what asks for a trill on the shared
    // markers and for a lane per pad on elite.
    auto pitches = laneWritePitches(merged->startQN, merged->endQN, spread);
    if (pitches.empty()) return;

    beginBatch("Chartchotic: Lane");

    for (const auto& span : absorbed)
        for (int pitch : laneCandidatePitches(spread))
        {
            auto note = findNote(trackIdx, span.startQN, pitch);
            if (note.noteIndex >= 0 && GridMath::same(note.startQN, span.startQN))
                eraseMarkerNote(trackIdx, span.startQN, pitch, -1);
        }

    for (int pitch : pitches)
        createPhraseNote(trackIdx, merged->startQN, merged->endQN - merged->startQN, pitch);
    endBatch();

    if (onStateChanged) onStateChanged();
    if (onChartWritten) onChartWritten();
}

void WriteController::clearLaneDrag()
{
    laneDragActive   = false;
    laneDragTrackIdx = -1;
    laneDragColumnLo = -1;
    laneDragColumnHi = -1;
    overlayState.phrasePreview = {};
    recomputePhraseHover();
    if (onStateChanged) onStateChanged();
}

void WriteController::recomputeLanePreview()
{
    // Snapped as the drag moves rather than only on release. Edges land on note starts, so a
    // raw range would draw a shape that is never what gets written; this way the only thing
    // ever on screen is a lane that could really land, merge with its neighbours and all. A
    // drag that covers no run of notes draws nothing, which is exactly what it would write.
    const auto spread = laneDragSpread();
    const auto lane = laneDragActive ? resolveLaneWrite(laneDragRawStartQN, laneDragRawEndQN,
                                                        spread)
                                     : std::nullopt;

    overlayState.phrasePreview = {
        lane.has_value(),
        lane ? lane->startQN : 0.0,
        lane ? lane->endQN : 0.0,
        OverlayState::PREVIEW_KIND_LANE,
        lane ? laneDrawColumns(lane->startQN, lane->endQN, spread) : std::vector<int>{} };

    overlayState.laneHighlights.clear();
    if (lane)
        for (const auto& n : laneNotesIn(lane->startQN, lane->endQN, spread))
            overlayState.laneHighlights.push_back({ n.column, n.qn });

    // Suppresses itself while the drag owns the range, so the two never draw at once.
    recomputePhraseHover();
    if (onStateChanged) onStateChanged();
}

// Every lane already on the part that a new one in this column could merge with.
std::vector<PhraseMath::PhraseSpan> WriteController::laneSpansOnPart(LaneSpread spread) const
{
    if (instrumentSession == nullptr) return {};
    int idx = resolveSessionTrackIdx();
    if (idx < 0) return {};

    std::vector<PhraseMath::PhraseSpan> spans;
    for (int pitch : laneCandidatePitches(spread))
        for (const auto& s : PhraseMath::rangesOf(instrumentSession->getNotes(idx), pitch))
            spans.push_back(s);

    std::sort(spans.begin(), spans.end(),
              [](const PhraseMath::PhraseSpan& a, const PhraseMath::PhraseSpan& b) {
                  return a.startQN < b.startQN; });
    return spans;
}

void WriteController::eraseLaneAt(const AuthoringPoint& p, int trackIdx)
{
    if (instrumentSession == nullptr) return;
    int idx = resolveSessionTrackIdx();
    if (idx < 0) return;

    const auto& notes = instrumentSession->getNotes(idx);
    bool erased = false;

    for (int pitch : laneCandidatePitches(oneColumn(p.laneIndex)))
    {
        auto spans = PhraseMath::rangesOf(notes, pitch);
        int hit = PhraseMath::phraseAt(spans, p.rawProjectQN);
        if (hit < 0) continue;

        if (!erased) { beginBatch("Chartchotic: Erase lane"); erased = true; }
        eraseMarkerNote(trackIdx, spans[(size_t)hit].startQN, pitch, -1);
    }

    if (!erased) return;
    endBatch();
    if (onStateChanged) onStateChanged();
    if (onChartWritten) onChartWritten();
}

//==============================================================================

void WriteController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (phraseViewActive())
    {
        if (writeModeActive() && !isPlaying() && p.onHighway) handlePhraseDown(p, ctx);
        return;
    }

    if (!canWrite(p)) return;

    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    auto cmd = commandMapper.resolve(currentSubMode, EventType::Down, ctx);

    // The lane tool writes a range on the track rather than a note in a lane, so it takes the
    // gesture before anything resolves a pitch to place. Right removes the lane under the
    // cursor; a right drag does nothing further, since there is no sweep to make.
    if (laneToolFlag)
    {
        if      (cmd == WriteCommand::BeginSustain) handleBeginLane(p, trackIdx);
        else if (cmd == WriteCommand::BeginErase)   eraseLaneAt(p, trackIdx);
        return;
    }

    bool drums = isDrums();
    int  pitch = resolveActivePitch(p.laneIndex);
    if (pitch < 0) return;

    switch (cmd)
    {
        case WriteCommand::BeginSustain: handleBeginSustain(p, trackIdx, pitch, drums); break;
        case WriteCommand::BeginPaint:   handleBeginPaint(p, trackIdx, pitch, drums);   break;
        case WriteCommand::BeginErase:   handleBeginErase(p, trackIdx, pitch, drums);   break;
        default: break;
    }
}

void WriteController::onPointerDrag(const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    sustainPendingClick = false;
    if (phraseDragActive)    { handlePhraseDrag(p);     return; }
    if (laneDragActive)      { handleUpdateLane(p);     return; }
    if (sustainDragActive)   { handleUpdateSustain(p);  return; }
    if (paintDragActive)     { handleContinuePaint(p);  return; }
    if (eraseDragActive)     { handleContinueErase(p);  return; }
}

void WriteController::onPointerUp(const AuthoringPoint& p,
                                  [[maybe_unused]] const AuthoringContext& ctx)
{
    if (phraseDragActive) { handlePhraseUp(); return; }
    if (laneDragActive)   { handleCommitLane(); return; }

    if (sustainPendingClick)
    {
        sustainPendingClick = false;
        // Replacing a note's type must not destroy its sustain, so carry the
        // existing duration across when we land on the same note.
        auto existing = findNote(sustainDragTrackIdx, sustainPendingClickQN, sustainDragTarget.single());
        double duration = (existing.noteIndex >= 0
                           && GridMath::same(existing.startQN, sustainPendingClickQN))
                        ? existing.endQN - existing.startQN : 0.0;
        placeNote(sustainDragTrackIdx, sustainPendingClickQN,
                  sustainDragTarget.single(), sustainDragLane, resolveVelocity(), duration);
        endBatch();
        clearSustainDrag();
        recomputeGhost();
        return;
    }
    if (sustainDragActive)   { handleCommitSustain(p);  return; }
    if (paintDragActive)     { handleCommitPaint();     return; }
    if (eraseDragActive)     { handleEndErase();        return; }
}

bool WriteController::altKickAvailable() const
{
    return writeModeActive()
        && currentSubMode == SubMode::Draw
        && barModeFlag
        && isDrums()
        && kick2xEnabled;
}

void WriteController::onFrameTick([[maybe_unused]] double currentProjectQN,
                                  [[maybe_unused]] bool isPlaying)
{
    bool armed = altKickAvailable()
              && juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    if (armed != altKickArmed)
    {
        altKickArmed = armed;
        recomputeGhost();
    }

    if (stampCaptureActive && !juce::KeyPress::isKeyCurrentlyDown('C'))
    {
        stampCaptureActive = false;
        overlayState.marqueeVisible = false;

        auto classified = classifyNotesInRect(stampCaptureTrackIdx, stampCaptureRect);
        double minQN = std::numeric_limits<double>::max();
        for (const auto& cn : classified)
            if (!cn.sustainOnly && cn.note.startQN < minQN) minQN = cn.note.startQN;

        std::vector<StampNote> notes;
        for (const auto& cn : classified)
        {
            if (cn.sustainOnly) continue;
            uint32_t mask = captureMarkerMask(stampCaptureTrackIdx, cn.note.startQN, cn.lane);
            notes.push_back({ cn.lane, cn.note.startQN - minQN,
                              cn.note.endQN - cn.note.startQN,
                              cn.note.velocity, mask });
        }
        if (notes.size() >= 2)
            setStamp(std::move(notes));

        if (onStateChanged) onStateChanged();
    }

    recomputeGhost();
}

//==============================================================================
// Sustain command handlers

void WriteController::handleBeginSustain(const AuthoringPoint& p, int trackIdx, int pitch, bool drums)
{
    double clickQN = snapQN(p.rawProjectQN);

    if (!stamp.empty())
    {
        beginBatch("Chartchotic: Stamp notes");
        for (const auto& sn : stamp)
        {
            int lane = sn.lane;
            int sp = resolvePitch(lane);
            if (sp >= 0)
            {
                // Velocity and markers come from the captured note, not the
                // current toolbar state, so a paste round-trips drum dynamics
                // and note type exactly as copied. One path for both
                // instruments: the mask says which markers to reproduce.
                placeStampNote(trackIdx, clickQN + sn.qnOffset, sp, lane,
                               sn.velocity, sn.duration, sn.markerMask);
            }
        }
        if (drums) { endBatch(); return; }
        enterSustainDrag(trackIdx, clickQN, p.laneIndex, {{pitch}, true});
        return;
    }

    bool onExistingNote = p.overExistingNote
        && findNote(trackIdx, p.hitNoteStartQN, pitch).noteIndex >= 0;

    if (drums)
    {
        // No guard on an existing note: clicking one re-applies the current
        // type, so dropping an accent on a normal note upgrades it instead of
        // silently doing nothing. createNote erases and recreates at the same
        // QN, which makes this a replace. Targeting clickQN rather than the
        // hit note also means clicking further along a sustain body creates a
        // new note at that step, which previously fell through and did nothing.
        double duration = 0.0;
        if (onExistingNote && GridMath::same(p.hitNoteStartQN, clickQN))
        {
            auto found = findNote(trackIdx, p.hitNoteStartQN, pitch);
            if (found.noteIndex >= 0) duration = found.endQN - found.startQN;
        }
        placeNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity(), duration);
        return;
    }

    beginBatch("Chartchotic: Sustain note");

    if (!onExistingNote)
    {
        placeNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity());
        enterSustainDrag(trackIdx, clickQN, p.laneIndex, {{pitch}, true});
        return;
    }

    enterSustainDrag(trackIdx, p.hitNoteStartQN, p.laneIndex, {{pitch}, true});
    sustainPendingClick = true;
    sustainPendingClickQN = clickQN;
}

void WriteController::handleUpdateSustain(const AuthoringPoint& p)
{
    double dragQN  = snapQN(p.rawProjectQN);

    if (!GridMath::atLeast(dragQN - sustainDragStartQN, double(MIDI_MIN_SUSTAIN_LENGTH)))
    {
        overlayState.drawPreviewVisible = false;
        overlayState.drawPreviewNotes.clear();
        return;
    }

    overlayState.drawPreviewVisible = true;
    overlayState.drawPreviewNotes.clear();
    if (!stamp.empty())
    {
        bool drums = isDrums();
        for (const auto& sn : stamp)
        {
            int lane = sn.lane;
            overlayState.drawPreviewNotes.push_back({
                lane, sustainDragStartQN + sn.qnOffset, dragQN, resolvePitch(lane)
            });
        }
    }
    else
    {
        overlayState.drawPreviewNotes.push_back({
            sustainDragLane, sustainDragStartQN, dragQN, sustainDragTarget.single()
        });
    }
}

void WriteController::handleCommitSustain(const AuthoringPoint& p)
{
    double endQN = snapQN(p.rawProjectQN);

    if (GridMath::atLeast(endQN - sustainDragStartQN, double(MIDI_MIN_SUSTAIN_LENGTH)))
    {
        if (!stamp.empty())
        {
            bool drums = isDrums();
            for (const auto& sn : stamp)
            {
                int lane = sn.lane;
                int sp = resolvePitch(lane);
                if (sp >= 0)
                    chainExtendNotes(sustainDragTrackIdx,
                                     sustainDragStartQN + sn.qnOffset,
                                     endQN, sp, lane);
            }
        }
        else
        {
            for (int pitch : sustainDragTarget.pitches)
                chainExtendNotes(sustainDragTrackIdx, sustainDragStartQN, endQN, pitch,
                                 sustainDragLane);
        }
    }

    endBatch();
    clearSustainDrag();
    recomputeGhost();
}

//==============================================================================
// Paint command handlers

void WriteController::handleBeginPaint(const AuthoringPoint& p, int trackIdx,
                                       [[maybe_unused]] int pitch, [[maybe_unused]] bool drums)
{
    double qn = snapQN(p.rawProjectQN);

    paintDragActive   = true;
    paintDragTrackIdx = trackIdx;
    paintStartQN      = qn;
    paintLastQN        = qn;
    paintLastLane      = p.laneIndex;
    paintedNotes.clear();
    beginBatch("Chartchotic: Paint notes");

    paintFillRange(qn, qn, p.laneIndex);
}

void WriteController::handleContinuePaint(const AuthoringPoint& p)
{
    if (!p.onHighway || p.laneIndex < 0) return;

    double cursorQN = snapQN(p.rawProjectQN);
    int lane = paintLastLane;

    if (p.laneIndex != paintLastLane)
    {
        paintShrinkTo(-1.0, -1.0);
        paintLastQN   = cursorQN;
        paintLastLane = p.laneIndex;
        return;
    }

    double lo = std::min(paintStartQN, cursorQN);
    double hi = std::max(paintStartQN, cursorQN);

    paintShrinkTo(lo, hi);
    paintFillRange(lo, hi, lane);

    paintLastQN   = cursorQN;
    paintLastLane = lane;
}

void WriteController::handleCommitPaint()
{
    endBatch();
    paintDragActive   = false;
    paintDragTrackIdx = -1;
    paintedNotes.clear();
    recomputeGhost();
}

void WriteController::paintFillRange(double fromQN, double toQN, int lane)
{
    bool drums = isDrums();

    // Paint is only ever reached by shift-dragging, so kick mode needs no
    // extra modifier to mean "alternate": being here is the request.
    bool alternatingKicks = drums && barModeFlag && kick2xEnabled;

    double spacing = stepSpacingQN(currentStepDivision, currentTuplet);
    if (spacing <= 0.0) return;

    double lo = std::min(fromQN, toQN);
    double hi = std::max(fromQN, toQN);

    double firstStep = snapToStep(lo, currentStepDivision, currentTuplet);
    if (GridMath::before(firstStep, lo)) firstStep += spacing;

    for (double qn = firstStep; !GridMath::after(qn, hi); qn += spacing)
    {
        double snapped = snapToStep(qn, currentStepDivision, currentTuplet);

        bool alreadyPainted = false;
        for (const auto& pn : paintedNotes)
            if (GridMath::same(pn.qn, snapped)) { alreadyPainted = true; break; }
        if (alreadyPainted) continue;

        if (!stamp.empty())
        {
            for (const auto& sn : stamp)
            {
                int lane = sn.lane;
                int sp = resolvePitch(lane);
                if (sp >= 0)
                {
                    // Painting a stamp is still a paste, so it carries the
                    // captured velocity and markers like the click path does.
                    placeStampNote(paintDragTrackIdx, snapped + sn.qnOffset, sp, lane,
                                   sn.velocity, 0.0, sn.markerMask);
                }
            }
        }
        else
        {
            // Painting kicks lays down an alternating 1x/2x roll rather than a
            // run of one pitch, so a 1x-only player still gets a playable
            // half-speed version of the pattern. Parity is anchored to
            // paintStartQN so it stays put when the drag reverses or refills,
            // and the run always opens on 1x.
            int paintLane = lane;
            if (alternatingKicks)
            {
                long long step = std::llround((snapped - paintStartQN) / spacing);
                bool offbeat = ((step % 2) + 2) % 2 == 1;
                // The 2x column is per-instrument (4-lane 6, elite 9). Hardcoding 6 painted
                // elite's alternating kicks into Tom 3.
                const auto* cfg = authoring();
                paintLane = (offbeat && cfg && cfg->kick2xColumn >= 0)
                    ? cfg->kick2xColumn : DRUM_KICK_COLUMN;
            }

            int pitch = alternatingKicks ? resolvePitch(paintLane)
                                         : resolveActivePitch(lane);
            if (pitch < 0) continue;
            auto pre = findNote(paintDragTrackIdx, snapped, pitch);
            if (pre.noteIndex >= 0 && GridMath::same(pre.startQN, snapped))
                continue;
            placeNote(paintDragTrackIdx, snapped, pitch, paintLane, resolveVelocity());
            paintedNotes.push_back({ snapped, paintLane });
            continue;
        }
        paintedNotes.push_back({ snapped, lane });
    }
}

void WriteController::paintShrinkTo(double lo, double hi)
{
    bool drums = isDrums();
    for (auto it = paintedNotes.begin(); it != paintedNotes.end(); )
    {
        if (GridMath::before(it->qn, lo) || GridMath::after(it->qn, hi))
        {
            if (!stamp.empty())
            {
                for (const auto& sn : stamp)
                {
                    int lane = sn.lane;
                    int sp = resolvePitch(lane);
                    if (sp >= 0)
                        eraseNote(paintDragTrackIdx, it->qn + sn.qnOffset, sp, lane, currentActiveSkill);
                }
            }
            else
            {
                int oldPitch = resolveActivePitch(it->lane);
                if (oldPitch >= 0)
                    eraseNote(paintDragTrackIdx, it->qn, oldPitch, it->lane, currentActiveSkill);
            }
            it = paintedNotes.erase(it);
        }
        else
            ++it;
    }
}

//==============================================================================
// Erase command handlers

void WriteController::handleBeginErase(const AuthoringPoint& p, int trackIdx,
                                       [[maybe_unused]] int pitch, [[maybe_unused]] bool drums)
{
    if (!stamp.empty()) clearStamp();
    eraseDragActive   = true;
    eraseDragTrackIdx = trackIdx;
    eraseRect.begin(p.rawProjectQN, p.laneIndex);
    eraseClickedNoteQN = (p.overExistingNote && !p.hitSustainBody) ? p.hitNoteStartQN : -1.0;
    eraseClickedSustainQN   = (p.overExistingNote && p.hitSustainBody) ? p.hitNoteStartQN : -1.0;
    eraseClickedSustainLane = (p.overExistingNote && p.hitSustainBody) ? p.laneIndex : -1;

    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = true;
    overlayState.marqueeRect    = eraseRect;
    overlayState.eraseClickedNoteQN     = eraseClickedNoteQN;
    overlayState.eraseClickedLane       = eraseClickedNoteQN >= 0.0 ? p.laneIndex : -1;
    overlayState.eraseClickedSustainQN  = eraseClickedSustainQN;
    overlayState.eraseClickedSustainLane = eraseClickedSustainLane;
    if (onStateChanged) onStateChanged();
}

void WriteController::handleContinueErase(const AuthoringPoint& p)
{
    if (!p.onHighway || p.laneIndex < 0) return;

    eraseRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());

    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = true;
    overlayState.marqueeRect    = eraseRect;
    if (onStateChanged) onStateChanged();
}

void WriteController::handleEndErase()
{
    bool drums = isDrums();
    beginBatch("Chartchotic: Erase notes");
    for (const auto& cn : classifyNotesInRect(eraseDragTrackIdx, eraseRect))
    {
        if (cn.sustainOnly)
        {
            // A predecessor trimmed to butt against the note being clicked ends
            // exactly where the click is, so the raw cursor QN can land a hair
            // inside it. Erasing a head is not a reason to shorten its
            // neighbour.
            if (eraseClickedNoteQN >= 0.0
                && GridMath::same(cn.note.endQN, eraseClickedNoteQN))
                continue;
            truncateNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch, cn.lane);
        }
        else
            eraseNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch,
                      cn.lane, currentActiveSkill);
    }
    if (eraseClickedNoteQN >= 0.0)
    {
        if (barModeFlag && drums)
        {
            const auto* cfg = authoring();
            const int kick2xLane = (cfg && cfg->kick2xColumn >= 0) ? cfg->kick2xColumn
                                                                   : DRUM_KICK_COLUMN;
            for (int tryLane : {DRUM_KICK_COLUMN, kick2xLane})
            {
                int tryPitch = resolveBarPitch(tryLane);
                if (tryPitch >= 0 && findNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch).noteIndex >= 0)
                {
                    eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch, tryLane, currentActiveSkill);
                    break;
                }
            }
        }
        else
        {
            int pitch = resolveActivePitch(eraseRect.startLane);
            if (pitch >= 0)
                eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, pitch,
                          eraseRect.startLane, currentActiveSkill);
        }
    }
    if (eraseClickedSustainQN >= 0.0)
    {
        int pitch = resolveActivePitch(eraseClickedSustainLane);
        if (pitch >= 0)
            truncateNote(eraseDragTrackIdx, eraseClickedSustainQN, pitch, eraseClickedSustainLane);
    }
    endBatch();

    eraseDragActive   = false;
    eraseDragTrackIdx = -1;
    eraseClickedNoteQN      = -1.0;
    eraseClickedSustainQN   = -1.0;
    eraseClickedSustainLane = -1;
    overlayState.marqueeVisible         = false;
    overlayState.marqueeErase           = false;
    overlayState.eraseClickedNoteQN     = -1.0;
    overlayState.eraseClickedLane       = -1;
    overlayState.eraseClickedSustainQN  = -1.0;
    overlayState.eraseClickedSustainLane = -1;
    recomputeGhost();
}

//==============================================================================
// Key commands

bool WriteController::onKeyPress(const juce::KeyPress& key)
{
    auto cmd = commandMapper.resolveKey(writeModeActive(), key);

    switch (cmd)
    {
        case WriteCommand::ToggleWriteMode: setWriteModeActive(!writeModeActive()); return true;
        case WriteCommand::ToggleSubMode:   setSubMode(subMode() == SubMode::Draw ? SubMode::Edit : SubMode::Draw); return true;
        case WriteCommand::ToggleLaneTool:  setLaneTool(!laneTool());                         return true;
        case WriteCommand::ToggleSnap:      setSnapEnabled(!snapEnabled());                   return true;
        case WriteCommand::StepDown:        setStepDivision(std::max(1, stepDivision() / 2)); return true;
        case WriteCommand::StepUp:          setStepDivision(stepDivision() * 2);              return true;
        case WriteCommand::ToggleTuplet:    toggleTuplet();                                   return true;
        case WriteCommand::CycleTuplet:     cycleTuplet();                                    return true;
        default: return false;
    }
}
