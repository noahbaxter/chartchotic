#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <vector>
#include "TrackDiscovery.h"
#include "../../UI/ControlConstants.h"

// Which REAPER track feeds which part. Discovery reads canonical track names; this is the layer
// over it that guesses at the names people actually use, and records the answers they correct by
// hand. A part lives on exactly one track, so every assignment here is a part claiming a track.
namespace PartMapping
{
    // How a track came to hold a part. A guess is the name reading like the part; a pin is the
    // user saying so, which outranks any guess and survives a rename.
    enum class Origin { Guess, Pin };

    struct Assignment
    {
        Part part;
        int  sourceTrackIndex;
        Origin origin;
    };

    // Name fragments that read as a part, most specific first: "PART GUITAR COOP" holds both
    // "coop" and "guitar", and the co-op reading is the one that is actually right. Order is the
    // whole rule, so a new entry goes above anything it is a special case of.
    struct NameHint { const char* fragment; Part part; };

    inline const std::vector<NameHint>& getNameHints()
    {
        static const std::vector<NameHint> hints = {
            { "elite",       Part::ELITE_DRUMS },
            { "coop",        Part::GUITAR_COOP },
            { "co op",       Part::GUITAR_COOP },
            { "rhythm",      Part::RHYTHM },
            { "ghl bass",    Part::GHL_BASS },
            { "bass ghl",    Part::GHL_BASS },
            { "ghl",         Part::GHL_GUITAR },
            { "real bass",   Part::PRO_BASS },
            { "pro bass",    Part::PRO_BASS },
            { "real guitar", Part::PRO_GUITAR },
            { "pro guitar",  Part::PRO_GUITAR },
            { "real keys",   Part::PRO_KEYS },
            { "pro keys",    Part::PRO_KEYS },
            { "harm",        Part::HARMONIES },
            { "vocal",       Part::VOCALS },
            { "vox",         Part::VOCALS },
            { "drum",        Part::DRUMS },
            { "bass",        Part::BASS },
            { "keys",        Part::KEYS },
            { "guitar",      Part::GUITAR },
            { "gtr",         Part::GUITAR },
        };
        return hints;
    }

    // Lowercased, with '_' and '-' read as spaces, so "Lead_Gtr" and "lead gtr" are one name.
    inline juce::String normalise(const juce::String& name)
    {
        return name.toLowerCase().replaceCharacters("_-", "  ");
    }

    // The part a track name reads as, or nothing when it reads as none. Canonical names are
    // matched first and exactly, so a track actually called "PART BASS" never falls to a hint.
    inline std::optional<Part> guessPart(const juce::String& trackName)
    {
        Part exact;
        if (matchTrackNameToPart(trackName.toStdString(), exact))
            return exact;

        const auto key = normalise(trackName);
        for (const auto& hint : getNameHints())
            if (key.contains(hint.fragment))
                return hint.part;

        return {};
    }

    // Tracks that carry a chart but no instrument. They are song-scoped or host-side, nothing
    // maps to them, and listing them only pads the table with rows nobody can use.
    inline bool isUtilityTrackName(const juce::String& name)
    {
        const auto key = normalise(name).trim();

        // The names that are not a Part at all, so there is nowhere to point them. EVENTS used
        // to be in here too, which is exactly why it could never be mapped and why sections
        // went empty: it has a Part and resolves through guessPart like any other track.
        //
        // Whole name, because these are short words an instrument track can legitimately
        // contain: "Beat Box Gtr" is a guitar, not the BEAT track.
        static const char* exact[] = { "beat", "master", "tempo map" };
        for (const auto* u : exact)
            if (key == u) return true;

        // Venue is matched as a word anywhere instead, since no instrument is ever called one
        // and templates prefix it freely ("MV VENUE"). The format only documents the bare
        // name (`_refs/midi/overview.md:51`), so the prefix is a template's, not the spec's.
        for (const auto& token : juce::StringArray::fromTokens(key, " ", {}))
            if (token == "venue") return true;

        return false;
    }

