#include "InteractionController.h"
#include "../UI/ControlConstants.h"
#include <limits>

InteractionController::InteractionController(juce::ValueTree& st)
    : state(st), writeController(st)
{
    writeController.onStateChanged = [this]() {
        if (writeController.subMode() == SubMode::Draw)
            editController.clearSelection();
        if (onStateChanged) onStateChanged();
    };

    editController.onStateChanged = [this]() {
        if (onStateChanged) onStateChanged();
    };
}

bool InteractionController::isEditActive() const
{
    return writeController.writeModeActive()
        && writeController.subMode() == SubMode::Edit;
}

void InteractionController::onFrame(MidiWriter* writer, InstrumentSession* session,
                                    const bool* playingState, Part activePart, SkillLevel activeSkill,
                                    double currentProjectQN)
{
    writeController.setMidiWriter(writer);
    writeController.setInstrumentSession(session);
    writeController.setPlayingStatePtr(playingState);
    writeController.setActivePart(activePart);
    writeController.setActiveSkill(activeSkill);
    writeController.setPatchBuffer(&patchBuffer);

    editController.setMidiWriter(writer);
    editController.setInstrumentSession(session);
    editController.setPlayingStatePtr(playingState);
    editController.setActivePart(activePart);
    editController.setActiveSkill(activeSkill);
    editController.setStepDivision(writeController.stepDivision());
    editController.setTuplet(writeController.tuplet());
    editController.setSnapEnabled(writeController.snapEnabled());
    editController.setPatchBuffer(&patchBuffer);

    writeController.setBarMode(barModeFlag);
    editController.setBarMode(barModeFlag);

    bool kick2x = (bool)state.getProperty("kick2x", false);
    writeController.setKick2x(kick2x);
    editController.setKick2x(kick2x);

    patchBuffer.tick();
    bool playing = playingState ? *playingState : false;
    writeController.onFrameTick(currentProjectQN, playing);
    editController.onFrameTick();
}

void InteractionController::setView(EditorView v)
{
    if (writeController.view() == v) return;
    // A selection made in one view means nothing in the next.
    editController.clearSelection();
    writeController.setView(v);
    dropCymbalUnlessPlainNotes();
}

void InteractionController::onPointerMove(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    writeController.onPointerMove(p, ctx);
    if (gemsAuthored()) editController.onPointerMove(p, ctx);
}

// A phrase is a range, not a note, so the phrase views go to the write controller whichever
// sub-mode is showing: there is nothing for edit mode to select.
void InteractionController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!highwayAuthors()) return;
    if (isEditActive() && gemsAuthored()) editController.onPointerDown(p, ctx);
    else                                  writeController.onPointerDown(p, ctx);
}

void InteractionController::onPointerDrag(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!highwayAuthors()) return;
    if (isEditActive() && gemsAuthored()) editController.onPointerDrag(p, ctx);
    else                                  writeController.onPointerDrag(p, ctx);
}

void InteractionController::onPointerUp(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!highwayAuthors()) return;
    if (isEditActive() && gemsAuthored()) editController.onPointerUp(p, ctx);
    else                                  writeController.onPointerUp(p, ctx);
}

void InteractionController::onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (gemsAuthored() && isEditActive())
        editController.onPointerDoubleClick(p, ctx);
}

void InteractionController::onPointerExit()
{
    writeController.onPointerExit();
    editController.onPointerExit();
}

void InteractionController::onPointerCancel()
{
    writeController.onPointerCancel();
}

