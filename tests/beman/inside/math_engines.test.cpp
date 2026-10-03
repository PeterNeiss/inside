// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Phase-3: explicit engine namespaces. `beman::inside::math::cordic::fn` (integer/CORDIC,
// always present) and `beman::inside::math::dbl::fn` (double engine, present unless
// BEMAN_INSIDE_MATH_NO_FP) expose the same public-shaped API as `beman::inside::math::fn` and are
// callable SIDE-BY-SIDE in one binary. The unqualified name aliases the build's
// default engine. This TU instantiates both so the wrappers actually compile.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  // A double-backed real grid (works under both engines: `real` ⊃ snap; under
  // BEMAN_INSIDE_MATH_FIXED it is an ordinary round_nearest integer-backed inside).
  using Ang = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using Pos = inside<{{1, 1000}, notch<1, 16384>}, round_nearest | real>;
  using Sq  = inside<{{0, 16}, notch<1, 16384>}, round_nearest | real>;   // sqrt needs Lower 0
}

// cordic engine is always callable and exact on special values
TEST(MathEnginesTest, cordic_engine_is_always_callable_and_exact_on_special_values)
{
  ASSERT_EQ(rational{math::cordic::sin(Ang{0})}, 0);
  ASSERT_EQ(rational{math::cordic::cos(Ang{0})}, 1);
  ASSERT_EQ(rational{math::cordic::atan(Ang{0})}, 0);
  ASSERT_EQ(rational{math::cordic::sinh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::cordic::cosh(Ang{0})}, 1);
  ASSERT_EQ(rational{math::cordic::tanh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::cordic::sqrt(Sq{4})}, 2);
  ASSERT_EQ(rational{math::cordic::cbrt(Ang{8})}, 2);
  ASSERT_EQ(rational{math::cordic::log10(Pos{100})}, 2);
  ASSERT_EQ(rational{math::cordic::exp(Ang{0})}, 1);

  // expected-returning ops
  auto p = math::cordic::pow(Pos{2}, Ang{4});
  ASSERT_TRUE(p.has_value());
  ASSERT_EQ(rational{*p}, 16);

  // constexpr: the integer engine evaluates at compile time
  constexpr auto cs = math::cordic::sin(Ang{0});
  static_assert(rational{cs} == 0);
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
// double engine is callable side-by-side and agrees on special values
TEST(MathEnginesTest, double_engine_is_callable_side_by_side_and_agrees_on_special_values)
{
  ASSERT_EQ(rational{math::dbl::sin(Ang{0})}, 0);
  ASSERT_EQ(rational{math::dbl::cos(Ang{0})}, 1);
  ASSERT_EQ(rational{math::dbl::atan(Ang{0})}, 0);
  ASSERT_EQ(rational{math::dbl::sinh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::dbl::cosh(Ang{0})}, 1);
  ASSERT_EQ(rational{math::dbl::tanh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::dbl::sqrt(Sq{4})}, 2);
  ASSERT_EQ(rational{math::dbl::cbrt(Ang{8})}, 2);
  ASSERT_EQ(rational{math::dbl::log10(Pos{100})}, 2);
  ASSERT_EQ(rational{math::dbl::exp(Ang{0})}, 1);

  auto p = math::dbl::pow(Pos{2}, Ang{4});
  ASSERT_TRUE(p.has_value());
  ASSERT_EQ(rational{*p}, 16);
}

// float engine is callable side-by-side and agrees on special values
TEST(MathEnginesTest, float_engine_is_callable_side_by_side_and_agrees_on_special_values)
{
  ASSERT_EQ(rational{math::flt::sin(Ang{0})}, 0);
  ASSERT_EQ(rational{math::flt::cos(Ang{0})}, 1);
  ASSERT_EQ(rational{math::flt::atan(Ang{0})}, 0);
  ASSERT_EQ(rational{math::flt::sinh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::flt::cosh(Ang{0})}, 1);
  ASSERT_EQ(rational{math::flt::tanh(Ang{0})}, 0);
  ASSERT_EQ(rational{math::flt::sqrt(Sq{4})}, 2);
  ASSERT_EQ(rational{math::flt::cbrt(Ang{8})}, 2);
  ASSERT_EQ(rational{math::flt::log10(Pos{100})}, 2);
  ASSERT_EQ(rational{math::flt::exp(Ang{0})}, 1);

  auto p = math::flt::pow(Pos{2}, Ang{4});
  ASSERT_TRUE(p.has_value());
  ASSERT_EQ(rational{*p}, 16);
}

// Golden pins for the float engine: the EXACT grid-snapped rational the binary32
// engine must produce, bit-for-bit, on every IEEE-754 binary32 platform. These
// are a THIRD value set (float ≠ double ≠ cordic); regenerate only on a
// deliberate engine change. Grid: notch 1/16384 real (Ang/Pos/Sq above).
#define EXACT_FLT(expr, N, D) ASSERT_EQ(rational{(expr)}, (rational{N, D}))

// float engine golden pins are bit-exact (determinism)
TEST(MathEnginesTest, float_engine_golden_pins_are_bit_exact_determinism)
{
  EXACT_FLT(math::flt::sin(Ang{1}),    13787, 16384);
  EXACT_FLT(math::flt::cos(Ang{1}),     2213,  4096);
  EXACT_FLT(math::flt::atan(Ang{1}),    3217,  4096);
  EXACT_FLT(math::flt::exp(Ang{2}),    60531,  8192);
  EXACT_FLT(math::flt::log(Pos{10}),   18863,  8192);
  EXACT_FLT(math::flt::log10(Pos{50}),  6959,  4096);
  EXACT_FLT(math::flt::sqrt(Sq{2}),    11585,  8192);
  EXACT_FLT(math::flt::cbrt(Ang{2}),   20643, 16384);
  EXACT_FLT(math::flt::sinh(Ang{2}),   29711,  8192);
  EXACT_FLT(math::flt::tanh(Ang{1}),    6239,  8192);
}

// all three engines coexist in one binary and meet at exact points
TEST(MathEnginesTest, all_three_engines_coexist_in_one_binary_and_meet_at_exact_points)
{
  // Phase-4 property: cordic + dbl + flt all instantiated in the same TU.
  ASSERT_EQ(rational{math::flt::sqrt(Sq{4})}, rational{math::dbl::sqrt(Sq{4})});
  ASSERT_EQ(rational{math::flt::cos(Ang{0})}, rational{math::cordic::cos(Ang{0})});
  // A pole errors through the expected channel under the float engine too.
  using TanAng = inside<{{-2, 2}, notch<1, 4096>}, round_nearest | real>;
  auto tf = math::flt::tan(TanAng{0});
  ASSERT_TRUE(tf.has_value());
  ASSERT_EQ(rational{*tf}, 0);
}

// both engines coexist in one binary and meet at exact points
TEST(MathEnginesTest, both_engines_coexist_in_one_binary_and_meet_at_exact_points)
{
  // The defining property of Phase 3: both engines instantiated in the same TU.
  // On algebraically-exact inputs they land on the identical grid value.
  ASSERT_EQ(rational{math::cordic::sqrt(Sq{4})}, rational{math::dbl::sqrt(Sq{4})});
  ASSERT_EQ(rational{math::cordic::cos(Ang{0})}, rational{math::dbl::cos(Ang{0})});

  // A pole still errors through the expected channel under both engines.
  using TanAng = inside<{{-2, 2}, notch<1, 4096>}, round_nearest | real>;
  auto tc = math::cordic::tan(TanAng{0});
  auto td = math::dbl::tan(TanAng{0});
  ASSERT_TRUE(tc.has_value());
  ASSERT_TRUE(td.has_value());
  ASSERT_EQ(rational{*tc}, 0);
  ASSERT_EQ(rational{*td}, 0);
}
#endif // !BEMAN_INSIDE_MATH_NO_FP

// unqualified name aliases the build's default engine
TEST(MathEnginesTest, unqualified_name_aliases_the_build_s_default_engine)
{
  // beman::inside::math::sin must equal the selected engine bit-for-bit.
#if defined(BEMAN_INSIDE_MATH_NO_FP)
  ASSERT_EQ(rational{math::sin(Ang{1})}, rational{math::cordic::sin(Ang{1})});
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
  ASSERT_EQ(rational{math::sin(Ang{1})}, rational{math::flt::sin(Ang{1})});
#else
  ASSERT_EQ(rational{math::sin(Ang{1})}, rational{math::dbl::sin(Ang{1})});
#endif
}
