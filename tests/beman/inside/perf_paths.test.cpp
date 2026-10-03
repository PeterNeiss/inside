// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Regression coverage for the perf-driven changes:
//   - Case 1: `needs_runtime_domain_check` gates the runtime range branch
//             in assignment::assign. Verify behaviour is preserved under
//             every policy and action that should still trigger it.
//   - Case 3: native_div_qformat fast path + Q-format operator rational()
//             fast path. Verify numerical exactness vs the rational route.

#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>
#include <stdexcept>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Case 1 — runtime range branch elision under `unsafe`
//---------------------------------------------------------------------------
// unsafe + no action: out-of-range int rhs stores without throwing
TEST(PerfPathsTest, unsafe_plus_no_action_out_of_range_int_rhs_stores_without_throwing)
{
  using L = inside<{0, 100}, unsafe>;
  // The += must not throw, even with an out-of-range value. Storage holds
  // the raw value as-is (UB on read, but that's the `unsafe` contract).
  L b{50};
  // A delta inside with Lower==0 keeps the raw-add fast path (a singleton `200_ins`
  // would widen to a disjoint grid that only clamp/wrap can absorb).
  ASSERT_NO_THROW((void)((b += inside<{0, 200}>{200})));
  // No assertion on the value — `unsafe` doesn't promise meaningful behaviour
  // for out-of-range writes, only that they don't throw.
}

// checked + no action: range check still fires
TEST(PerfPathsTest, checked_plus_no_action_range_check_still_fires)
{
  using L = inside<{0, 100}, checked>;
  L b{50};
  ASSERT_THROW((void)((b += inside<{0, 200}>{200})), beman::inside::inside_error);
}

// clamp policy: range check still fires
TEST(PerfPathsTest, clamp_policy_range_check_still_fires)
{
  using L = inside<{0, 100}, clamp>;
  L b{50};
  b += inside<{0, 200}>{200};
  ASSERT_EQ(b, 100);
}

// wrap policy: range check still fires
TEST(PerfPathsTest, wrap_policy_range_check_still_fires)
{
  using L = inside<{0, 100}, wrap>;
  L b{50};
  b += inside<{0, 200}>{200};
  ASSERT_EQ(b, 48);   // (50 + 200 - 0) mod 101 + 0 = 250 mod 101 = 48
}

//---------------------------------------------------------------------------
// Case 3 — Q-format fast path correctness
//---------------------------------------------------------------------------
// Q-format division: native_div_qformat matches rational arithmetic
TEST(PerfPathsTest, q_format_division_native_div_qformat_matches_rational_arithmetic)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}>;   // Q8.8; the `truncated` call policy
                                            // supplies snap for the native
                                            // path, without unsafe's ignore_zero

  // Spot checks against expected Q-format integer-truncation values.
  auto q1 = div(fp{200}, fp{8}, truncated);
  ASSERT_TRUE(q1.has_value());
  ASSERT_EQ(*q1, 25);

  auto q2 = div(fp{255}, fp{1}, truncated);
  ASSERT_TRUE(q2.has_value());
  ASSERT_EQ(*q2, 255);

  // Non-integer quotient: 200 / 3 ≈ 66.6667. Q-format multiplies before
  // dividing — (51200 * 256) / 768 = 17066 (= floor(66.6667 * 256)) — same
  // as native `(a << 8) / b`. NOT 66 * 256 = 16896 (that would be
  // truncate-then-scale, which loses fractional precision).
  auto q3 = div(fp{200}, fp{3}, truncated);
  ASSERT_TRUE(q3.has_value());
  ASSERT_EQ((*q3).raw(), 17066);

  // Divide by zero produces errc::division_by_zero.
  auto q4 = div(fp{1}, fp{0}, truncated);
  ASSERT_FALSE(q4.has_value());
  ASSERT_EQ(q4.error(), errc::division_by_zero);
}

// Q-format division: result type is Q-format (same notch as L)
TEST(PerfPathsTest, q_format_division_result_type_is_q_format_same_notch_as_l)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
  auto q = div(fp{200}, fp{8}, truncated);
  using R = std::remove_cvref_t<decltype(*q)>;
  static_assert(notch_of<R> == notch_of<fp>);   // same Q-format, not rational-raw
  static_assert(!(rational_raw<R>));
}

//---------------------------------------------------------------------------
// Case 3 — operator rational() fast path correctness
//---------------------------------------------------------------------------
// operator rational() round-trips through Q-format fast path
TEST(PerfPathsTest, operator_rational_round_trips_through_q_format_fast_path)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;

  // Bit-for-bit exactness over a sampling of values.
  for (int v : {0, 1, 50, 127, 254, 255})
  {
    fp b{v};
    rational r = b;
    ASSERT_EQ(r, rational{static_cast<unsigned>(v)});
    // round-trip back to fp.value
    ASSERT_EQ(b, v);
  }
}

// operator rational() handles fractional Q-format value
TEST(PerfPathsTest, operator_rational_handles_fractional_q_format_value)
{
  using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
  // Raw=128 → value 128/256 = 0.5.
  auto b = fp::from_raw(128);
  rational r = b;
  ASSERT_EQ(r, 0.5_r);
}

