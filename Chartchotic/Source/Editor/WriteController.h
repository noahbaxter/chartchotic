#pragma once

#include <JuceHeader.h>
#include "AuthoringControllerBase.h"
#include "PhraseWriter.h"

class WriteController : public AuthoringControllerBase
{
public:
    explicit WriteController(juce::ValueTree& state);

    bool         writeModeActive() const { return writeModeActiveFlag; }
    SubMode      subMode()         const { return currentSubMode; }
    EditorView   view()            const { return currentView; }
    bool         laneTool()        const { return laneToolFlag; }
    PhraseMath::Kind phraseKind()  const { return currentPhraseKind; }
    // Which of the two shared lane markers a write uses. Picked rather than derived; see
    // LaneMath.h for why guessing it does not work. Inert on elite, which has no variant.
    PhraseTarget laneType()        const { return currentLaneType; }
    void setLaneType(PhraseTarget t);

    // Everything worth preserving about a copied note. velocity carries drum
    // dynamics, markerMask carries note type (cymbal/tom, HOPO/strum/tap) as
    // slot flags over modifierMarkerPitches(). Both are per note so a paste
    // reproduces what was copied, not whatever the toolbar is set to.
    struct StampNote
    {
        int      lane;
        double   qnOffset;
        double   duration;
        int      velocity   = 100;
        // Velocity and mask are the whole note; the preview resolves its art from them at the
        // position it would land on, so it picks up the destination's star power.
        uint32_t markerMask = 0;
    };
    void setStamp(std::vector<StampNote> s);
    void clearStamp();
    void shiftStampLanes(int delta);
    bool hasStamp() const { return !stamp.empty(); }
    const std::vector<StampNote>& getStamp() const { return stamp; }

    void beginStampCapture();

    void setWriteModeActive(bool active);
    void setSubMode(SubMode mode);
    void setView(EditorView v);
    void setLaneTool(bool on);
    // A lane over the selected notes, the way a note type applies to a selection. Their extent
    // is the range and the columns they span decide one lane or two. A toggle: a selection
    // already under a lane loses it.
    void applyLaneToSelection(const std::vector<SelectedNote>& notes);
    // Whether that selection already sits under a lane, so the lane controls can show it lit.
    bool selectionHasLane(const std::vector<SelectedNote>& notes) const;
    // And which kind it is, so the right one of the two lights.
    std::optional<PhraseTarget> selectionLaneKind(const std::vector<SelectedNote>& notes) const;
    // Picks the kind and writes it over the selection, retyping any lane already there. Never
    // removes one: that is the LANE toggle's job.
    void setLaneTypeOnSelection(const std::vector<SelectedNote>& notes, PhraseTarget target);
    void setPhraseKind(PhraseMath::Kind k);

    // What a phrase gesture could not do, for the footer to say. Cleared when read.
    std::function<void(const juce::String&)> onPhraseRefused;

    // A phrase write only refreshes InstrumentSession, which is what the utility strip reads.
    // The highway parses its own copy of the MIDI, so it has to be told to re-read or the
    // phrase does not appear until the 50ms hash poll happens to notice.
    std::function<void()> onChartWritten;

    // Asks the editor for a section's name: title, text to start from, and where the answer goes.
    // Never called with an empty answer.
    using NameHandler = std::function<void(const juce::String&)>;
    std::function<void(const juce::String&, const juce::String&, NameHandler)> onNameRequested;
    void setStepDivision(int division);
    void setTuplet(int t);
    void toggleTuplet();
    void cycleTuplet();
    void setSnapEnabled(bool enabled);

