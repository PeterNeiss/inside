#include "bound/bound.hpp"
#include "bound/rational.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("_r literal: integer forms", "[literal][r-literal]")
{
  STATIC_REQUIRE(5_r == rational{5});
  STATIC_REQUIRE(1'000_r == rational{1000});
  STATIC_REQUIRE(0xff_r == rational{255});
  STATIC_REQUIRE(0xFF_r == rational{255});
  STATIC_REQUIRE(0b1010_r == rational{10});
  STATIC_REQUIRE(0b1'010_r == rational{10});
}

TEST_CASE("_r literal: decimal forms", "[literal][r-literal]")
{
  STATIC_REQUIRE(1.25_r == rational{5, 4});
  STATIC_REQUIRE(.5_r == rational{1, 2});
  STATIC_REQUIRE(0.1_r == rational{1, 10});                  // exact, no double round-trip
  STATIC_REQUIRE(0.01_r == rational{1, 100});
  STATIC_REQUIRE(3.14_r == rational{157, 50});
}

TEST_CASE("_r literal: decimal scientific (e)", "[literal][r-literal]")
{
  STATIC_REQUIRE(1.5e2_r == rational{150});
  STATIC_REQUIRE(2.5e-1_r == rational{1, 4});
  STATIC_REQUIRE(1e3_r == rational{1000});
  STATIC_REQUIRE(1e-3_r == rational{1, 1000});
}

TEST_CASE("_r literal: hex float / binary exponent (p)", "[literal][r-literal][q-format]")
{
  STATIC_REQUIRE(0x1p15_r == rational{32768});
  STATIC_REQUIRE(0x1p-15_r == rational{1, 32768});
  STATIC_REQUIRE(0x3p-4_r == rational{3, 16});
  STATIC_REQUIRE(0x1.8p3_r == rational{12});
  STATIC_REQUIRE(0x1.8p0_r == rational{3, 2});
  STATIC_REQUIRE(0x1p-14_r == rational{1, 16384});           // Q14 notch
}

TEST_CASE("_b literal: produces point bound (just<value>)", "[literal][b-literal]")
{
  constexpr auto five = 5_b;
  STATIC_REQUIRE(Lower<decltype(five)> == 5);
  STATIC_REQUIRE(Upper<decltype(five)> == 5);

  constexpr auto quarter = 0.25_b;
  STATIC_REQUIRE(Lower<decltype(quarter)> == rational{1, 4});
  STATIC_REQUIRE(Upper<decltype(quarter)> == rational{1, 4});

  constexpr auto q14_notch = 0x1p-14_b;
  STATIC_REQUIRE(Lower<decltype(q14_notch)> == rational{1, 16384});
}

TEST_CASE("a_b / b_b ≈ rational{a,b} — value-equivalent (optional-wrapped)", "[literal][b-literal][replacement]")
{
  // Verification §6 from the plan: `bound / bound` is the *checked* division,
  // so the result is `slim::optional<bound>` even when both operands are
  // point bounds. The inner bound has grid {a/b, a/b}, value a/b — so value
  // equality holds, but the type carries an optional wrapper. For a fully
  // unwrapped point bound, write `just<rational{a, b}>` directly, or use the
  // `_r` literal forms (`3_r / 4_r` has the same property at the rational layer).
  constexpr auto three_quarters = 3_b / 4_b;
  STATIC_REQUIRE(three_quarters == rational{3, 4});
  STATIC_REQUIRE(three_quarters.has_value());

  // The inner bound *is* a point bound with the expected grid.
  using inner_t = typename decltype(three_quarters)::value_type;
  STATIC_REQUIRE(Lower<inner_t> == rational{3, 4});
  STATIC_REQUIRE(Upper<inner_t> == rational{3, 4});
}

TEST_CASE("_r and _b agree", "[literal]")
{
  STATIC_REQUIRE(1.25_b == 1.25_r);
  STATIC_REQUIRE(0x1p-14_b == 0x1p-14_r);
  STATIC_REQUIRE(1.5e2_b == 1.5e2_r);
}
