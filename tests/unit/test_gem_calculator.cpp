#include "test_helpers.h"
#include "Midi/Utils/GemCalculator.h"

// ============================================================================
// resolveGuitarGem — pure function, no locks

TEST_CASE("resolveGuitarGem - modifier priority", "[gem_calculator][guitar]")
{
    SECTION("strum forced → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, true, false) == Gem::NOTE);
    }

    SECTION("HOPO forced → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, true, false, false) == Gem::HOPO_GHOST);
    }

    SECTION("TAP forced → TAP_ACCENT")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, true) == Gem::TAP_ACCENT);
    }

    SECTION("strum + HOPO both forced → NOTE (strum wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, true, true, false) == Gem::NOTE);
    }

    SECTION("strum + TAP both forced → NOTE (strum wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, true, true) == Gem::NOTE);
    }

    SECTION("no modifiers → NOTE (default)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, false) == Gem::NOTE);
    }
}

TEST_CASE("resolveGuitarGem - chord always strum", "[gem_calculator][guitar][chord]")
{
    SECTION("chord, no modifiers → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, false, false, false, false) == Gem::NOTE);
    }

    SECTION("chord + autoHOPO → NOTE (chord wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, true, false, false, false) == Gem::NOTE);
    }

    SECTION("chord + forced HOPO → HOPO_GHOST (forced wins over chord)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, false, true, false, false) == Gem::HOPO_GHOST);
    }
}

TEST_CASE("resolveGuitarGem - auto-HOPO", "[gem_calculator][guitar][auto_hopo]")
{
    SECTION("autoHOPO true, single note → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, true, false, false, false) == Gem::HOPO_GHOST);
    }

    SECTION("autoHOPO false → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, false) == Gem::NOTE);
    }
}

// ============================================================================
// resolveDrumGem — pure function, no locks

TEST_CASE("resolveDrumGem - cymbal vs tom", "[gem_calculator][drums]")
{
    SECTION("not cymbal, no dynamics → NOTE")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, false, Dynamic::NONE) == Gem::NOTE);
    }

    SECTION("cymbal, no dynamics → CYM")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, false, Dynamic::NONE) == Gem::CYM);
    }

    SECTION("cymbal, dynamics enabled, ghost → CYM_GHOST")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, true, Dynamic::GHOST) == Gem::CYM_GHOST);
    }

    SECTION("cymbal, dynamics enabled, accent → CYM_ACCENT")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, true, Dynamic::ACCENT) == Gem::CYM_ACCENT);
    }

    SECTION("not cymbal, dynamics enabled, ghost → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, true, Dynamic::GHOST) == Gem::HOPO_GHOST);
    }

    SECTION("not cymbal, dynamics enabled, accent → TAP_ACCENT")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, true, Dynamic::ACCENT) == Gem::TAP_ACCENT);
    }

    SECTION("cymbal, dynamics disabled, ghost velocity → CYM (dynamics ignored)")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, false, Dynamic::GHOST) == Gem::CYM);
    }
}