    // Whether the app should guess this track into a part on its own. Narrower than what a
    // user is allowed to pick by hand: a guess is the app asserting something, so an
    // audio-only track reading as "rhythm" stays quiet rather than claiming the column.
    // Empty counts, since a template's untouched PART RHYTHM is exactly what it says.
    // Shared by the menu and by discovery, so what the table shows is what the app loads.
    inline bool canBeGuessedAsPart(bool hasMidiItems, bool hasAudioItems, bool isUtility)
    {
        if (isUtility) return false;
        return !(hasAudioItems && !hasMidiItems);
    }

    // Whether a part can be mapped at all. The others are shown so the table says what exists,
    // but nothing reads them, so pointing a track at one would map into a dead end.
    //
    // NOT the same question as isHighwayRenderable, though it was an alias for it until the
    // EVENTS track went missing: EVENTS has no highway and never gets a slot, but something
    // does read it (sections and the song list, InstrumentSession.cpp:143), so it has to be
    // mappable. Conflating "can I draw it" with "can I point a track at it" is what silently
    // dropped it from discovery.
    inline bool isMappable(Part p) { return isHighwayRenderable(p) || p == Part::EVENTS; }

    // The parts almost every chart has. The rest are real but rare, and burying them behind a
    // toggle keeps the common case to a handful of columns instead of fourteen.
    inline bool isCommonPart(Part p)
    {
        switch (p) {
            case Part::GUITAR:
            case Part::BASS:
            case Part::DRUMS:
            case Part::KEYS:
            case Part::VOCALS: return true;
            default:           return false;
        }
    }

    // Every part that gets a column, in the app's usual order. Only the ones a track can
    // actually be pointed at: the GHL and Pro entries used to show struck through so the table
    // read as the full list, but a column you cannot use is a column in the way, and they made
    // the row wider than the useful part of it.
    inline std::vector<Part> columnParts()
    {
        std::vector<Part> parts;
        for (const auto& entry : getImplementedTrackNames())
            if (isMappable(entry.part))
                parts.push_back(entry.part);

        std::sort(parts.begin(), parts.end(), [](Part a, Part b)
                  { return getPartSortOrder(a) < getPartSortOrder(b); });
        return parts;
    }

    // Which track the waveform reads when nothing is pinned. It cannot be the master: REAPER's
    // audio accessor reads a track's own media items, not the audio routed through it, so the
    // master reads as silence however loud the mix is. The song's audio has to be on a real
    // track with a real clip, and this is the guess at which one that is.
    //
    // Deliberately loose. Nobody agrees on what to call it, the cost of a wrong guess is a
    // waveform of the wrong track (fixable with one click in the parts menu), and the cost of
    // no guess is an empty view that looks broken.
    inline bool isSongTrackName(const juce::String& name)
    {
        const auto key = name.toLowerCase();
        // No "track": REAPER names every new track "Track 1" and so on, so it would match
        // whatever happened to be first rather than the song. The audio requirement plus the
        // first-audio-track fallback already covers that case without guessing wrong out loud.
        static const char* wanted[] = { "song", "music", "audio", "backing", "mix" };

        for (const auto* w : wanted)
            if (key.contains(w)) return true;

        return false;
    }

    // The pins, and the assignments that fall out of them once the guesses fill the gaps.
    // Holds no track state of its own: the caller owns the track list and passes it in, so a
    // pin survives a track the plugin cannot currently see.
    class Map
    {
    public:
        // A part lives on one track, so pinning it anywhere takes it from wherever it was. That
        // is the whole conflict rule: refusing would leave a wrong pin with no way to correct it.
        void pin(Part part, int sourceTrackIndex)
        {
            if (!isMappable(part)) return;
            pins[part] = sourceTrackIndex;
            // Picking it is the opposite of turning it off, so it stops being off.
            suppressed.erase(part);
        }

        void unpin(Part part) { pins.erase(part); }

        // "This part has no track", said out loud. Unpinning alone is not enough: the name
        // guess that the pin was covering would just take over again, so a part turned off
        // by hand has to stay off until it is picked again.
        void clearPart(Part part)
        {
            pins.erase(part);
            suppressed.insert(part);
        }

        bool isSuppressed(Part part) const { return suppressed.count(part) > 0; }

