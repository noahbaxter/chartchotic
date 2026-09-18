/*
  ==============================================================================

    MidiWriter.h
    DAW-agnostic interface for MIDI note mutations

    Abstracts note insert/delete/move operations so the authoring layer
    doesn't depend on REAPER directly. Each DAW backend provides its own
    implementation.

  ==============================================================================
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

/**
 * Abstract interface for writing MIDI notes back to the host DAW.
 *
 * All time positions are project quarter notes (QN). Backends are responsible
 * for converting to take-local time domains (e.g. take PPQ in REAPER).
 *
 * Single-note operations wrap their own undo block.
 * Batch operations share one undo block between beginBatch/endBatch.
 */
class MidiWriter
{
public:
    virtual ~MidiWriter() = default;

    /** Returns true if the writer is connected and can accept mutations. */
    virtual bool isAvailable() const = 0;

    // --- Sections ---
    // FF 01 text events of the form "[section NAME]" on a dedicated EVENTS track, not
    // notes on an instrument track. `name` holds just NAME, without the wrapper.
    //
    // The whole set is read and written at once: renaming or inserting one section can
    // renumber every later one, so replace-all keeps that a single edit and a single
    // undo point instead of index juggling.

    struct SectionInfo {
        double      qn = 0.0;
        std::string name;
    };

    /** All sections, sorted by qn. Empty when there is no EVENTS track. */
    virtual std::vector<SectionInfo> getSections() { return {}; }

    /** Replaces every section. Creates the EVENTS track and item if missing. */
    virtual bool setSections(const std::vector<SectionInfo>&) { return false; }

    /**
        Names the EVENTS item starting at `startQN`, which is what a song is.

        Matched on the item's own start rather than on what covers the position: songs are
        allowed to overlap, so an album item spanning the per-song items covers every one of
        their starts and would otherwise swallow the rename.
    */
    virtual bool setSongName(double, const std::string&) { return false; }

    /**
        Moves the song's single [coda] to `qn`, or removes it when `qn` is empty. Every
        existing [coda] on every EVENTS item is cleared first, so there is never more than
        one. Inside a batch it joins the batch's undo point instead of making its own, since
        a coda placed by writing a BRE is part of that one edit.
    */
    virtual bool setCoda(std::optional<double>) { return false; }

    /**
        Adds a text event at the start of a track's first item unless an
        identical one is already there. Used for chart-level switches like
        [ENABLE_CHART_DYNAMICS], which have to exist for the data already in
        the track to mean anything.
    */
    virtual bool ensureTrackTextEvent(int, const std::string&) { return false; }

    // --- Single-note operations (each creates its own undo point) ---

    virtual bool insertNote(int trackIndex, double startQN, double endQN,
                           int channel, int pitch, int velocity) = 0;

    virtual bool deleteNote(int trackIndex, int noteIndex) = 0;
    virtual bool deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN) { return deleteNote(trackIndex, noteIndex); }

    struct NoteInfo {
        int    noteIndex = -1;
        double startQN = 0;
        double endQN = 0;
        int    pitch = -1;
        // Drum dynamics live in velocity (ghost 1, accent 127, normal 100), so
        // anything that copies a note has to carry this across.
        int    velocity = 100;
    };

    virtual NoteInfo findNote(int trackIndex, double positionQN, int pitch)
    { (void)trackIndex; (void)positionQN; (void)pitch; return {}; }

    virtual std::vector<NoteInfo> findNotesInRange(int trackIndex, double startQN,
                                                    double endQN, int pitch)
    { (void)trackIndex; (void)startQN; (void)endQN; (void)pitch; return {}; }

    virtual bool moveNote(int trackIndex, int noteIndex,
                         double newStartQN, double newEndQN, int newPitch) = 0;

    // --- Batch operations (single undo point for all) ---

    virtual void beginBatch(const char* undoDescription) = 0;

    virtual bool batchInsertNote(int trackIndex, double startQN, double endQN,
                                int channel, int pitch, int velocity) = 0;

    virtual bool batchDeleteNote(int trackIndex, int noteIndex, double hintQN = -1.0) = 0;

    virtual bool batchMoveNote(int trackIndex, int noteIndex,
                              double newStartQN, double newEndQN, int newPitch) = 0;

    virtual void endBatch() = 0;
};
