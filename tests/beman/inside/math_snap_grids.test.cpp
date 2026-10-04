// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Phase-1: transcendentals are gated on `snap` (rounding permission), not `f64`
// (double storage). This exercises the new capability — `beman::inside::math` on NON-`f64`
// snap grids: integer-index storage and non-dyadic (1/100) grids — using exact
// special values that are bit-exact on both engines and any grid containing them.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// snap-gated transcendentals on non-f64 grids (integer & 1/100)
TEST(MathSnapGridsTest, snap_gated_transcendentals_on_non_f64_grids_integer_1_100)
{
  // round_nearest implies snap but NOT f64 → these are integer/index-stored,
  // not double-backed. Pre-Phase-1 these were a hard `require_real` compile error.
  using Ang = inside<{{-8, 8}, notch<1, 16384>}, round_nearest>;     // integer-index storage
  ASSERT_EQ(rational{math::sin(Ang{0})}, 0);
  ASSERT_EQ(rational{math::cos(Ang{0})}, 1);
  ASSERT_EQ(rational{math::atan(Ang{0})}, 0);

  using Sq = inside<{{0, 16}, notch<1, 100>}, round_nearest>;        // non-dyadic 1/100 grid
  ASSERT_EQ(rational{math::sqrt(Sq{0})}, 0);
  ASSERT_EQ(rational{math::sqrt(Sq{4})}, 2);

  using Lg = inside<{{1, 1000}, notch<1, 100>}, round_nearest>;
  ASSERT_EQ(rational{math::log10(Lg{1})}, 0);
  ASSERT_EQ(rational{math::log10(Lg{100})}, 2);

  using Cb = inside<{{-8, 8}, notch<1, 100>}, round_nearest>;
  ASSERT_EQ(rational{math::cbrt(Cb{0})}, 0);
  ASSERT_EQ(rational{math::cbrt(Cb{8})}, 2);
  ASSERT_EQ(rational{math::cbrt(Cb{-8})}, -2);

  using Hy = inside<{{-10, 10}, notch<1, 100>}, round_nearest>;
  ASSERT_EQ(rational{math::sinh(Hy{0})}, 0);
  ASSERT_EQ(rational{math::cosh(Hy{0})}, 1);
  ASSERT_EQ(rational{math::tanh(Hy{0})}, 0);

  using Ex = inside<{{-4, 4}, notch<1, 100>}, round_nearest>;
  ASSERT_EQ(rational{math::exp(Ex{0})}, 1);

  // pow returns expected; 2^4 snaps exactly onto the 1/100 grid.
  using B = inside<{{1, 16}, notch<1, 100>}, round_nearest>;
  using E = inside<{{-4, 8}, notch<1, 100>}, round_nearest>;
  auto p = math::pow(B{2}, E{4});
  ASSERT_TRUE(p.has_value());
  ASSERT_EQ(rational{*p}, 16);
}