        // A track was removed, so REAPER renumbered everything after it. Pins are held by
        // index, so they have to move with it or they quietly point at a neighbour. The pin
        // on the deleted track itself is dropped; there is nothing left to point at.
        void trackDeleted(int deletedIndex)
        {
            for (auto it = pins.begin(); it != pins.end(); )
            {
                if (it->second == deletedIndex)      it = pins.erase(it);
                else                               { if (it->second > deletedIndex) --it->second;
                                                     ++it; }
            }

            reindexAfterDelete(globalAudio, deletedIndex);

            for (auto it = audioPins.begin(); it != audioPins.end(); )
            {
                reindexAfterDelete(it->second, deletedIndex);
                // A set emptied by the delete is no set, so the part goes back to the global
                // one rather than being left choosing nothing.
                it = it->second.empty() ? audioPins.erase(it) : std::next(it);
            }
        }

        /*
            Which audio drives the waveform. Not a chart part and not one track: a part can
            read several, which are SUMMED into one shape (three drum stems as one waveform),
            and different parts can read different audio (drums against the drum stem, guitar
            against the guitar stem).

            Two levels, because the common case is one song track behind everything and it
            should stay one click. A part's own set wins if it has one; otherwise the global
            set applies to every part. Kept here beside the MIDI pins so there is one answer to
            "what is this track for" and one thing to save.
        */
        void toggleAudioSource(Part part, int sourceTrackIndex)
        {
            toggleIn(audioPins[part], sourceTrackIndex);
            // An emptied set is no set, so the part falls back to the global one rather than
            // being stuck on "chosen, but nothing", which reads identically and cannot be
            // undone from the grid.
            if (audioPins[part].empty()) audioPins.erase(part);
        }

        void toggleGlobalAudioSource(int sourceTrackIndex) { toggleIn(globalAudio, sourceTrackIndex); }

        // Whether this track is in the part's OWN set. Deliberately not answering for the
        // global fallback: the grid has to draw a part inheriting the global set differently
        // from one that picked the same track by hand, the same way a guess reads differently
        // from a pin.
        bool hasAudioSource(Part part, int sourceTrackIndex) const
        {
            auto it = audioPins.find(part);
            return it != audioPins.end() && contains(it->second, sourceTrackIndex);
        }

        bool hasGlobalAudioSource(int sourceTrackIndex) const
        {
            return contains(globalAudio, sourceTrackIndex);
        }

        bool hasOwnAudioSet(Part part) const { return audioPins.count(part) > 0; }

        const std::vector<int>& globalAudioSources() const { return globalAudio; }

        // What this part's waveform reads. Its own set if it has one, else the global set.
        //
        // Empty has no answer here on purpose, the same as an unpinned part: the fallback needs
        // the track list to guess from (isSongTrackName), which this class deliberately does not
        // hold, so the caller resolves it and the choice stays the only thing saved.
        std::vector<int> audioSources(Part part) const
        {
            auto it = audioPins.find(part);
            return it != audioPins.end() ? it->second : globalAudio;
        }

        std::optional<int> pinnedTrack(Part part) const
        {
            auto it = pins.find(part);
            return it == pins.end() ? std::optional<int>{} : it->second;
        }

        // Resolve pins and guesses against the tracks that exist. A pin always wins. A guess
        // only lands on a part no pin claimed, and the lowest track index wins a tie, so two
        // tracks reading as "guitar" settle on one rather than flickering between them.
        std::vector<Assignment> resolve(const std::vector<InstrumentTrackInfo>& tracks) const
        {
            std::vector<Assignment> out;

            // A pin to a track that is no longer there resolves to nothing. Keeping it would
            // light the part as mapped with no row to show for it, and the pin itself is kept
            // so the mapping comes back if the track does.
            auto trackExists = [&tracks](int index)
            {
                return std::any_of(tracks.begin(), tracks.end(),
                                   [index](const auto& t) { return t.sourceTrackIndex == index; });
            };

            for (const auto& [part, trackIndex] : pins)
                if (trackExists(trackIndex))
                    out.push_back({ part, trackIndex, Origin::Pin });

            auto claimed = [&out](Part p)
            {
                return std::any_of(out.begin(), out.end(),
                                   [p](const Assignment& a) { return a.part == p; });
            };

            auto sorted = tracks;
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto& a, const auto& b)
                      { return a.sourceTrackIndex < b.sourceTrackIndex; });

