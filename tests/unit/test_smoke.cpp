#include <catch2/catch_test_macros.hpp>
#include "Utils/PPQ.h"

TEST_CASE("Catch2 smoke test", "[smoke]")
{
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("PPQ arithmetic", "[ppq]")
{
    PPQ a(1.0);
    PPQ b(2.0);
    PPQ c = a + b;

    REQUIRE(c.toDouble() == 3.0);
    REQUIRE(a < b);
    REQUIRE(b > a);
    REQUIRE(a == PPQ(1.0));
    REQUIRE(a != b);
}
