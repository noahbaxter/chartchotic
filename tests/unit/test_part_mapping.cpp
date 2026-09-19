#include <catch2/catch_test_macros.hpp>

#include "Midi/Discovery/PartMapping.h"

namespace
{
    InstrumentTrackInfo track(int index, const char* name)
    {
        return { Part::GUITAR, index, name };   // part is filled by resolve, not read from here
    }
}

TEST_CASE("PartMapping - a canonical name is matched exactly", "[partmapping]")
{
    REQUIRE(PartMapping::guessPart("PART GUITAR") == Part::GUITAR);
    REQUIRE(PartMapping::guessPart("PART ELITE_DRUMS") == Part::ELITE_DRUMS);
    // Discovery already treats '_' and ' ' as the same character.
    REQUIRE(PartMapping::guessPart("PART ELITE DRUMS") == Part::ELITE_DRUMS);
}

TEST_CASE("PartMapping - a name people actually type still reads", "[partmapping]")
{
    REQUIRE(PartMapping::guessPart("Lead Gtr") == Part::GUITAR);
    REQUIRE(PartMapping::guessPart("bass") == Part::BASS);
    REQUIRE(PartMapping::guessPart("Drum Kit") == Part::DRUMS);
    REQUIRE(PartMapping::guessPart("Lead_Gtr") == Part::GUITAR);
}

TEST_CASE("PartMapping - the more specific reading wins", "[partmapping]")
{
    // Both hold "guitar", and the co-op and rhythm readings are the right ones.
    REQUIRE(PartMapping::guessPart("Guitar Coop") == Part::GUITAR_COOP);
    REQUIRE(PartMapping::guessPart("Rhythm Guitar") == Part::RHYTHM);
    REQUIRE(PartMapping::guessPart("GHL Guitar") == Part::GHL_GUITAR);
    REQUIRE(PartMapping::guessPart("Elite Drums") == Part::ELITE_DRUMS);
    // "ghl bass" holds "ghl" and "bass"; neither plain reading is right.
    REQUIRE(PartMapping::guessPart("GHL Bass") == Part::GHL_BASS);
}

TEST_CASE("PartMapping - a name that reads as nothing is left alone", "[partmapping]")
{
    REQUIRE_FALSE(PartMapping::guessPart("Audio 1").has_value());
    REQUIRE_FALSE(PartMapping::guessPart("").has_value());
    REQUIRE_FALSE(PartMapping::guessPart("Reverb Bus").has_value());
}

TEST_CASE("PartMapping - guesses fill the parts no pin claimed", "[partmapping]")
{
    PartMapping::Map map;
    const std::vector<InstrumentTrackInfo> tracks = {
        track(0, "Lead Gtr"), track(1, "Bass"), track(2, "Drum Kit"),
    };

    const auto out = map.resolve(tracks);
    REQUIRE(out.size() == 3);
    for (const auto& a : out) REQUIRE(a.origin == PartMapping::Origin::Guess);
}

TEST_CASE("PartMapping - a pin outranks the guess on another track", "[partmapping]")
{
    PartMapping::Map map;
    const std::vector<InstrumentTrackInfo> tracks = { track(0, "Lead Gtr"), track(1, "Audio 2") };

    map.pin(Part::GUITAR, 1);
    const auto out = map.resolve(tracks);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].part == Part::GUITAR);
    REQUIRE(out[0].sourceTrackIndex == 1);
    REQUIRE(out[0].origin == PartMapping::Origin::Pin);
}

TEST_CASE("PartMapping - pinning a part steals it from wherever it was", "[partmapping]")
{
    PartMapping::Map map;
    map.pin(Part::DRUMS, 3);
    map.pin(Part::DRUMS, 7);

    REQUIRE(map.pinnedTrack(Part::DRUMS) == 7);
}

