// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// round_nearest assignment is half-away-from-zero on negatives
TEST(RoundNegativeTiesTest, round_nearest_assignment_is_half_away_from_zero_on_negatives)
{
  using N = inside<{-100, 100}, round_nearest>;
  ASSERT_TRUE((static_cast<rational>(N(rational{ 5, 2})) ==  3));   //  2.5 ->  3
  ASSERT_TRUE((static_cast<rational>(N(rational{-5, 2})) == -3));   // -2.5 -> -3 (was -2)
  ASSERT_TRUE((static_cast<rational>(N(rational{ 3, 2})) ==  2));   //  1.5 ->  2
  ASSERT_TRUE((static_cast<rational>(N(rational{-3, 2})) == -2));   // -1.5 -> -2 (was -1)
  ASSERT_TRUE((static_cast<rational>(N(rational{-1, 2})) == -1));   // -0.5 -> -1 (was  0)
}

// assignment rounding agrees with division on the same value
TEST(RoundNegativeTiesTest, assignment_rounding_agrees_with_division_on_the_same_value)
{
  // -5/2 == -2.5 ; assigning -2.5 and dividing to -2.5 must store the same point.
  using N  = inside<{-100, 100}, round_nearest>;
  using Nd = inside<{1, 10}, round_nearest>;
  ASSERT_TRUE((static_cast<rational>(N{-5} / Nd{2}) == static_cast<rational>(N(rational{-5, 2}))));
  ASSERT_TRUE((static_cast<rational>(N{-7} / Nd{2}) == static_cast<rational>(N(rational{-7, 2}))));
  ASSERT_TRUE((static_cast<rational>(N{ 7} / Nd{2}) == static_cast<rational>(N(rational{ 7, 2}))));
}

// round_half_even assignment ties to even VALUE regardless of Lower parity
TEST(RoundNegativeTiesTest, round_half_even_assignment_ties_to_even_value_regardless_of_lower_parity)
{
  using He = inside<{-10, 10}, round_half_even>;   // Lower even
  using Ho = inside<{ -9,  9}, round_half_even>;   // Lower odd
  // -2.5 -> -2 (even) in BOTH; the historical bug gave -3 for the odd-Lower grid.
  ASSERT_TRUE((static_cast<rational>(He(rational{-5, 2})) == -2));
  ASSERT_TRUE((static_cast<rational>(Ho(rational{-5, 2})) == -2));
  // a few more ties, both signs / both grids.
  ASSERT_TRUE((static_cast<rational>(He(rational{ 5, 2})) ==  2));   //  2.5 ->  2
  ASSERT_TRUE((static_cast<rational>(He(rational{ 7, 2})) ==  4));   //  3.5 ->  4
  ASSERT_TRUE((static_cast<rational>(Ho(rational{ 5, 2})) ==  2));
  ASSERT_TRUE((static_cast<rational>(Ho(rational{-7, 2})) == -4));   // -3.5 -> -4
}

// bare snap assignment truncates toward zero on negatives
TEST(RoundNegativeTiesTest, bare_snap_assignment_truncates_toward_zero_on_negatives)
{
  using S = inside<{{-100, 100}, notch<1, 4>}, snap>;
  // 1/8-off values truncate toward zero (not toward -inf).
  ASSERT_TRUE((static_cast<rational>(S(rational{ 7, 8})) == rational{ 3, 4}));  //  0.875 ->  0.75
  ASSERT_TRUE((static_cast<rational>(S(rational{-7, 8})) == rational{-3, 4}));  // -0.875 -> -0.75 (was -1)
  ASSERT_TRUE((static_cast<rational>(S(rational{-1, 8})) ==  0));               // -0.125 ->  0    (was -0.25)
}

// round_floor / round_ceil assignment stay direction-correct
TEST(RoundNegativeTiesTest, round_floor_round_ceil_assignment_stay_direction_correct)
{
  using F = inside<{-100, 100}, round_floor>;
  using C = inside<{-100, 100}, round_ceil>;
  ASSERT_TRUE((static_cast<rational>(F(rational{-3, 2})) == -2));   // floor(-1.5)
  ASSERT_TRUE((static_cast<rational>(F(rational{ 3, 2})) ==  1));   // floor( 1.5)
  ASSERT_TRUE((static_cast<rational>(C(rational{-3, 2})) == -1));   // ceil(-1.5)
  ASSERT_TRUE((static_cast<rational>(C(rational{ 3, 2})) ==  2));   // ceil( 1.5)
}

// Cross-grid inside->inside conversion has its own rounding path (assignment.hpp
// store()). It used to special-case only round_nearest (offset half-up) and let
// round_ceil / round_half_even fall through to truncation; it now routes through
// the shared round_quotient so every mode rounds in value space.
// cross-grid conversion rounds every mode in value space
TEST(RoundNegativeTiesTest, cross_grid_conversion_rounds_every_mode_in_value_space)
{
  using Src = inside<{{-10, 10}, notch<1, 2>}>;        // half-steps, spans negatives
  using Dn  = inside<{-10, 10}, round_nearest>;
  using Df  = inside<{-10, 10}, round_floor>;
  using Dc  = inside<{-10, 10}, round_ceil>;
  using De  = inside<{-10, 10}, round_half_even>;

  Src s{rational{-5, 2}};                              // -2.5, on the source grid
  ASSERT_TRUE(static_cast<rational>(Dn{s}) == -3);         // half away (was -2)
  ASSERT_TRUE(static_cast<rational>(Df{s}) == -3);         // floor
  ASSERT_TRUE(static_cast<rational>(Dc{s}) == -2);         // ceil  (was -3: truncated)
  ASSERT_TRUE(static_cast<rational>(De{s}) == -2);         // tie -> even (was -3)

  Src s2{rational{-7, 2}};                             // -3.5
  ASSERT_TRUE(static_cast<rational>(De{s2}) == -4);        // tie -> even
  ASSERT_TRUE(static_cast<rational>(Dc{s2}) == -3);        // ceil

  Src p{rational{5, 2}};                               // +2.5 (positives unaffected)
  ASSERT_TRUE(static_cast<rational>(Dn{p}) ==  3);
  ASSERT_TRUE(static_cast<rational>(De{p}) ==  2);
}
