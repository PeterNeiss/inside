// Assignment/conversion rounding of negative values must match the division
// path (detail::div_rounded) and the documented policy semantics.
//
// Regression: round_quotient rounded the NON-NEGATIVE grid offset
// (rhs - Lower)/Notch instead of the signed value, so on grids spanning
// negative values:
//   * round_nearest behaved as round-half-UP, not half-away-from-zero — and
//     disagreed with division (assigning -2.5 gave -2 while x/y == -2.5 gave -3);
//   * round_half_even broke ties to an even OFFSET INDEX, giving an odd VALUE
//     whenever Lower was odd (Lower=-9: -2.5 -> -3 instead of -2);
//   * bare snap truncated toward -inf instead of toward zero.
// The fix rounds in value space, so assignment and division now agree.

#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("round_nearest assignment is half-away-from-zero on negatives", "[round][assign]")
{
  using N = bound<{-100, 100}, round_nearest>;
  REQUIRE(static_cast<rational>(N(rational{ 5, 2})) ==  3);   //  2.5 ->  3
  REQUIRE(static_cast<rational>(N(rational{-5, 2})) == -3);   // -2.5 -> -3 (was -2)
  REQUIRE(static_cast<rational>(N(rational{ 3, 2})) ==  2);   //  1.5 ->  2
  REQUIRE(static_cast<rational>(N(rational{-3, 2})) == -2);   // -1.5 -> -2 (was -1)
  REQUIRE(static_cast<rational>(N(rational{-1, 2})) == -1);   // -0.5 -> -1 (was  0)
}

TEST_CASE("assignment rounding agrees with division on the same value", "[round][assign][div]")
{
  // -5/2 == -2.5 ; assigning -2.5 and dividing to -2.5 must store the same point.
  using N  = bound<{-100, 100}, round_nearest>;
  using Nd = bound<{1, 10}, round_nearest>;
  REQUIRE(static_cast<rational>(N{-5} / Nd{2}) == static_cast<rational>(N(rational{-5, 2})));
  REQUIRE(static_cast<rational>(N{-7} / Nd{2}) == static_cast<rational>(N(rational{-7, 2})));
  REQUIRE(static_cast<rational>(N{ 7} / Nd{2}) == static_cast<rational>(N(rational{ 7, 2})));
}

TEST_CASE("round_half_even assignment ties to even VALUE regardless of Lower parity", "[round][assign]")
{
  using He = bound<{-10, 10}, round_half_even>;   // Lower even
  using Ho = bound<{ -9,  9}, round_half_even>;   // Lower odd
  // -2.5 -> -2 (even) in BOTH; the historical bug gave -3 for the odd-Lower grid.
  REQUIRE(static_cast<rational>(He(rational{-5, 2})) == -2);
  REQUIRE(static_cast<rational>(Ho(rational{-5, 2})) == -2);
  // a few more ties, both signs / both grids.
  REQUIRE(static_cast<rational>(He(rational{ 5, 2})) ==  2);   //  2.5 ->  2
  REQUIRE(static_cast<rational>(He(rational{ 7, 2})) ==  4);   //  3.5 ->  4
  REQUIRE(static_cast<rational>(Ho(rational{ 5, 2})) ==  2);
  REQUIRE(static_cast<rational>(Ho(rational{-7, 2})) == -4);   // -3.5 -> -4
}

TEST_CASE("bare snap assignment truncates toward zero on negatives", "[round][assign]")
{
  using S = bound<{{-100, 100}, notch<1, 4>}, snap>;
  // 1/8-off values truncate toward zero (not toward -inf).
  REQUIRE(static_cast<rational>(S(rational{ 7, 8})) == rational{ 3, 4});  //  0.875 ->  0.75
  REQUIRE(static_cast<rational>(S(rational{-7, 8})) == rational{-3, 4});  // -0.875 -> -0.75 (was -1)
  REQUIRE(static_cast<rational>(S(rational{-1, 8})) ==  0);               // -0.125 ->  0    (was -0.25)
}

TEST_CASE("round_floor / round_ceil assignment stay direction-correct", "[round][assign]")
{
  using F = bound<{-100, 100}, round_floor>;
  using C = bound<{-100, 100}, round_ceil>;
  REQUIRE(static_cast<rational>(F(rational{-3, 2})) == -2);   // floor(-1.5)
  REQUIRE(static_cast<rational>(F(rational{ 3, 2})) ==  1);   // floor( 1.5)
  REQUIRE(static_cast<rational>(C(rational{-3, 2})) == -1);   // ceil(-1.5)
  REQUIRE(static_cast<rational>(C(rational{ 3, 2})) ==  2);   // ceil( 1.5)
}

// Cross-grid bound->bound conversion has its own rounding path (assignment.hpp
// store()). It used to special-case only round_nearest (offset half-up) and let
// round_ceil / round_half_even fall through to truncation; it now routes through
// the shared round_quotient so every mode rounds in value space.
TEST_CASE("cross-grid conversion rounds every mode in value space", "[round][convert]")
{
  using Src = bound<{{-10, 10}, notch<1, 2>}>;        // half-steps, spans negatives
  using Dn  = bound<{-10, 10}, round_nearest>;
  using Df  = bound<{-10, 10}, round_floor>;
  using Dc  = bound<{-10, 10}, round_ceil>;
  using De  = bound<{-10, 10}, round_half_even>;

  Src s{rational{-5, 2}};                              // -2.5, on the source grid
  REQUIRE(static_cast<rational>(Dn{s}) == -3);         // half away (was -2)
  REQUIRE(static_cast<rational>(Df{s}) == -3);         // floor
  REQUIRE(static_cast<rational>(Dc{s}) == -2);         // ceil  (was -3: truncated)
  REQUIRE(static_cast<rational>(De{s}) == -2);         // tie -> even (was -3)

  Src s2{rational{-7, 2}};                             // -3.5
  REQUIRE(static_cast<rational>(De{s2}) == -4);        // tie -> even
  REQUIRE(static_cast<rational>(Dc{s2}) == -3);        // ceil

  Src p{rational{5, 2}};                               // +2.5 (positives unaffected)
  REQUIRE(static_cast<rational>(Dn{p}) ==  3);
  REQUIRE(static_cast<rational>(De{p}) ==  2);
}