TEST_CASE("PartMapping - unpinning hands the part back to the guess", "[partmapping]")
{
    PartMapping::Map map;
    const std::vector<InstrumentTrackInfo> tracks = { track(0, "Lead Gtr") };

    map.pin(Part::GUITAR, 5);
    REQUIRE(map.resolve(tracks)[0].sourceTrackIndex == 5);

    map.unpin(Part::GUITAR);
    const auto out = map.resolve(tracks);
    REQUIRE(out[0].sourceTrackIndex == 0);
    REQUIRE(out[0].origin == PartMapping::Origin::Guess);
}

TEST_CASE("PartMapping - two tracks reading the same settle on the lower index", "[partmapping]")
{
    PartMapping::Map map;
    const std::vector<InstrumentTrackInfo> tracks = { track(4, "Gtr B"), track(2, "Gtr A") };

    const auto out = map.resolve(tracks);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].sourceTrackIndex == 2);
}

TEST_CASE("PartMapping - a part nothing renders cannot be pinned", "[partmapping]")
{
    PartMapping::Map map;
    map.pin(Part::VOCALS, 1);
    REQUIRE_FALSE(map.pinnedTrack(Part::VOCALS).has_value());
}

TEST_CASE("PartMapping - pins survive a round trip through state", "[partmapping]")
{
    PartMapping::Map map;
    map.pin(Part::GUITAR, 2);
    map.pin(Part::ELITE_DRUMS, 5);

    juce::ValueTree tree("partPins");
    map.writeTo(tree);

    PartMapping::Map restored;
    restored.readFrom(tree);

    REQUIRE(restored.pinnedTrack(Part::GUITAR) == 2);
    REQUIRE(restored.pinnedTrack(Part::ELITE_DRUMS) == 5);
}

TEST_CASE("PartMapping - clearing a part turns off the guess too", "[partmapping]")
{
    PartMapping::Map map;
    const std::vector<InstrumentTrackInfo> tracks = { track(0, "PART GUITAR") };

    REQUIRE(map.resolve(tracks).size() == 1);

    // Unpinning alone would hand the part straight back to the name guess, which is the
    // thing the user was trying to say no to.
    map.clearPart(Part::GUITAR);
    REQUIRE(map.resolve(tracks).empty());
    REQUIRE(map.isSuppressed(Part::GUITAR));

    // Picking it again is the opposite of turning it off.
    map.pin(Part::GUITAR, 0);
    REQUIRE_FALSE(map.isSuppressed(Part::GUITAR));
    REQUIRE(map.resolve(tracks).size() == 1);
}

TEST_CASE("PartMapping - a part turned off stays off across a reload", "[partmapping]")
{
    PartMapping::Map map;
    map.clearPart(Part::DRUMS);
    map.pin(Part::GUITAR, 1);

    juce::ValueTree tree("partPins");
    map.writeTo(tree);

    PartMapping::Map restored;
    restored.readFrom(tree);
    REQUIRE(restored.isSuppressed(Part::DRUMS));
    REQUIRE(restored.pinnedTrack(Part::GUITAR) == 1);
    REQUIRE(restored.resolve({ track(2, "PART DRUMS") }).empty());
}

TEST_CASE("PartMapping - a pin to a track that is gone resolves to nothing", "[partmapping]")
{
    PartMapping::Map map;
    map.pin(Part::RHYTHM, 9);

    // Nothing lights up for a part whose track is not in the project any more, which is what
    // otherwise showed a column as mapped with no row under it.
    REQUIRE(map.resolve({ track(0, "Lead Gtr") }).size() == 1);
    REQUIRE(map.resolve({ track(0, "Lead Gtr") })[0].part == Part::GUITAR);

    // The pin is kept, so the mapping comes back with the track.
    REQUIRE(map.pinnedTrack(Part::RHYTHM) == 9);
    const auto back = map.resolve({ track(0, "Lead Gtr"), track(9, "Whatever") });
    REQUIRE(std::any_of(back.begin(), back.end(),
                        [](const auto& a) { return a.part == Part::RHYTHM; }));
}

