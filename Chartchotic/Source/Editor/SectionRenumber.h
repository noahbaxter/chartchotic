#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Sections named "Verse 1", "Verse 2" count their own kind, so adding or removing one leaves the
// rest out of step. Renumbering puts every such family back in time order from 1.
namespace SectionRenumber
{
    struct Split
    {
        std::string base;
        bool numbered = false;
    };

    // A number only counts when a space sets it off from a word: "Verse 2", not "Verse2" or "2".
    inline Split split(const std::string& name)
    {
        size_t digits = name.size();
        while (digits > 0 && std::isdigit((unsigned char)name[digits - 1])) --digits;
        if (digits == name.size() || digits == 0 || name[digits - 1] != ' ') return { name, false };

        size_t baseEnd = digits - 1;
        while (baseEnd > 0 && name[baseEnd - 1] == ' ') --baseEnd;
        if (baseEnd == 0) return { name, false };

        return { name.substr(0, baseEnd), true };
    }

    inline std::string lowered(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    // `sections` must already be in time order. A family of one is left alone, so a lone
    // "Chorus 2" is not quietly turned into "Chorus 1". `songOf` maps a section to the song it
    // sits in, and each song counts its families from 1 on its own.
    template <typename Section, typename SongOf>
    void renumber(std::vector<Section>& sections, SongOf songOf)
    {
        std::map<std::pair<int, std::string>, std::vector<size_t>> families;
        for (size_t i = 0; i < sections.size(); ++i)
        {
            const auto parts = split(sections[i].name);
            if (parts.numbered)
                families[{ songOf(sections[i]), lowered(parts.base) }].push_back(i);
        }

        for (const auto& family : families)
        {
            if (family.second.size() < 2) continue;

            int next = 1;
            for (size_t i : family.second)
                sections[i].name = split(sections[i].name).base + " " + std::to_string(next++);
        }
    }
}
