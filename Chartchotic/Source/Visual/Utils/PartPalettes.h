#pragma once

#include <JuceHeader.h>
#include <map>
#include "LaneColours.h"
#include "../../Midi/Discovery/TrackDiscovery.h"
#include "../../UI/ControlConstants.h"

// Which colours each part is drawn with. A part with no entry uses the defaults, so the file
// only ever holds what someone actually changed and a new part needs no migration.
//
// Saved globally rather than in the plugin's `state`: that tree travels with the .rpp, and a
// colour scheme is a property of the person, not of the song.
class PartPalettes
{
public:
    // One store for the whole process. These are the person's colours, not a project's and
    // not an instance's, so two plugin windows open at once are looking at the same thing.
    static PartPalettes& global()
    {
        static PartPalettes instance;
        return instance;
    }

    const LaneColours::Palette& forPart(Part part) const
    {
        const auto it = palettes.find(part);
        return it != palettes.end() ? it->second : fallback;
    }

    bool isCustomised(Part part) const { return palettes.count(part) > 0; }

    // Storing a palette equal to the defaults is the same as having none: it keeps the file
    // from filling up with entries that say nothing.
    void set(Part part, const LaneColours::Palette& p)
    {
        if (p == fallback) palettes.erase(part);
        else               palettes[part] = p;
    }

    void resetToDefault(Part part) { palettes.erase(part); }

    //==========================================================================
    // Persistence. Same directory AssetController already makes for backgrounds.

    static juce::File file()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        return dir.getChildFile("Application Support/Chartchotic/palette.xml");
#else
        return dir.getChildFile("Chartchotic/palette.xml");
#endif
    }

    void load()
    {
        // Written back out even when there was nothing to read. The file is how someone finds
        // their colours to back up or hand to somebody else, and a setting you cannot point at
        // on disk may as well not be a file at all.
        if (!loadFrom(file())) save();
    }

    // A palette file from somewhere else: a share, a backup, a hand-edit. Replaces what is
    // held rather than merging, so what you see is what you were given.
    bool loadFrom(const juce::File& source)
    {
        const auto xml = juce::parseXML(source);
        if (xml == nullptr || !xml->hasTagName("palette")) return false;

        palettes.clear();

        for (auto* row : xml->getChildWithTagNameIterator("part"))
        {
            Part part;
            // Keyed by canonical track name, the way pins are, so the file reads as itself and
            // survives the Part enum gaining a member.
            if (!matchTrackNameToPart(row->getStringAttribute("name").toStdString(), part))
                continue;

            auto p = fallback;
            for (int h = 0; h < LaneColours::hueCount; ++h)
            {
                readLane(*row, "pad" + juce::String(h), p.pad[h]);
                readLane(*row, "cym" + juce::String(h), p.cymbal[h]);
            }
            readLane(*row, "kick", p.kick);
            readLane(*row, "kick2x", p.kick2x);
            readLane(*row, "open", p.open);

            set(part, p);
        }

        return true;
    }

    void save() const
    {
        juce::XmlElement xml("palette");

        for (const auto& [part, p] : palettes)
        {
            auto* row = xml.createNewChildElement("part");
            row->setAttribute("name", trackNameFor(part));

            // Only the lanes that differ from the defaults. A lane written at its default is
            // frozen there, and a later change to that default would never reach this person.
            for (int h = 0; h < LaneColours::hueCount; ++h)
            {
                writeLane(*row, "pad" + juce::String(h), p.pad[h], fallback.pad[h]);
                writeLane(*row, "cym" + juce::String(h), p.cymbal[h], fallback.cymbal[h]);
            }
            writeLane(*row, "kick", p.kick, fallback.kick);
            writeLane(*row, "kick2x", p.kick2x, fallback.kick2x);
            writeLane(*row, "open", p.open, fallback.open);
        }

        const auto target = file();
        target.getParentDirectory().createDirectory();
        xml.writeTo(target);
    }

private:
    // Hex, so the file can be read and hand-edited. A malformed or missing pair leaves the
    // default in place rather than painting something black.
    static void readLane(const juce::XmlElement& row, const juce::String& key, LaneColours::Lane& out)
    {
        if (!row.hasAttribute(key)) return;

        const auto pair = juce::StringArray::fromTokens(row.getStringAttribute(key), ",", "");
        if (pair.size() != 2) return;

        out.dark   = (juce::uint32)pair[0].getHexValue64();
        out.bright = (juce::uint32)pair[1].getHexValue64();
    }

    static void writeLane(juce::XmlElement& row, const juce::String& key,
                          const LaneColours::Lane& l, const LaneColours::Lane& def)
    {
        if (l.dark == def.dark && l.bright == def.bright) return;
        row.setAttribute(key, juce::String::toHexString((juce::int64)l.dark)
                              + "," + juce::String::toHexString((juce::int64)l.bright));
    }

    static constexpr LaneColours::Palette fallback = LaneColours::defaultPalette();

    std::map<Part, LaneColours::Palette> palettes;
};