bool InteractionController::onKeyPress(const juce::KeyPress& key)
{
    auto cmd = writeController.resolveKeyCommand(key);

    // In the Phrase view the home row picks which phrase a drag writes, the same way it picks a
    // note type in the Notes view.
    if (writeController.view() == EditorView::Phrase
        && cmd >= WriteCommand::ModifierSlot1 && cmd <= WriteCommand::ModifierSlot5)
    {
        int index = (int)cmd - (int)WriteCommand::ModifierSlot1;
        // The focused part, not the hovered one: the keyboard acts on what the bar shows and
        // what a click would write to, never on whichever highway the cursor happens to be over.
        auto kinds = PhraseMath::kindsFor(focusPart());
        if (index >= (int)kinds.size()) return false;

        writeController.setPhraseKind(kinds[(size_t)index]);
        return true;
    }

    // Ahead of the gem gate below, which the lane tool itself fails: R has to switch the tool
    // back off. Routed here rather than through the write controller so it drops the cymbal
    // flag, and so it stays inert in the phrase views, which have no lanes to draw.
    if (cmd == WriteCommand::ToggleLaneTool)
    {
        if (view() != EditorView::Notes) return false;
        // With a selection up, R applies a lane to it rather than arming the tool, matching what
        // the LANE pill does and what the note-type keys already do to a selection.
        if (isEditActive() && hasEditSelection()) { applyLaneToSelection(); return true; }
        setLaneTool(!laneTool());
        return true;
    }

    // Outside the gem views the keys that act on gems have nothing to act on. Mode and grid
    // keys still work, since the other views use the same grid.
    if (!gemsAuthored())
    {
        switch (cmd)
        {
            case WriteCommand::ToggleWriteMode:
            case WriteCommand::ToggleSubMode:
            case WriteCommand::ToggleSnap:
            case WriteCommand::ToggleTuplet:
            case WriteCommand::CycleTuplet:
            case WriteCommand::StepDown:
            case WriteCommand::StepUp:
                return writeController.onKeyPress(key);
            default:
                return false;
        }
    }

    if (cmd == WriteCommand::ToggleBarMode)
    {
        setBarMode(!barModeFlag);
        return true;
    }

    // Note-type slots live here rather than in WriteController because the
    // dynamic/force/cymbal setters have to reach both controllers, and edit
    // mode applies them to the selection the same way the sub-toolbar does.
    if (cmd >= WriteCommand::ModifierSlot1 && cmd <= WriteCommand::ModifierSlot5)
    {
        int index = (int)cmd - (int)WriteCommand::ModifierSlot1;
        auto slots = modifierSlotsFor(getRenderType(focusPart()), writeController.laneTool());
        if (index >= (int)slots.size())
            return false;

        applyModifierSlot(slots[(size_t)index]);
        writeController.stateDidChange();
        return true;
    }

    int code = key.getKeyCode();

    if (code == 'C' && !key.getModifiers().isCommandDown()
        && writeController.writeModeActive())
    {
        if (!isEditActive())
        {
            writeController.beginStampCapture();
            return true;
        }
        if (isEditActive())
        {
            const auto& sel = editController.getSelection();
            std::vector<WriteController::StampNote> notes;
            double minQN = std::numeric_limits<double>::max();
            for (const auto& n : sel)
                if (!n.sustainOnly && n.startQN < minQN) minQN = n.startQN;

            for (const auto& n : sel)
            {
                if (n.sustainOnly) continue;
                auto info = editController.lookupNote(n.trackIdx, n.startQN, n.pitch);
                double dur = (info.noteIndex >= 0) ? (info.endQN - info.startQN) : kShortNoteDurationQN;
                uint32_t mask = editController.markerMaskAt(n.trackIdx, n.startQN, n.lane);
                notes.push_back({ n.lane, n.startQN - minQN, dur, info.velocity, mask });
            }
            if (notes.size() >= 2)
            {
                writeController.setStamp(std::move(notes));
                writeController.setSubMode(SubMode::Draw);
            }
            return true;
        }
    }

    if (writeController.hasStamp())
    {
        if (code == juce::KeyPress::leftKey)  { writeController.shiftStampLanes(-1); return true; }
        if (code == juce::KeyPress::rightKey) { writeController.shiftStampLanes(1);  return true; }
    }

    if (cmd == WriteCommand::DeselectAll && writeController.hasStamp())
    {
        writeController.clearStamp();
        return true;
    }

    if (isEditActive())
    {
        if (editController.onKeyPress(key))
            return true;
    }
    return writeController.onKeyPress(key);
}

void InteractionController::applyModifierSlot(const ModifierSlot& slot)
{
    // Pressing the slot that is already active drops its group back to the
    // group default, so "normal"/"auto" get a key without needing a rule.
    switch (slot.group)
    {
        case ModifierGroup::Dynamic:
        {
            auto target = (DrumDynamic)slot.value;
            auto next = writeController.drumDynamic() == target ? DrumDynamic::Normal
                                                                : target;
            setDrumDynamic(next);
            if (isEditActive()) applyDrumDynamicToSelection(next);
            break;
        }
        case ModifierGroup::Cymbal:
        {
            bool next = !writeController.cymbalMode();
            setCymbalMode(next);
            if (isEditActive()) applyCymbalModeToSelection(next);
            break;
        }
        case ModifierGroup::Flam:
        {
            bool next = !writeController.flamMode();
            setFlamMode(next);
            if (isEditActive()) applyFlamModeToSelection(next);
            break;
        }
        case ModifierGroup::Force:
        {
            auto target = (GuitarForce)slot.value;
            auto next = writeController.guitarForce() == target ? GuitarForce::None
                                                                : target;
            setGuitarForce(next);
            if (isEditActive()) applyGuitarForceToSelection(next);
            break;
        }
    }
}

void InteractionController::setBarMode(bool v)
{
    if (barModeFlag == v) return;
    barModeFlag = v;
    dropCymbalUnlessPlainNotes();
    writeController.stateDidChange();
    if (onStateChanged) onStateChanged();
}

const OverlayState& InteractionController::getOverlayState() const
{
    if (isEditActive())
        return editController.getOverlayState();
    return writeController.getOverlayState();
}
