#include <catch2/catch_test_macros.hpp>

#include "Visual/Art/BarGemArt.h"
#include "Visual/Utils/LaneColours.h"

namespace
{
    // The stop the rest of the tube is described against: the gloss line.
    juce::Colour peakOf(const GemArt::BarRamp& ramp)
    {
        juce::Colour peak;
        float best = -1.0f;
        for (const auto& stop : ramp)
            if (stop.second.getBrightness() > best)
            {
                best = stop.second.getBrightness();
                peak = stop.second;
            }
        return peak;
    }

    // Eight bits per channel, so "the same colour" means the same pixel, give or take the
    // round trip through HSB.
    void requireSameRamp(const GemArt::BarRamp& a, const GemArt::BarRamp& b, int tolerance)
    {
        REQUIRE(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            REQUIRE(std::abs((int)a[i].second.getRed()   - (int)b[i].second.getRed())   <= tolerance);
            REQUIRE(std::abs((int)a[i].second.getGreen() - (int)b[i].second.getGreen()) <= tolerance);
            REQUIRE(std::abs((int)a[i].second.getBlue()  - (int)b[i].second.getBlue())  <= tolerance);
        }
    }
}

// The regression that shipped twice: recolouring a ramp with its own colour has to give the
// ramp back. It did not, because every stop was forced to one hue, which flattened the kick
// bar's orange-rim-to-yellow-gloss tube into a single yellow.
TEST_CASE("BarRamps - recolouring to a ramp's own peak reproduces it", "[barramp]")
{
    for (const auto& ramp : { GemArt::barRampKick(), GemArt::barRampKick2x(), GemArt::barRampOpen() })
        requireSameRamp(GemArt::recolouredRamp(ramp, peakOf(ramp)), ramp, 2);
}

TEST_CASE("BarRamps - the open bar still matches its art", "[barramp]")
{
    // The open bar was never restyled, so its constant is still the ramp's own colour. If it
    // drifts, a default install draws an open note nobody chose.
    REQUIRE(peakOf(GemArt::barRampOpen()).getARGB() == LaneColours::openBar.bright);
}

TEST_CASE("BarRamps - the kicks read as orange, the 2x darker", "[barramp]")
{
    // Deliberately NOT the ramps' own colours: the art bakes are yellow, and these are picked
    // to sit closer to the game. What matters is the relationship, which is the cue telling
    // the two kicks apart on a board where both are full-width bars.
    const auto kick   = LaneColours::bright(LaneColours::kick);
    const auto kick2x = LaneColours::bright(LaneColours::kick2x);

    // Orange: hue in the red-to-yellow arc, nearer the red end than a pure yellow's ~0.14.
    REQUIRE(kick.getHue()   > 0.02f);
    REQUIRE(kick.getHue()   < 0.12f);
    REQUIRE(kick2x.getHue() > 0.02f);
    REQUIRE(kick2x.getHue() < 0.12f);

    // The 2x is the same bar, darker. Both parts of that matter: too far apart in hue and it
    // reads as a different note, the same brightness and it reads as the same one.
    REQUIRE(kick2x.getBrightness() < kick.getBrightness() - 0.1f);
    REQUIRE(std::abs(kick2x.getHue() - kick.getHue()) < 0.05f);
}

TEST_CASE("BarRamps - the open bar is not the purple gem", "[barramp]")
{
    // They were one entry, which made the open bar darker than its art AND tied it to elite's
    // left crash, so recolouring either moved the other.
    REQUIRE(LaneColours::openBar.bright != LaneColours::purple.bright);
    REQUIRE(LaneColours::defaultPalette().open.bright == LaneColours::openBar.bright);
    REQUIRE(LaneColours::defaultPalette().padFor(LaneColours::Hue::Purple).bright
            == LaneColours::purple.bright);
}

TEST_CASE("BarRamps - a recoloured tube keeps its shading, not one flat hue", "[barramp]")
{
    const auto recoloured = GemArt::recolouredRamp(GemArt::barRampKick(),
                                                   LaneColours::bright(LaneColours::blue));

    float minHue = 1.0f, maxHue = 0.0f, minBright = 1.0f, maxBright = 0.0f;
    for (const auto& stop : recoloured)
    {
        minHue    = juce::jmin(minHue, stop.second.getHue());
        maxHue    = juce::jmax(maxHue, stop.second.getHue());
        minBright = juce::jmin(minBright, stop.second.getBrightness());
        maxBright = juce::jmax(maxBright, stop.second.getBrightness());
    }

    // Still a lit cylinder. The tube's shading lives mostly in HUE, not brightness: the stock
    // stops only span about 0.05 of brightness, which is why forcing one hue flattened it so
    // badly while the brightness ramp stayed intact and the bug still looked "lit".
    REQUIRE(maxHue - minHue > 0.01f);
    REQUIRE(maxBright - minBright > 0.02f);
}
