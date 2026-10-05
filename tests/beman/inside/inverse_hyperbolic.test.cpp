// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// math::asinh / acosh / atanh: shared domains, auto output grids, exact anchors,
// and agreement with <cmath> within one output notch on every engine.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace beman::inside;
using detail::rational;

namespace
{
  using A = inside<{{-64, 64}, per<1024>}, round_nearest | f64>;
  using C = inside<{{1, 64}, per<1024>}, round_nearest | f64>;
  using T = inside<{{-0.9990234375, 0.9990234375}, per<1024>}, round_nearest | f64>;
  constexpr double kNotch = 1.0 / 1024;

  template <class R>
  double value(R const& r) { return detail::as_double(r); }
}

TEST(InverseHyperbolicTest, anchors_are_exact_and_constexpr_on_the_integer_engine)
{
  static_assert(rational{math::asinh(A{0})} == 0);
  static_assert(rational{math::acosh(C{1})} == 0);
  static_assert(rational{math::atanh(T{0})} == 0);
  EXPECT_EQ(rational{math::asinh(A{0})}, 0);
  EXPECT_EQ(rational{math::acosh(C{1})}, 0);
  EXPECT_EQ(rational{math::atanh(T{0})}, 0);
}

TEST(InverseHyperbolicTest, auto_output_grids_hold_the_endpoint_images)
{
  using AR = decltype(math::asinh(A{0}));
  static_assert(lower_of<AR> <= rational{-48523, 10000} && upper_of<AR> >= rational{48523, 10000});  // asinh(64) ≈ 4.8523
  using CR = decltype(math::acosh(C{1}));
  static_assert(lower_of<CR> == 0 && upper_of<CR> >= rational{48520, 10000});                        // acosh(64) ≈ 4.8520
  static_assert(notch_of<CR> == notch_of<C>);
}

TEST(InverseHyperbolicTest, every_engine_matches_cmath_within_one_notch)
{
  for (double x = -64; x <= 64; x += 0.5)
  {
    EXPECT_NEAR(value(math::asinh(A{x})), std::asinh(x), kNotch) << x;
  }
  for (double x = 1; x <= 64; x += 0.25)
  {
    EXPECT_NEAR(value(math::acosh(C{x})), std::acosh(x), kNotch) << x;
  }
  for (double x = -0.9990234375; x <= 0.9990234375; x += 0.0625)
  {
    EXPECT_NEAR(value(math::atanh(T{x})), std::atanh(x), kNotch) << x;
  }
  // Near the poles of atanh the ratio form keeps full precision.
  EXPECT_NEAR(value(math::atanh(T{0.9990234375})), std::atanh(0.9990234375), kNotch);
}

TEST(InverseHyperbolicTest, explicit_output_and_non_f64_grids)
{
  using I = inside<{-20, 20}, round_nearest>;                   // integer grid
  EXPECT_EQ(rational{math::asinh(I{3})}, 2);                     // asinh(3) ≈ 1.818 → 2
  using Out = inside<{{0, 8}, per<256>}, round_nearest>;
  EXPECT_NEAR(value(math::acosh_into<Out>(C{2})), std::acosh(2.0), 1.0 / 256);
}
