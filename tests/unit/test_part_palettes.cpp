#include <catch2/catch_test_macros.hpp>

#include "Visual/Utils/PartPalettes.h"

namespace
{
    LaneColours::Palette monochrome(juce::uint32 dark, juce::uint32 bright)
    {
        auto p = LaneColours::defaultPalette();
        for (int h = 0; h < LaneColours::hueCount; ++h)
            p.pad[h] = p.cymbal[h] = { dark, bright };
        return p;
    }
}

TEST_CASE("PartPalettes - defaults reproduce the lane constants", "[palette]")
{
    constexpr auto d = LaneColours::defaultPalette();

    REQUIRE(d.padFor(LaneColours::Hue::Red).bright    == LaneColours::red.bright);
    REQUIRE(d.padFor(LaneColours::Hue::Green).dark    == LaneColours::green.dark);
    REQUIRE(d.padFor(LaneColours::Hue::White).bright  == LaneColours::white.bright);
    REQUIRE(d.kick.bright == LaneColours::kick.bright);

    // Cymbals start from the same hues; only an edit separates them.
    REQUIRE(d.cymbalFor(LaneColours::Hue::Yellow).bright == d.padFor(LaneColours::Hue::Yellow).bright);
}

TEST_CASE("PartPalettes - an untouched part reads the defaults", "[palette]")
{
    PartPalettes palettes;

    REQUIRE_FALSE(palettes.isCustomised(Part::GUITAR));
    REQUIRE(palettes.forPart(Part::GUITAR) == LaneColours::defaultPalette());
}

TEST_CASE("PartPalettes - parts hold colours independently", "[palette]")
{
    PartPalettes palettes;
    palettes.set(Part::BASS, monochrome(0xFF202020, 0xFF40E0D0));

    REQUIRE(palettes.forPart(Part::BASS).padFor(LaneColours::Hue::Red).bright == 0xFF40E0D0);
    // Guitar-likes no longer share a palette, so bass going teal leaves guitar alone.
    REQUIRE(palettes.forPart(Part::GUITAR) == LaneColours::defaultPalette());
}

TEST_CASE("PartPalettes - a cymbal recolour leaves the pad alone", "[palette]")
{
    auto p = LaneColours::defaultPalette();
    p.cymbal[(int)LaneColours::Hue::Yellow] = { 0xFF404040, 0xFF00FFAA };

    PartPalettes palettes;
    palettes.set(Part::DRUMS, p);

    const auto& drums = palettes.forPart(Part::DRUMS);
    REQUIRE(drums.cymbalFor(LaneColours::Hue::Yellow).bright == 0xFF00FFAA);
    REQUIRE(drums.padFor(LaneColours::Hue::Yellow).bright == LaneColours::yellow.bright);
}

TEST_CASE("PartPalettes - painting every pad leaves the bars and cymbals alone", "[palette]")
{
    // Three separate groups of entries, and painting one must not drag the others. A bar is
    // not a lane: anything reading the kick off the lane table showed a red kick the moment
    // the toms went red. The cymbals are what is left telling the board apart.
    auto p = LaneColours::defaultPalette();
    for (int h = 0; h < LaneColours::hueCount; ++h)
        p.pad[h] = LaneColours::red;

    REQUIRE(p.kick.bright   == LaneColours::kick.bright);
    REQUIRE(p.kick2x.bright == LaneColours::kick2x.bright);
    REQUIRE(p.open.bright   == LaneColours::openBar.bright);

    REQUIRE(p.cymbalFor(LaneColours::Hue::Yellow).bright == LaneColours::yellow.bright);
    REQUIRE(p.cymbalFor(LaneColours::Hue::Blue).bright   == LaneColours::blue.bright);
    REQUIRE(p.cymbalFor(LaneColours::Hue::Green).bright  == LaneColours::green.bright);
}

TEST_CASE("PartPalettes - setting the defaults back drops the entry", "[palette]")
{
    PartPalettes palettes;
    palettes.set(Part::DRUMS, monochrome(0xFF101010, 0xFFFF00FF));
    REQUIRE(palettes.isCustomised(Part::DRUMS));

    palettes.set(Part::DRUMS, LaneColours::defaultPalette());
    REQUIRE_FALSE(palettes.isCustomised(Part::DRUMS));
}
