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
    PhraseTarget phraseTarget()    const { return currentPhraseTarget; }

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
    void setPhraseKind(PhraseMath::Kind k);

    // What a phrase gesture could not do, for the footer to say. Cleared when read.
    std::function<void(const juce::String&)> onPhraseRefused;

    // A phrase write only refreshes InstrumentSession, which is what the utility strip reads.
    // The highway parses its own copy of the MIDI, so it has to be told to re-read or the
    // phrase does not appear until the 50ms hash poll happens to notice.
    std::function<void()> onChartWritten;
    void setPhraseTarget(PhraseTarget t);
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
    PhraseTarget currentPhraseTarget = PhraseTarget::Lane1;
    bool         writeModeActiveFlag = false;

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
    void handleBeginPhrase   (const AuthoringPoint& p, int trackIdx, double clickQN);
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
