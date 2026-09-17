#include "PhraseWriter.h"

using namespace PhraseMath;

namespace
{
    constexpr int FILL_REFERENCE_PITCH = (int)MidiPitchDefinitions::Guitar::BRE_5;
}

int PhraseWriter::writerTrack() const
{
    for (const auto& info : session.getTracks())
        if (info.part == part) return info.sourceTrackIndex;
    return -1;
}

int PhraseWriter::sessionTrack() const
{
    const auto& tracks = session.getTracks();
    for (int i = 0; i < (int)tracks.size(); ++i)
        if (tracks[i].part == part) return i;
    return -1;
}

void PhraseWriter::invalidatePart()
{
    if (int idx = sessionTrack(); idx >= 0) session.invalidateTrack(idx);
}

// The coda lives on EVENTS, so nothing sees a moved one until that track is re-read.
void PhraseWriter::invalidateEvents()
{
    const auto& tracks = session.getTracks();
    for (int i = 0; i < (int)tracks.size(); ++i)
        if (tracks[i].part == Part::EVENTS)
        {
            session.invalidateTrack(i);
            return;
        }
}

std::optional<double> PhraseWriter::codaQN() const
{
    const auto& sections = session.getSongSections();
    if (!sections.hasCoda()) return std::nullopt;
    return sections.getCodaPPQ().toDouble();
}

//==============================================================================
// BRE: one song-global phrase, written across every part that can carry one.

std::vector<PhraseWriter::BreTrack> PhraseWriter::breTracks() const
{
    std::vector<BreTrack> out;
    const auto& tracks = session.getTracks();
    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        auto kindPitches = pitches(Kind::Bre, tracks[i].part);
        if (kindPitches.empty()) continue;
        out.push_back({ i, tracks[i].sourceTrackIndex, tracks[i].part, std::move(kindPitches) });
    }
    return out;
}

// Every BRE currently in the chart: a 120 phrase the coda covers, on any part. Real charts carry
// exactly one across all parts at the same tick, which is why writing another replaces it.
std::vector<PhraseSpan> PhraseWriter::existingBres(const BreTrack& t) const
{
    std::vector<PhraseSpan> out;
    const auto coda = codaQN();
    for (const auto& s : rangesOf(session.getNotes(t.sessionIdx), FILL_REFERENCE_PITCH))
        if (isBre(s.startQN, coda)) out.push_back(s);
    return out;
}

void PhraseWriter::eraseBreNotes(const BreTrack& t)
{
    for (const auto& span : existingBres(t))
        for (int pitch : t.pitches)
        {
            auto note = writer.findNote(t.writerIdx, span.startQN, pitch);
            if (note.noteIndex >= 0 && GridMath::same(note.startQN, span.startQN))
                writer.batchDeleteNote(t.writerIdx, note.noteIndex, span.startQN);
        }
}

juce::String PhraseWriter::writeBre(double startQN, double endQN)
{
    const auto targets = breTracks();
    if (targets.empty()) return "No part in this song can carry a BRE";

    const bool replacing = std::any_of(targets.begin(), targets.end(),
                                       [this](const BreTrack& t) { return !existingBres(t).empty(); });

    writer.beginBatch("Chartchotic: Set BRE");

    bool wrote = true;
    for (const auto& t : targets)
    {
        eraseBreNotes(t);
        for (int pitch : t.pitches)
            wrote &= writer.batchInsertNote(t.writerIdx, startQN, endQN, PHRASE_CHANNEL, pitch,
                                            PHRASE_VELOCITY);
    }

    // The coda is what makes these markers a BRE rather than a drum fill, so it moves with them
    // inside the same batch: undoing the BRE must undo the coda too. Without it the markers are
    // still written, but every one of them reads back as a fill, so this is worth saying out loud.
    const bool codaOk = writer.setCoda(startQN);

    writer.endBatch();

    for (const auto& t : targets) session.invalidateTrack(t.sessionIdx);
    invalidateEvents();

    if (!wrote)  return "Could not write the BRE markers";
    if (!codaOk) return "Wrote the BRE markers, but no [coda] on EVENTS, so they read as a fill";
    return replacing ? "BRE moved; replaced the one already in this song" : juce::String();
}

void PhraseWriter::eraseBre()
{
    const auto targets = breTracks();

    writer.beginBatch("Chartchotic: Delete BRE");
    for (const auto& t : targets)
        eraseBreNotes(t);

    // Without a BRE the coda is a trap: the next drum fill charted past it would read as one.
    writer.setCoda(std::nullopt);
    writer.endBatch();

    for (const auto& t : targets) session.invalidateTrack(t.sessionIdx);
    invalidateEvents();
}

//==============================================================================

juce::String PhraseWriter::create(Kind kind, double startQN, double endQN)
{
    if (kind == Kind::Bre) return writeBre(startQN, endQN);

    int track = writerTrack();
    if (track < 0 || !writer.isAvailable()) return "No track for this part to write to";

    if (pitches(kind, part).empty())
        return "This part has no " + juce::String(phraseLabel(kind, part)) + " phrase";

    writer.beginBatch(kind == Kind::Fill ? "Chartchotic: Add fill"
                    : kind == Kind::Solo ? "Chartchotic: Add solo"
                                         : "Chartchotic: Add star power");

    // A refused insert used to be indistinguishable from a successful one, which made a write
    // that never landed look like a rendering bug.
    bool wrote = true;
    for (int pitch : pitches(kind, part))
        wrote &= writer.batchInsertNote(track, startQN, endQN, PHRASE_CHANNEL, pitch, PHRASE_VELOCITY);

    writer.endBatch();
    invalidatePart();

    if (!wrote) return "Could not write the phrase to this track";
    return {};
}

juce::String PhraseWriter::resize(Kind kind, double startQN, double endQN)
{
    if (kind == Kind::Bre) return writeBre(startQN, endQN);

    int track = writerTrack();
    if (track < 0 || !writer.isAvailable()) return "No track for this part to write to";

    writer.beginBatch("Chartchotic: Resize phrase");

    // Found again before every move: a batch write goes to the take the last lookup landed on.
    for (int pitch : pitches(kind, part))
    {
        auto note = writer.findNote(track, startQN, pitch);
        if (note.noteIndex >= 0 && GridMath::same(note.startQN, startQN))
            writer.batchMoveNote(track, note.noteIndex, note.startQN, endQN, pitch);
    }

    writer.endBatch();
    invalidatePart();
    return {};
}

void PhraseWriter::erase(Kind kind, double startQN)
{
    if (kind == Kind::Bre) { eraseBre(); return; }

    int track = writerTrack();
    if (track < 0 || !writer.isAvailable()) return;

    writer.beginBatch("Chartchotic: Delete phrase");

    for (int pitch : pitches(kind, part))
    {
        auto note = writer.findNote(track, startQN, pitch);
        if (note.noteIndex >= 0 && GridMath::same(note.startQN, startQN))
            writer.batchDeleteNote(track, note.noteIndex, startQN);
    }

    writer.endBatch();
    invalidatePart();
}
