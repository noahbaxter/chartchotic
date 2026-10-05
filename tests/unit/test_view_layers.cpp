#include <catch2/catch_test_macros.hpp>

#include "Visual/ViewLayers.h"

using L = ViewLayers::Layer;
using S = ViewLayers::Surface;

TEST_CASE("ViewLayers - the table is indexed by its own enum", "[view]")
{
    for (int i = 0; i < ViewLayers::layerCount; ++i)
        REQUIRE((int)ViewLayers::table[(size_t)i].id == i);
}

TEST_CASE("ViewOptions - a layer is never on where it does not exist", "[view]")
{
    auto v = ViewOptions::defaults();
    v.set(L::Sustains, S::Strip, true);
    v.set(L::Strikeline, S::Strip, true);

    REQUIRE_FALSE(v.has(L::Sustains, S::Strip));
    REQUIRE_FALSE(v.has(L::Strikeline, S::Strip));
}

TEST_CASE("ViewOptions - defaults come from the table", "[view]")
{
    const auto v = ViewOptions::defaults();

    REQUIRE(v.has(L::Gems, S::Highway));
    REQUIRE(v.has(L::Gems, S::Strip));
    REQUIRE(v.has(L::Fills, S::Highway) == DEFAULT_SHOW_FILLS);
}

TEST_CASE("ViewOptions - a 1.3 project migrates from its flag keys", "[view]")
{
    juce::ValueTree state("state");
    state.setProperty("showGems", false, nullptr);
    state.setProperty("showStrikeline", false, nullptr);

    const auto v = ViewOptions::fromState(state);

    REQUIRE_FALSE(v.has(L::Gems, S::Highway));
    REQUIRE_FALSE(v.has(L::Gems, S::Strip));
    REQUIRE_FALSE(v.has(L::Strikeline, S::Highway));
    REQUIRE(v.has(L::Bars, S::Highway));
}

TEST_CASE("ViewOptions - a flag an old project never saved takes the default, not on", "[view]")
{
    // Fills used to come back on for any project saved before showFills existed.
    const auto v = ViewOptions::fromState(juce::ValueTree("state"));
    REQUIRE(v.has(L::Fills, S::Highway) == DEFAULT_SHOW_FILLS);
}

TEST_CASE("ViewOptions - round trips through state, surfaces kept apart", "[view]")
{
    auto v = ViewOptions::defaults();
    v.set(L::Fills, S::Highway, false);
    v.set(L::Fills, S::Strip, true);
    v.set(L::Bars, S::Highway, false);
    v.set(L::MeasureNumbers, S::Strip, false);

    juce::ValueTree state("state");
    v.writeTo(state);

    REQUIRE(ViewOptions::fromState(state) == v);
    // The legacy key follows the highway, for renderers and older builds that still read it.
    REQUIRE_FALSE((bool)state["showFills"]);
    REQUIRE_FALSE((bool)state["showBars"]);
}

TEST_CASE("ViewOptions - the view child wins over stale legacy keys", "[view]")
{
    auto v = ViewOptions::defaults();
    v.set(L::Gems, S::Highway, false);

    juce::ValueTree state("state");
    v.writeTo(state);
    state.setProperty("showGems", true, nullptr);

    REQUIRE_FALSE(ViewOptions::fromState(state).has(L::Gems, S::Highway));
}