TEST_CASE("PartMapping - utility tracks are not parts", "[partmapping]")
{
    REQUIRE(PartMapping::isUtilityTrackName("EVENTS"));
    REQUIRE(PartMapping::isUtilityTrackName("BEAT"));
    REQUIRE(PartMapping::isUtilityTrackName("VENUE"));
    REQUIRE(PartMapping::isUtilityTrackName("venue"));

    REQUIRE_FALSE(PartMapping::isUtilityTrackName("PART GUITAR"));
    // Only the whole name, so a track that merely mentions one is still a track.
    REQUIRE_FALSE(PartMapping::isUtilityTrackName("Beat Box Gtr"));

    // Venue is the exception, matched as a word anywhere, because templates prefix it and
    // no instrument is ever called one.
    REQUIRE(PartMapping::isUtilityTrackName("MV VENUE"));
    REQUIRE(PartMapping::isUtilityTrackName("mv_venue"));
}

TEST_CASE("PartMapping - an empty track named for a part still guesses it", "[partmapping]")
{
    PartMapping::Map map;

    // REAPER has no MIDI track type, so a template's empty "PART RHYTHM" has no items on it
    // and still has to be pickable. The caller decides which tracks are offered; resolve
    // reads the name of whatever it is handed.
    const auto out = map.resolve({ track(3, "PART RHYTHM") });
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].part == Part::RHYTHM);
    REQUIRE(out[0].sourceTrackIndex == 3);
}

TEST_CASE("PartMapping - a name only guesses for a track that was offered", "[partmapping]")
{
    PartMapping::Map map;

    // The caller passes only the tracks that could hold a part, so an audio-only track named
    // "rhythm" never reaches resolve and never claims the Rhythm column.
    const auto out = map.resolve({ track(0, "PART GUITAR") });
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].part == Part::GUITAR);

    const auto withRhythm = map.resolve({ track(0, "PART GUITAR"), track(14, "rhythm") });
    REQUIRE(withRhythm.size() == 2);
}

TEST_CASE("PartMapping - common parts are the ones every chart has", "[partmapping]")
{
    REQUIRE(PartMapping::isCommonPart(Part::GUITAR));
    REQUIRE(PartMapping::isCommonPart(Part::BASS));
    REQUIRE(PartMapping::isCommonPart(Part::DRUMS));
    REQUIRE(PartMapping::isCommonPart(Part::KEYS));
    REQUIRE(PartMapping::isCommonPart(Part::VOCALS));

    // Real, but rare enough to sit behind the toggle.
    REQUIRE_FALSE(PartMapping::isCommonPart(Part::RHYTHM));
    REQUIRE_FALSE(PartMapping::isCommonPart(Part::GUITAR_COOP));
    REQUIRE_FALSE(PartMapping::isCommonPart(Part::GHL_GUITAR));
    REQUIRE_FALSE(PartMapping::isCommonPart(Part::ELITE_DRUMS));
}

TEST_CASE("PartMapping - the waveform track saves beside the part pins", "[partmapping]")
{
    PartMapping::Map map;
    REQUIRE_FALSE(map.waveformTrack().has_value());

    map.pinWaveform(4);
    map.pin(Part::GUITAR, 1);

    juce::ValueTree tree("partPins");
    map.writeTo(tree);

    PartMapping::Map restored;
    restored.readFrom(tree);
    REQUIRE(restored.waveformTrack() == 4);
    REQUIRE(restored.pinnedTrack(Part::GUITAR) == 1);

    restored.unpinWaveform();
    REQUIRE_FALSE(restored.waveformTrack().has_value());
}

TEST_CASE("PartMapping - EVENTS is not a column, unmappable parts still are", "[partmapping]")
{
    const auto parts = PartMapping::columnParts();

    REQUIRE(std::find(parts.begin(), parts.end(), Part::EVENTS) == parts.end());
    REQUIRE(std::find(parts.begin(), parts.end(), Part::GUITAR) != parts.end());
    // Shown struck through rather than hidden, so the table reads as the full list.
    REQUIRE(std::find(parts.begin(), parts.end(), Part::GHL_GUITAR) != parts.end());

    // The app's usual part order, so the columns match every other picker.
    for (size_t i = 1; i < parts.size(); ++i)
        REQUIRE(getPartSortOrder(parts[i - 1]) <= getPartSortOrder(parts[i]));
}
