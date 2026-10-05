/*
    ==============================================================================

        ViewLayers.h
        Author: Noah Baxter

        What the highways and the utility strip draw. One table defines every
        layer: the View menu builds its rows from it, defaults come from it, and
        state is saved and loaded by walking it. Adding a layer is one row.

    ==============================================================================
*/

#pragma once

#include <array>
#include <bitset>
#include <juce_data_structures/juce_data_structures.h>
#include "Utils/DrawingConstants.h"

namespace ViewLayers
{
    enum class Layer
    {
        Gems, Sustains, Bars, Hits,
        StarPower, Solos, Fills, Activation, Bre, Lanes,
        BeatLines, MeasureNumbers, Sections,
        SectionGates, TimeSigGates, TempoGates, DiscoGates,
        Surface, SideRails, LaneLines, Strikeline,
        Count
    };
    inline constexpr int layerCount = (int)Layer::Count;

    enum class Surface { Highway, Strip };

    enum class Group { Notes, Phrases, Timing, Gates, Board };
    inline constexpr int groupCount = 5;

    struct LayerInfo
    {
        Layer id;
        Group group;
        const char* label;
        const char* key;        // under the `view` child, suffixed .highway / .strip
        bool onHighway, onStrip;
        bool defaultOn;
        // The pre-2.0 flag the highway surface still writes, so renderers that read state
        // directly keep working and a 1.3 project migrates from it. Empty for new layers.
        const char* legacyKey;
    };

    inline constexpr std::array<LayerInfo, layerCount> table {{
        { Layer::Gems,           Group::Notes,    "Gems",               "gems",           true,  true,  true,  "showGems" },
        { Layer::Sustains,       Group::Notes,    "Sustains",           "sustains",       true,  false, true,  "showSustains" },
        { Layer::Bars,           Group::Notes,    "Bars",               "bars",           true,  false, true,  "showBars" },
        { Layer::Hits,           Group::Notes,    "Hit animations",     "hits",           true,  false, true,  "hitIndicators" },
        { Layer::StarPower,      Group::Phrases,  "Star power",         "starPower",      true,  true,  true,  "starPower" },
        { Layer::Solos,          Group::Phrases,  "Solos",              "solos",          true,  true,  true,  "" },
        { Layer::Fills,          Group::Phrases,  "Fills",              "fills",          true,  true,  DEFAULT_SHOW_FILLS, "showFills" },
        { Layer::Activation,     Group::Phrases,  "Activation",         "activation",     true,  false, DEFAULT_SHOW_ACTIVATION, "showActivation" },
        { Layer::Bre,            Group::Phrases,  "BRE",                "bre",            true,  true,  DEFAULT_SHOW_BRE, "showBre" },
        { Layer::Lanes,          Group::Phrases,  "Roll and trill lanes", "lanes",        true,  false, true,  "showLanes" },
        { Layer::BeatLines,      Group::Timing,   "Beat lines",         "beatLines",      true,  true,  true,  "showGridlines" },
        { Layer::MeasureNumbers, Group::Timing,   "Measure numbers",    "measureNumbers", true,  true,  true,  "" },
        { Layer::Sections,       Group::Timing,   "Sections",           "sections",       true,  true,  true,  "showSectionBanner" },
        { Layer::SectionGates,   Group::Gates,    "Sections",           "sectionGates",   true,  false, false, "" },
        { Layer::TimeSigGates,   Group::Gates,    "Time signature",     "timeSigGates",   true,  false, false, "" },
        { Layer::TempoGates,     Group::Gates,    "Tempo",              "tempoGates",     true,  false, false, "" },
        { Layer::DiscoGates,     Group::Gates,    "Disco flip",         "discoGates",     true,  false, true,  "" },
        { Layer::Surface,        Group::Board,    "Surface",            "surface",        true,  false, true,  "showHighway" },
        { Layer::SideRails,      Group::Board,    "Side rails",         "sideRails",      true,  false, true,  "showTrack" },
        { Layer::LaneLines,      Group::Board,    "Lane lines",         "laneLines",      true,  false, true,  "showLaneSeparators" },
        { Layer::Strikeline,     Group::Board,    "Strikeline",         "strikeline",     true,  false, true,  "showStrikeline" },
    }};

    inline constexpr const char* groupLabels[groupCount] = { "Notes", "Phrases", "Timing", "Gates", "Board" };

    inline const LayerInfo& info(Layer l) { return table[(size_t)l]; }

    inline bool applies(Layer l, Surface s)
    {
        return s == Surface::Highway ? info(l).onHighway : info(l).onStrip;
    }

    inline const juce::Identifier viewTreeId { "view" };

    inline juce::String propertyName(Layer l, Surface s)
    {
        return juce::String(info(l).key) + (s == Surface::Highway ? ".highway" : ".strip");
    }
}

// Which layers are on, per surface. A layer that does not exist on a surface is never on there.
class ViewOptions
{
public:
    using Layer = ViewLayers::Layer;
    using Surface = ViewLayers::Surface;

    static ViewOptions defaults()
    {
        ViewOptions v;
        for (const auto& row : ViewLayers::table)
        {
            v.set(row.id, Surface::Highway, row.defaultOn);
            v.set(row.id, Surface::Strip, row.defaultOn);
        }
        return v;
    }

    bool has(Layer l, Surface s) const { return bits[index(l, s)]; }

    void set(Layer l, Surface s, bool on)
    {
        bits[index(l, s)] = on && ViewLayers::applies(l, s);
    }

    bool operator==(const ViewOptions& o) const { return bits == o.bits; }
    bool operator!=(const ViewOptions& o) const { return bits != o.bits; }

    // A project from before the `view` child is read from the old per-flag keys. A key it
    // never saved takes the table default, not "on", which is what used to turn fills on.
    static ViewOptions fromState(const juce::ValueTree& state)
    {
        auto v = defaults();
        const auto tree = state.getChildWithName(ViewLayers::viewTreeId);

        for (const auto& row : ViewLayers::table)
        {
            for (auto s : { Surface::Highway, Surface::Strip })
            {
                if (!ViewLayers::applies(row.id, s)) continue;
                const auto name = ViewLayers::propertyName(row.id, s);

                if (tree.isValid() && tree.hasProperty(name))
                    v.set(row.id, s, (bool)tree[name]);
                else if (!tree.isValid() && juce::String(row.legacyKey).isNotEmpty()
                         && state.hasProperty(row.legacyKey))
                    v.set(row.id, s, (bool)state[row.legacyKey]);
            }
        }
        return v;
    }

    // The legacy keys are written too: several renderers and the interpreter still read them,
    // and an older build opening the project sees the same view.
    void writeTo(juce::ValueTree& state) const
    {
        auto tree = state.getOrCreateChildWithName(ViewLayers::viewTreeId, nullptr);

        for (const auto& row : ViewLayers::table)
        {
            for (auto s : { Surface::Highway, Surface::Strip })
                if (ViewLayers::applies(row.id, s))
                    tree.setProperty(ViewLayers::propertyName(row.id, s), has(row.id, s), nullptr);

            if (juce::String(row.legacyKey).isNotEmpty())
                state.setProperty(row.legacyKey, has(row.id, Surface::Highway), nullptr);
        }
    }

private:
    static size_t index(Layer l, Surface s)
    {
        return (size_t)l * 2 + (s == Surface::Strip ? 1 : 0);
    }

    std::bitset<(size_t)ViewLayers::layerCount * 2> bits;
};