//---------------------------------------------------------------------------
// Point-operand multiply — `x * just<c>` scales x's lattice (notch N·|c|), so the
// product keeps integer storage and its offset is x's offset (mirrored for c < 0).
//---------------------------------------------------------------------------
TEST(PerfPathsTest, multiply_by_point_keeps_integer_storage)
{
  using U = inside<{0, 200}>;
  using R3 = decltype(U{} * just<3>);
  static_assert(!rational_raw<R3>);
  static_assert(notch_of<R3> == 3 && lower_of<R3> == 0 && upper_of<R3> == 600);
  using RH = decltype(midpoint(U{}, U{}));
  static_assert(!rational_raw<RH>);
  static_assert(notch_of<RH> == rational{1, 2});
}

TEST(PerfPathsTest, multiply_by_point_matches_exact_product)
{
  for (int v : {-100, -7, 0, 1, 99, 100})
  {
    inside<{-100, 100}> x{v};
    EXPECT_EQ(rational{x * just<3>}, rational{3 * v});
    EXPECT_EQ(rational{just<3> * x}, rational{3 * v});
    EXPECT_EQ(rational{x * just<-2>}, rational{-2 * v});
    EXPECT_EQ((rational{x * just<frac<-1, 4>>}), (rational{v} * rational(-1, 4)).value());
  }
  inside<{0, 200}> a{3}, b{4};
  EXPECT_EQ(rational{midpoint(a, b)}, (rational{7, 2}));
}

//---------------------------------------------------------------------------
// Native div/mod narrowed to int32 — boundary values must match the 64-bit result.
//---------------------------------------------------------------------------
TEST(PerfPathsTest, int32_native_div_mod_matches_wide_reference)
{
  constexpr std::int64_t M = 2147483647;   // INT32_MAX; INT32_MIN is excluded
  using A = inside<{-M, M}>;
  using B = inside<{-(M / 2 + 2), M / 2 + 2}>;
  static_assert(std::same_as<native_div_t<A, B>, std::int32_t>);
  for (std::int64_t a : std::initializer_list<std::int64_t>{-M, -M + 1, -7, 0, 7, M - 1, M})
    for (std::int64_t b : std::initializer_list<std::int64_t>{-(M / 2 + 2), -(M / 2 + 1), -3, -1, 1, 3, M / 2 + 1, M / 2 + 2})
      for (round_mode m : {round_mode::trunc, round_mode::floor, round_mode::ceil,
                           round_mode::nearest, round_mode::half_even})
      {
        const std::int64_t q = div_rounded(a, b, m);
        EXPECT_EQ(div_rounded(static_cast<std::int32_t>(a), static_cast<std::int32_t>(b), m), q);
      }
  // q·b exceeds INT32_MAX here (ceil(M / (M/2 + 1)) = 2); mod must still be exact.
  A a{M};
  B b{M / 2 + 1};
  auto r = mod(a, b, make_policy<round_ceil>());
  EXPECT_EQ(rational{*r}, rational{M - 2 * (M / 2 + 1)});
}

//---------------------------------------------------------------------------
// numerator()/denominator() and to_value fast paths agree with the rational view.
//---------------------------------------------------------------------------
TEST(PerfPathsTest, fraction_and_to_value_fast_paths_match_rational)
{
  using Q = inside<{-4, 4, frac<1, 8>}>;            // dyadic, negative Lower
  static_assert(index_raw<Q> && HasQFormatFastPath<Q>);
  for (imax k = 0; k <= 64; ++k)
  {
    Q q = Q::from_raw(static_cast<Q::raw_type>(k));
    const rational r{q};
    const imax num = (r.Denominator < 0) ? -r.Numerator : r.Numerator;
    EXPECT_EQ(q.numerator(), num);
    EXPECT_EQ(q.denominator(), static_cast<imax>(abs_den(r.Denominator)));
    EXPECT_EQ(to_value(q), trunc(r));
  }
  using I = inside<{-30, 90, 3}>;                    // integer notch 3, index storage
  static_assert(index_raw<I> && IsIntegerAligned<I>);
  for (imax v = -30; v <= 90; v += 3)
  {
    I i{v};
    EXPECT_EQ(to_value(i), v);
    EXPECT_EQ(i.numerator(), v);
    EXPECT_EQ(i.denominator(), 1);
    I j; from_value(j, v);
    EXPECT_EQ(j.raw(), i.raw());
  }
}

//---------------------------------------------------------------------------
// Double-backed abs/floor/ceil/round/trunc match the exact rational results.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_FIXED
TEST(PerfPathsTest, fp_algebraic_fast_path_matches_rational)
{
  using X = inside<{{-8, 8}, notch<1, 4>}, round_nearest | f64>;
  static_assert(fp_raw<X>);
  for (int k = -32; k <= 32; ++k)
  {
    const X x = X::from_raw(k / 4.0);
    const rational r{x};
    EXPECT_EQ(rational{math::abs(x)}, abs(r));
    EXPECT_EQ(rational{math::floor(x)}, rational{floor(r)});
    EXPECT_EQ(rational{math::ceil(x)}, rational{ceil(r)});
    EXPECT_EQ(rational{math::round(x)}, rational{round(r)});
    EXPECT_EQ(rational{math::trunc(x)}, rational{trunc(r)});
  }
}
#endif
