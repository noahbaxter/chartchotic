#pragma once

#include <JuceHeader.h>
#include "../Midi/InstrumentSession.h"
#include "../Midi/Providers/MidiWriter.h"
#include "../Visual/PhraseMath.h"

// Writes what a phrase gesture asked for. Star power, solo and fill land on the one part that
// took the gesture. A BRE does not: it is a single song-global moment written across every part
// at once, plus the [coda] that makes those markers a BRE rather than a drum fill, so writing one
// replaces whatever BRE the song already had.
class PhraseWriter
{
public:
    PhraseWriter(MidiWriter& writer, InstrumentSession& session, Part part)
        : writer(writer), session(session), part(part) {}

    // Empty on success, otherwise what happened: either the reason nothing was written, or a note
    // that a BRE elsewhere in the song was replaced.
    juce::String create(PhraseMath::Kind kind, double startQN, double endQN);

    // A phrase keeps its start; only where it ends moves, the way a sustain's tail does. A BRE is
    // rewritten wholesale instead, since it spans every part.
    juce::String resize(PhraseMath::Kind kind, double startQN, double endQN);

    void erase(PhraseMath::Kind kind, double startQN);

private:
    static constexpr int PHRASE_CHANNEL = 0;
    static constexpr int PHRASE_VELOCITY = 100;

    // One part that can carry a BRE, with both index spaces it needs: the session's for reading
    // notes, the backend's for writing them. Mixing the two fails silently.
    struct BreTrack
    {
        int sessionIdx = -1;
        int writerIdx = -1;
        Part part = Part::GUITAR;
        std::vector<int> pitches;
    };

    int writerTrack() const;
    int sessionTrack() const;
    void invalidatePart();
    void invalidateEvents();

    std::optional<double> codaQN() const;

    std::vector<BreTrack> breTracks() const;
    std::vector<PhraseMath::PhraseSpan> existingBres(const BreTrack& t) const;
    void eraseBreNotes(const BreTrack& t);
    juce::String writeBre(double startQN, double endQN);
    void eraseBre();

    MidiWriter& writer;
    InstrumentSession& session;
    Part part;
};