            for (const auto& track : sorted)
            {
                auto guess = guessPart(track.trackName);
                if (!guess || !isMappable(*guess) || claimed(*guess)) continue;
                if (isSuppressed(*guess)) continue;   // turned off by hand, stays off
                out.push_back({ *guess, track.sourceTrackIndex, Origin::Guess });
            }

            std::sort(out.begin(), out.end(), [](const Assignment& a, const Assignment& b)
                      { return getPartSortOrder(a.part) < getPartSortOrder(b.part); });
            return out;
        }

        // Pins ride in the plugin's own state tree, so they travel with the project and go with
        // the plugin. Stored under the part's canonical track name rather than the enum value,
        // so the saved state stays readable and survives the enum gaining a member.
        void writeTo(juce::ValueTree& tree) const
        {
            tree.removeAllChildren(nullptr);
            // The old single-track property. Removed rather than written, so a state saved by
            // this build does not resurrect a pre-multi-source reading of it on the way back in.
            tree.removeProperty("waveform", nullptr);

            for (int trackIndex : globalAudio)
            {
                juce::ValueTree row("audio");
                row.setProperty("track", trackIndex, nullptr);
                tree.appendChild(row, nullptr);
            }

            for (const auto& [part, set] : audioPins)
                for (int trackIndex : set)
                {
                    juce::ValueTree row("partaudio");
                    row.setProperty("part", trackNameFor(part), nullptr);
                    row.setProperty("track", trackIndex, nullptr);
                    tree.appendChild(row, nullptr);
                }

            for (const auto& [part, trackIndex] : pins)
            {
                juce::ValueTree row("pin");
                row.setProperty("part", trackNameFor(part), nullptr);
                row.setProperty("track", trackIndex, nullptr);
                tree.appendChild(row, nullptr);
            }

            // A part turned off by hand is as much a decision as one pointed at a track, so
            // it has to survive a reload the same way.
            for (auto part : suppressed)
            {
                juce::ValueTree row("off");
                row.setProperty("part", trackNameFor(part), nullptr);
                tree.appendChild(row, nullptr);
            }
        }

        void readFrom(const juce::ValueTree& tree)
        {
            pins.clear();
            suppressed.clear();
            audioPins.clear();
            globalAudio.clear();

            // A state written before the waveform could read more than one track. Its single
            // track becomes the global set, which is what it always meant.
            if (tree.hasProperty("waveform"))
                toggleGlobalAudioSource((int)tree.getProperty("waveform"));

            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                const auto row = tree.getChild(i);

                // The global audio set names no part, so it is read before anything asks which
                // part a row belongs to.
                if (row.hasType("audio"))
                {
                    toggleGlobalAudioSource((int)row.getProperty("track"));
                    continue;
                }

                Part part;
                if (!matchTrackNameToPart(row.getProperty("part").toString().toStdString(), part))
                    continue;

                if      (row.hasType("off"))       clearPart(part);
                else if (row.hasType("partaudio")) toggleAudioSource(part, (int)row.getProperty("track"));
                else                               pin(part, (int)row.getProperty("track"));
            }
        }

    private:
        static bool contains(const std::vector<int>& set, int value)
        {
            return std::find(set.begin(), set.end(), value) != set.end();
        }

        // Kept sorted, because the set is the pool's cache key: two parts that picked the same
        // tracks in a different order have to resolve to the same key or they build the same
        // audio twice.
        static void toggleIn(std::vector<int>& set, int value)
        {
            auto it = std::find(set.begin(), set.end(), value);
            if (it != set.end()) { set.erase(it); return; }

            set.insert(std::upper_bound(set.begin(), set.end(), value), value);
        }

        // Drops a deleted track and shifts everything above it down, in place.
        static void reindexAfterDelete(std::vector<int>& set, int deletedIndex)
        {
            for (auto it = set.begin(); it != set.end(); )
            {
                if (*it == deletedIndex)    it = set.erase(it);
                else                      { if (*it > deletedIndex) --*it;
                                            ++it; }
            }
        }

        std::map<Part, int> pins;
        std::set<Part> suppressed;

        // Per-part audio, and the set that applies to any part without its own.
        std::map<Part, std::vector<int>> audioPins;
        std::vector<int> globalAudio;
    };
}
