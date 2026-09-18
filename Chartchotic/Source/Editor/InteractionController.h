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
    PhraseMath::Kind phraseKind() const { return writeController.phraseKind(); }
    int         stepDivision()    const { return writeController.stepDivision(); }
    int         tuplet()          const { return writeController.tuplet(); }
    bool        snapEnabled()     const { return writeController.snapEnabled(); }
    bool        barMode()         const { return barModeFlag; }
    Part        activePart()      const { return writeController.activePart(); }
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
    void setActivePart(Part p)        { writeController.setActivePart(p); editController.setActivePart(p); }
    void setActiveSkill(SkillLevel s) { writeController.setActiveSkill(s); editController.setActiveSkill(s); }
    void setDrumDynamic(DrumDynamic d){ writeController.setDrumDynamic(d); editController.setDrumDynamic(d); }
    void setGuitarForce(GuitarForce f){ writeController.setGuitarForce(f); editController.setGuitarForce(f); }
    void setCymbalMode(bool c)        { writeController.setCymbalMode(c);  editController.setCymbalMode(c); }
    void setFlamMode(bool f)          { writeController.setFlamMode(f);    editController.setFlamMode(f); }

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
    // Gems only exist to edit in Notes; the phrase views author ranges instead.
    bool gemsAuthored() const { return writeController.view() == EditorView::Notes; }
    void applyModifierSlot(const ModifierSlot& slot);

    juce::ValueTree& state;
    bool barModeFlag = false;
    OptimisticPatchBuffer patchBuffer;
    WriteController writeController;
    EditController  editController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractionController)
};
