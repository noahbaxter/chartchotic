/*
    ==============================================================================

        EventsTrackText.h
        Author: Noah Baxter

        What a text event on the EVENTS track actually is.

        The track carries far more than song structure: crowd cues, camera
        shots, lighting, hand maps, drum mixes. Matching a prefix at each place
        that cares is how `[chorus]` and `[verse]` — which are singalong cues,
        not sections — end up one typo away from being treated as structure.
        So every event is classified once, here, and each caller takes the kinds
        it wants.

        Vocabulary checked against the charts in assets/midi (official RB2 and
        community charts); see test_events_track_text.cpp.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace EventsTrackText
{
    enum class Kind
    {
        Section,      // [section x] [section_x] [prc_x]   value: the name
        End,          // [end]
        MusicStart,   // [music_start]
        MusicEnd,     // [music_end]
        Coda,         // [coda]
        Crowd,        // [crowd_normal] and friends
        Venue,        // lighting, camera, fog, singalong cues, bonusfx
        HandMap,      // [map HandMap_*] [map StrumMap_*]
        DirectedCut,  // [do_directed_cut *]
        Mix,          // [mix 3 drums0]
        Trainer,      // [begin_pg song_trainer_pg_1] [end_pg ...] [end_key ...]
        ChartFlag,    // [ENABLE_CHART_DYNAMICS]
        Unknown
    };

    struct Event
    {
        Kind kind = Kind::Unknown;
        juce::String value;   // section name for Section, the body otherwise
    };

    // Venue and lighting cues that are a bare word. `chorus` and `verse` live here: they
    // are singalong cues, and the whole reason this is a list rather than a prefix test.
    inline bool isBareVenueCue(const juce::String& body)
    {
        static const juce::StringArray cues {
            "play", "play_solo", "next", "idle", "idle_realtime", "idle_intense",
            "intense", "mellow", "chorus", "verse", "bonusfx",
            "FogOn", "FogOff", "verse_chorus"
        };
        return cues.contains(body, true);
    }

    inline Event classify(const juce::String& raw)
    {
        Event out;

        auto text = raw.trim();
        if (!text.startsWith("[") || !text.endsWith("]")) return out;

        // Trimmed inside the brackets too: "[ map StrumMap_Pick]" is real, and turns up in
        // shipped charts.
        auto body = text.substring(1, text.length() - 1).trim();
        if (body.isEmpty()) return out;

        // Exact before prefix, or the Pro Guitar trainer's [end_pg ...] reads as [end].
        if (body == "end")          { out.kind = Kind::End;        return out; }
        if (body == "music_start")  { out.kind = Kind::MusicStart; return out; }
        if (body == "music_end")    { out.kind = Kind::MusicEnd;   return out; }
        if (body == "coda")         { out.kind = Kind::Coda;       return out; }

        auto sectionName = [&](int prefixLen)
        {
            out.value = body.substring(prefixLen).trim();
            out.kind = out.value.isNotEmpty() ? Kind::Section : Kind::Unknown;
            return out;
        };

        if (body.startsWith("prc_"))     return sectionName(4);
        if (body.startsWith("section "))  return sectionName(8);
        if (body.startsWith("section_"))  return sectionName(8);

        out.value = body;

        if (body.startsWith("crowd_"))            { out.kind = Kind::Crowd;       return out; }
        if (body.startsWith("do_directed_cut"))   { out.kind = Kind::DirectedCut; return out; }
        if (body.startsWith("map "))              { out.kind = Kind::HandMap;     return out; }
        if (body.startsWith("mix "))              { out.kind = Kind::Mix;         return out; }
        if (body.startsWith("coop_"))             { out.kind = Kind::Venue;       return out; }
        if (body.startsWith("ProFilm"))           { out.kind = Kind::Venue;       return out; }
        if (body.startsWith("ENABLE_"))           { out.kind = Kind::ChartFlag;   return out; }

        // Trainer sections, per the Pro Guitar / Pro Keys docs.
        if (body.startsWith("begin_") || body.startsWith("end_"))
        {
            if (body.contains("song_trainer_")) { out.kind = Kind::Trainer; return out; }
        }

        if (isBareVenueCue(body)) { out.kind = Kind::Venue; return out; }

        out.kind = Kind::Unknown;
        return out;
    }

    // Only the kinds that say where the song is, for anything reading structure.
    inline bool isStructural(Kind k)
    {
        return k == Kind::Section || k == Kind::End || k == Kind::MusicEnd || k == Kind::Coda;
    }
}