    void onPointerMove   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp     (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit   ()    { lastPointValid = false; recomputeGhost(); }
    void onPointerCancel ()    {}
    void stateDidChange();
    bool onKeyPress      (const juce::KeyPress& key);
    WriteCommand resolveKeyCommand(const juce::KeyPress& key) const
    {
        return commandMapper.resolveKey(writeModeActive(), key);
    }
    void onFrameTick     (double currentProjectQN, bool isPlaying);

private:
    void loadPersistedState();

    juce::ValueTree& state;

    SubMode      currentSubMode      = SubMode::Draw;
    EditorView   currentView         = EditorView::Notes;
    // Inside the Notes view: a drag writes a lane marker instead of a sustain.
    bool         laneToolFlag        = false;
    // Inside the Phrase view: which phrase a drag writes. SP has only its own kind.
    PhraseMath::Kind currentPhraseKind = PhraseMath::Kind::Solo;

    // Phrase drag: a range on the part's own track, not a lane.
    bool   phraseDragActive = false;
    bool   phraseDragResizing = false;   // dragging an existing phrase's end rather than a new one
    double phraseDragStartQN = 0.0;
    double phraseDragEndQN = 0.0;

    PhraseMath::Kind kindForView() const;
    bool phraseViewActive() const;
    // Every phrase of the active kind on the active part, in order.
    std::vector<PhraseMath::PhraseSpan> phrasesOnPart() const;
    // Same, for one named kind, so the overlay can carry all three at once.
    std::vector<PhraseMath::PhraseSpan> phrasesOfKind(PhraseMath::Kind kind) const;
    // Where the song's coda is, which is what tells a fill from a BRE on the shared 120 markers.
    std::optional<double> codaQN() const;
    void recomputePhraseHover();
    void handlePhraseDown(const AuthoringPoint& p, const AuthoringContext& ctx);
    void handlePhraseDrag(const AuthoringPoint& p);
    void handlePhraseUp();
    void clearPhraseDrag();
    void recomputePhrasePreview();

    // Sections are points with a name rather than ranges: click empty time to add one, click one
    // to rename it, right click to delete.
    void handleSectionDown(const AuthoringPoint& p, const AuthoringContext& ctx);
    void handleSectionDrag(const AuthoringPoint& p);
    void handleSectionUp();
    void clearSectionDrag();
    void recomputeSectionHover();
    // The song's sections as the highway draws them, from every EVENTS item.
    std::vector<MidiWriter::SectionInfo> currentSections() const;
    // `renumber` closes the gap in numbered families ("Verse 1, Verse 2"); a rename skips it,
    // since renumbering would undo the number just typed.
    void commitSections(std::vector<MidiWriter::SectionInfo> sections, const char* undoLabel,
                        bool renumber);

    // Pressing on a section holds it: a drag moves it, a release without one renames it.
    bool        sectionDragActive = false;
    bool        sectionDragMoved  = false;
    double      sectionDragFromQN = 0.0;
    double      sectionDragToQN   = 0.0;
    std::string sectionDragName;
    PhraseTarget currentLaneType     = PhraseTarget::Lane1;
    bool         writeModeActiveFlag = false;

    //==========================================================================
    // Lane drag. Its own state rather than the sustain drag's: a lane writes no gem, chain
    // extends nothing, and draws free of the grid until it lands, none of which a sustain does.
    // Raw QNs throughout, reconciled to the grid once on release.
    bool   laneDragActive     = false;
    int    laneDragTrackIdx   = -1;
    double laneDragRawStartQN = 0.0;
    double laneDragRawEndQN   = 0.0;
    // Columns the drag has reached across, widest extent so far. A lane is one lane until it is
    // pulled sideways: that is what asks for a trill on the shared markers, and for a lane per
    // pad on elite, which supports arbitrary combinations of them.
    int    laneDragColumnLo   = -1;
    int    laneDragColumnHi   = -1;

    void handleBeginLane  (const AuthoringPoint& p, int trackIdx);
    void handleUpdateLane (const AuthoringPoint& p);
    void handleCommitLane ();
    void clearLaneDrag();
    void recomputeLanePreview();
    void eraseLaneAt(const AuthoringPoint& p, int trackIdx);

    // The columns a gesture reaches across. A hover, or a drag never pulled sideways, is a
    // single column with lo == hi.
    struct LaneSpread
    {
        int lo = -1;
        int hi = -1;
        bool valid()  const { return lo >= 0 && hi >= lo; }
        int  count()  const { return valid() ? hi - lo + 1 : 0; }
        bool covers(int column) const { return column >= lo && column <= hi; }
    };
    LaneSpread laneDragSpread() const { return { laneDragColumnLo, laneDragColumnHi }; }
    static LaneSpread oneColumn(int column) { return { column, column }; }

    // Writes a resolved lane, or says why it could not. Shared by the drag and by applying one
    // to a selection, so the two cannot drift apart.
    void commitLane(int trackIdx, std::optional<PhraseMath::PhraseSpan> merged, LaneSpread spread);

    // A note a lane can snap to or capture: the dragged columns on elite, the whole board minus
    // kicks on the shared markers.
    struct LaneNote { double qn; int column; };

    // What a selection asks a lane to cover: its extent, and the columns it reaches across.
    struct LaneSelection { double lo = 0.0; double hi = 0.0; LaneSpread spread; };
    // Selected notes a lane could apply to, filtered the way the lane rules require.
    std::vector<LaneNote> laneSelectionNotes(const std::vector<SelectedNote>& notes) const;
    std::optional<LaneSelection> laneSelectionRange(const std::vector<SelectedNote>& notes) const;

    // A lane as written: the range, and the pitch it was written on, so an existing one can be
    // grown in place instead of being replaced by whatever a fresh write would have chosen.
    struct LanePlacement { PhraseMath::PhraseSpan span; int pitch; };
    std::vector<LanePlacement> lanesOnPart(LaneSpread spread) const;
    // Lanes anywhere in this stretch of time, which is what a selection acts on: a lane is a
    // range, not a property of a note, so reaching across columns does not mean one per column.
    std::vector<LanePlacement> lanesInRange(const LaneSelection& sel) const;
    // Whether every selected note already sits inside one of them.
    bool allSelectedUnderLane(const std::vector<SelectedNote>& notes,
                              const std::vector<LanePlacement>& lanes) const;
    void eraseLanePlacement(int trackIdx, const LanePlacement& lane);

    std::vector<LaneNote> laneCandidateNotes(LaneSpread spread) const;
    std::vector<LaneNote> laneNotesIn(double startQN, double endQN, LaneSpread spread) const;
    // The note nearest a point, in either direction, which is what an edge snaps to.
    std::optional<LaneNote> nearestLaneNote(double rawQN, LaneSpread spread) const;
    // One note reached to its neighbour, so a click has a lane to place rather than nothing.
    std::optional<PhraseMath::PhraseSpan> laneAround(double qn, LaneSpread spread) const;

    // The lane a gesture would commit, snapped and merged. Empty only where the part has no
    // notes to sit on. Hover, preview and commit share it, so the drawn shape is the written
    // one and anything on screen can be clicked into place.
    std::optional<PhraseMath::PhraseSpan> resolveLaneWrite(double rawStartQN, double rawEndQN,
                                                           LaneSpread spread) const;

    // Lanes already on the part that a new one here merges with.
    std::vector<PhraseMath::PhraseSpan> laneSpansOnPart(LaneSpread spread) const;

    // Columns struck under a range, in first-struck order, which is what the game reads a
    // shared marker's position off. Kicks excluded, as the game excludes them.
    std::vector<int> laneColumnsUnder(double startQN, double endQN, LaneSpread spread) const;
    // Where a lane over this range will actually be drawn: the dragged columns on elite, the
    // columns struck underneath on the shared markers.
    std::vector<int> laneDrawColumns(double startQN, double endQN, LaneSpread spread) const;
    // The pitches a lane over this range should be written as: one per dragged column on elite,
    // and the 1-lane or 2-lane marker on the shared instruments.
    std::vector<int> laneWritePitches(double startQN, double endQN, LaneSpread spread) const;
    // Lane pitches a click here could have landed on, for erase.
    std::vector<int> laneCandidatePitches(LaneSpread spread) const;

    AuthoringPoint lastPoint;
    bool           lastPointValid = false;

    // Erase drag state (deferred — notes tinted red, erased on commit)
    bool        eraseDragActive    = false;
    int         eraseDragTrackIdx  = -1;
    MarqueeRect eraseRect;
    double      eraseClickedNoteQN = -1.0;
    double      eraseClickedSustainQN = -1.0;
    int         eraseClickedSustainLane = -1;

    // Sustain drag state
    bool   sustainDragActive   = false;
    int    sustainDragTrackIdx = -1;
    double sustainDragStartQN  = 0.0;
    int    sustainDragLane     = -1;
    RangeTarget sustainDragTarget;
    bool   sustainPendingClick    = false;
    double sustainPendingClickQN  = 0.0;

    // Paint drag state
    bool   paintDragActive   = false;
    int    paintDragTrackIdx = -1;
    double paintStartQN      = 0.0;
    double paintLastQN        = 0.0;
    int    paintLastLane      = -1;
    struct PaintedNote { double qn; int lane; };
    std::vector<PaintedNote> paintedNotes;
    void paintFillRange(double fromQN, double toQN, int lane);
    void paintShrinkTo(double lo, double hi);

    std::vector<StampNote> stamp;

    // Stamp capture (hold C + drag in draw mode)
    // Value T restores when toggling the tuplet grid back on. Triplet until
    // Shift+T picks something else.
    int         lastTuplet = 3;

    // Shift in kick mode arms the alternating 1x/2x paint. Polled per frame so
    // the hint appears the moment the key goes down, with no mouse move.
    bool        altKickArmed = false;
    bool        altKickAvailable() const;

    bool        stampCaptureActive = false;
    int         stampCaptureTrackIdx = -1;
    MarqueeRect stampCaptureRect;

    bool   canWrite(const AuthoringPoint& p) const;
    void   recomputeGhost();
    void   enterSustainDrag(int trackIdx, double startQN, int lane, RangeTarget target);
    void   clearSustainDrag();

    void handleBeginSustain  (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleUpdateSustain (const AuthoringPoint& p);
    void handleCommitSustain (const AuthoringPoint& p);
    void handleBeginPaint    (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleContinuePaint (const AuthoringPoint& p);
    void handleCommitPaint   ();
    void handleBeginErase    (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleContinueErase (const AuthoringPoint& p);
    void handleEndErase      ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteController)
};
