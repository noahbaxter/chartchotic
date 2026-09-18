#pragma once

#include <JuceHeader.h>
#include "WriteController.h"
#include "EditController.h"
#include "ModifierSlots.h"

class InteractionController
{
public:
    explicit InteractionController(juce::ValueTree& state);

    void onFrame(MidiWriter* writer, InstrumentSession* session,
                 const bool* playingState, Part activePart, SkillLevel activeSkill,
                 double currentProjectQN);

    void onPointerMove       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp         (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit();
    void onPointerCancel();
    bool onKeyPress(const juce::KeyPress& key);

    bool        writeModeActive() const { return writeController.writeModeActive(); }
    SubMode     subMode()         const { return writeController.subMode(); }
    EditorView  view()            const { return writeController.view(); }
    bool        laneTool()        const { return writeController.laneTool(); }
    PhraseTarget laneType()       const { return writeController.laneType(); }
    void setLaneType(PhraseTarget t)    { writeController.setLaneType(t); }
    PhraseMath::Kind phraseKind() const { return writeController.phraseKind(); }
    int         stepDivision()    const { return writeController.stepDivision(); }
    int         tuplet()          const { return writeController.tuplet(); }
    bool        snapEnabled()     const { return writeController.snapEnabled(); }
    bool        barMode()         const { return barModeFlag; }
    Part        activePart()      const { return writeController.activePart(); }
    // The part the write bar describes and a click writes to. Tracks the FOCUSED highway, while
    // activePart follows the cursor across highways so a hover ghosts on the one being pointed
    // at. The two differ only between a move onto another highway and the click that claims it.
    Part        focusPart()       const { return focusedPart; }
    void        setFocusPart(Part p)    { focusedPart = p; }
    SkillLevel  activeSkill()     const { return writeController.activeSkill(); }
    DrumDynamic drumDynamic()     const { return writeController.drumDynamic(); }
    GuitarForce guitarForce()     const { return writeController.guitarForce(); }
    bool        cymbalMode()      const { return writeController.cymbalMode(); }
    bool        flamMode()        const { return writeController.flamMode(); }
    bool        hasStamp()        const { return writeController.hasStamp(); }
    bool        hasEditSelection() const { return !editController.getSelection().empty(); }

    void setWriteModeActive(bool v) { writeController.setWriteModeActive(v); }
    void setSubMode(SubMode m)      { writeController.setSubMode(m); }
    void setView(EditorView v);
    void setPhraseKind(PhraseMath::Kind k) { writeController.setPhraseKind(k); }
    void setStepDivision(int d)     { writeController.setStepDivision(d); }
    void setTuplet(int t)           { writeController.setTuplet(t); }
    void cycleTuplet()              { writeController.cycleTuplet(); }
    void setSnapEnabled(bool v)     { writeController.setSnapEnabled(v); }
    void clearEditSelection()       { editController.clearSelection(); }
    void setBarMode(bool v);
    // Lanes are drawn, never selected, and edit mode has nothing to do with them: leaving the
    // sub-mode on Edit would swallow every click with no sign why.
    void setLaneTool(bool v)
    {
        if (v) writeController.setSubMode(SubMode::Draw);
        writeController.setLaneTool(v);
        dropCymbalUnlessPlainNotes();
    }

    // A cymbal is a property of a drum pad hit, so it only means anything while plain notes are
    // what a click writes. A kick bar, a lane and every phrase are written as something else, and
    // leaving the flag set through them would have it come back on a note the user never set it
    // for. Enforced here rather than at each caller so the keyboard and the bar cannot disagree.
    void dropCymbalUnlessPlainNotes()
    {
        if (!cymbalMode()) return;
        if (barMode() || laneTool() || view() != EditorView::Notes) setCymbalMode(false);
    }
    void setActivePart(Part p)        { writeController.setActivePart(p); editController.setActivePart(p); }
    void setActiveSkill(SkillLevel s) { writeController.setActiveSkill(s); editController.setActiveSkill(s); }
    void setDrumDynamic(DrumDynamic d){ writeController.setDrumDynamic(d); editController.setDrumDynamic(d); }
    void setGuitarForce(GuitarForce f){ writeController.setGuitarForce(f); editController.setGuitarForce(f); }
    void setCymbalMode(bool c)        { writeController.setCymbalMode(c);  editController.setCymbalMode(c); }
    void setFlamMode(bool f)          { writeController.setFlamMode(f);    editController.setFlamMode(f); }

    // A lane over the selection, alongside the note-type appliers. The selection lives on the
    // edit controller and the lane write on the write controller, so it is joined up here.
    void applyLaneToSelection()
    {
        writeController.applyLaneToSelection(editController.getSelection());
    }
    bool selectionHasLane() const
    {
        return writeController.selectionHasLane(editController.getSelection());
    }
    void setLaneTypeOnSelection(PhraseTarget t)
    {
        writeController.setLaneTypeOnSelection(editController.getSelection(), t);
    }

    void applyDrumDynamicToSelection(DrumDynamic d) { editController.applyDrumDynamicToSelection(d); }
    void applyGuitarForceToSelection(GuitarForce f) { editController.applyGuitarForceToSelection(f); }
    void applyCymbalModeToSelection(bool c)         { editController.applyCymbalModeToSelection(c); }
    void applyFlamModeToSelection(bool f)           { editController.applyFlamModeToSelection(f); }

    const OverlayState& getOverlayState() const;
    const OptimisticPatchBuffer& getPatchBuffer() const { return patchBuffer; }

    std::function<void()> onStateChanged;

    // Why a phrase gesture wrote nothing, for the editor to show. A setter rather than a
    // reference to the write controller's own: that member is declared after this one.
    void setPhraseRefusedHandler(std::function<void(const juce::String&)> f)
    {
        writeController.onPhraseRefused = std::move(f);
    }

    // Called after a phrase write lands, so the highway can re-read the MIDI it parses itself.
    void setChartWrittenHandler(std::function<void()> f)
    {
        writeController.onChartWritten = std::move(f);
    }

private:
    bool isEditActive() const;
    // Views that write from the highway. Sections and Tempo do not yet.
    bool highwayAuthors() const
    {
        auto v = writeController.view();
        return v == EditorView::Notes || v == EditorView::StarPower || v == EditorView::Phrase;
    }
    // Gems only exist to edit in Notes; the phrase views author ranges instead. The lane tool
    // lives inside Notes but authors ranges too, so gems are not its to select or erase.
    bool gemsAuthored() const
    {
        return writeController.view() == EditorView::Notes && !writeController.laneTool();
    }
    void applyModifierSlot(const ModifierSlot& slot);

    juce::ValueTree& state;
    Part focusedPart = Part::GUITAR;
    bool barModeFlag = false;
    OptimisticPatchBuffer patchBuffer;
    WriteController writeController;
    EditController  editController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractionController)
};
