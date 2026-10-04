// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// f64 (double-backed) arithmetic vs. the exact rational oracle.
//
// Premise of the `f64` policy: on a dyadic grid every on-grid value is exact in
// IEEE-754 double, so `f64` arithmetic must equal the exact grid arithmetic.
// `dyadic_grid<G>` (the current storage guard) only checks power-of-two
// denominators — it ignores the 53-bit significand. This test exercises the
// invariant directly: decode the f64 result to rational and compare to the
// exact rational result of the same operands.
//
// It SHOULD FAIL on the unfixed build wherever a stored/result value needs more
// than 53 significant bits (mantissa) or a coarser-ULP binade than the notch
// (exponent), and pass once `f64` is only selected on double-exact grids (the
// result silently dropping `f64` and falling back to exact storage).

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

// `f64` (double-backed) storage is elided under the fixed-point engine, so this
// whole file is double-engine only (it asserts raw_type == double).
#ifndef BEMAN_INSIDE_MATH_CORDIC

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  // A stored f64 result must be exactly ±0 or a normal double. Never a
  // NaN/inf/subnormal.
  template <class R>
  void check_bits(const R& r, const char* op)
  {
    if constexpr (f64_raw<R>)
    {
      const double v = r.raw();
      SCOPED_TRACE(::testing::Message() << op << " raw=" << v);
      ASSERT_TRUE(std::isfinite(v));
      ASSERT_TRUE((v == 0.0 || std::isnormal(v)));
    }
  }

  // Compare f64-inside +,-,* against the exact rational oracle on the *stored*
  // operand values (so this isolates arithmetic divergence from input snap).
  template <class A, class B>
  void oracle_check(A a, B b)
  {
    const rational ar = static_cast<rational>(a);
    const rational br = static_cast<rational>(b);

    if constexpr (requires { a + b; })
    {
      auto r = a + b;
      SCOPED_TRACE(::testing::Message() << "+ a=" << to_string(ar) << " b=" << to_string(br));
      ASSERT_TRUE(static_cast<rational>(r) == *(ar + br));
      check_bits(r, "+");
    }
    if constexpr (requires { a - b; })
    {
      auto r = a - b;
      SCOPED_TRACE(::testing::Message() << "- a=" << to_string(ar) << " b=" << to_string(br));
      ASSERT_TRUE(static_cast<rational>(r) == *(ar - br));
      check_bits(r, "-");
    }
    if constexpr (requires { a * b; })
    {
      auto r = a * b;
      SCOPED_TRACE(::testing::Message() << "* a=" << to_string(ar) << " b=" << to_string(br));
      ASSERT_TRUE(static_cast<rational>(r) == *(ar * br));
      check_bits(r, "*");
    }
  }

  // Random on-grid value for a f64 inside: lo + k*notch, k in [0, max_index_v].
  template <class T>
  double on_grid_value(std::mt19937_64& rng)
  {
    const double lo = static_cast<double>(lower_of<T>);
    const double nd = static_cast<double>(notch_of<T>);
    const umax   cnt = max_index_v<T>;
    std::uniform_int_distribution<umax> d(0, cnt);
    return lo + static_cast<double>(d(rng)) * nd;
  }

  template <class A, class B>
  void sweep(std::mt19937_64& rng, int iters)
  {
    // include the endpoints (max-magnitude is the binding case)
    oracle_check<A, B>(A{static_cast<double>(upper_of<A>)}, B{static_cast<double>(upper_of<B>)});
    oracle_check<A, B>(A{static_cast<double>(lower_of<A>)}, B{static_cast<double>(lower_of<B>)});
    for (int i = 0; i < iters; ++i)
      oracle_check<A, B>(A{on_grid_value<A>(rng)}, B{on_grid_value<B>(rng)});
  }
}

//---------------------------------------------------------------------------
// Deterministic regression: a f64 `×` whose product needs a bit below the
// product binade's ULP silently drops it. Both operands are exact (index
// 2^28-1 < 2^53); the product 16 - 2^-23 + 2^-52 rounds off the 2^-52 term
// because |product| ~ 16 has ULP 2^-49. The exact path keeps it.
//---------------------------------------------------------------------------
// f64 * drops bits below the product-binade ULP
TEST(F64ExactTest, f64_drops_bits_below_the_product_binade_ulp)
{
  using U = inside<{{0, 4}, per<(1u << 26)>}, f64>;   // f=26, exact operand
  static_assert(std::is_same_v<U::raw_type, double>);

  const U a = 4.0 - std::ldexp(1.0, -26);
  const rational ar = static_cast<rational>(a);

  auto p = a * a;
  SCOPED_TRACE(::testing::Message() << "a=" << to_string(ar));
  ASSERT_TRUE(static_cast<rational>(p) == *(ar * ar));
}

//---------------------------------------------------------------------------
// Fuzz sweep across dyadic f64 grids spanning double-exact and
// double-inexact cases (the latter via product numerators crossing 2^53).
//---------------------------------------------------------------------------
// f64 +,-,* match the exact rational oracle / double-exact: small notches, modest range (must always hold)
TEST(F64ExactTest, f64_plus_match_the_exact_rational_oracle__double_exact_small_notches_modest_range_must_always_hold)
{
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x9E3779B97F4A7C15ull);

  {
    SCOPED_TRACE("double-exact: small notches, modest range (must always hold)");
    using A = inside<{{-8, 8}, per<65536>}, f64>;
    using B = inside<{{-8, 8}, per<256>}, f64>;
    sweep<A, A>(rng, 2000);
    sweep<A, B>(rng, 2000);
  }

}

// f64 +,-,* match the exact rational oracle / mantissa: product numerator crosses 2^53
TEST(F64ExactTest, f64_plus_match_the_exact_rational_oracle__mantissa_product_numerator_crosses_2_53)
{
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x9E3779B97F4A7C15ull);

  {
    SCOPED_TRACE("mantissa: product numerator crosses 2^53");
    using A = inside<{{0, 4}, per<(1u << 26)>}, f64>;     // f=26
    using B = inside<{{0, 4}, per<(1u << 27)>}, f64>;     // f=27 -> f_prod=53
    sweep<A, A>(rng, 2000);
    sweep<A, B>(rng, 2000);
  }

}

// f64 +,-,* match the exact rational oracle / exponent-coarsening: fine value combined with a large one
TEST(F64ExactTest, f64_plus_match_the_exact_rational_oracle__exponent_coarsening_fine_value_combined_with_a_large_one)
{
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x9E3779B97F4A7C15ull);

  {
    SCOPED_TRACE("exponent-coarsening: fine value combined with a large one");
    // A lives in a high binade (ULP ~2^-12); B carries bits down to 2^-20.
    // A+B / A*B must keep B's sub-ULP bits, but the double op drops them.
    using A = inside<{{0, (umax{1} << 40)}, per<2>}, f64>;   // large, coarse
    using B = inside<{{0, 1}, per<(1u << 20)>}, f64>;        // small, fine
    sweep<A, B>(rng, 2000);
  }

}

// f64 +,-,* match the exact rational oracle / signed grids cross zero (all four multiply quadrants)
TEST(F64ExactTest, f64_plus_match_the_exact_rational_oracle__signed_grids_cross_zero_all_four_multiply_quadrants)
{
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x9E3779B97F4A7C15ull);

  {
    SCOPED_TRACE("signed grids cross zero (all four multiply quadrants)");
    using A = inside<{{-8, 8}, per<1024>}, f64>;
    using B = inside<{{-4, 12}, per<4096>}, f64>;            // asymmetric, crosses 0
    sweep<A, A>(rng, 3000);
    sweep<A, B>(rng, 3000);
    // inexact signed product: drops f64, must stay exact through the quadrants
    using C = inside<{{-4, 4}, per<(1u << 27)>}, f64>;
    sweep<C, C>(rng, 3000);
  }

}

// f64 +,-,* match the exact rational oracle / mixed: f64 operand with a non-f64 one
TEST(F64ExactTest, f64_plus_match_the_exact_rational_oracle__mixed_f64_operand_with_a_non_f64_one)
{
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x9E3779B97F4A7C15ull);

  {
    SCOPED_TRACE("mixed: f64 operand with a non-f64 one");
    using Re  = inside<{{-8, 8}, per<1024>}, f64>;
    using Int = inside<{-5, 5}>;                       // integer-direct storage
    using Fr  = inside<{{-8, 8}, per<4>}>;        // fractional notch-offset storage
    using Ex  = inside<{{-8, 8}, per<1024>}, exact>;   // rational storage
    sweep<Re, Int>(rng, 3000);
    sweep<Int, Re>(rng, 3000);
    sweep<Re, Fr>(rng, 3000);
    sweep<Re, Ex>(rng, 3000);
  }
}

namespace
{
  // Snapping oracle: assigning an arbitrary (possibly off-grid) value to a
  // double-exact f64 inside must land on the nearest grid point, ties away from
  // zero. std::round is exactly that rule, so it is the trusted reference.
  template <class R>
  void check_snap(double x)
  {
    R r = x;
    const double nd = static_cast<double>(notch_of<R>);
    const double expect = std::round(x / nd) * nd;   // value index, ties away from zero
    SCOPED_TRACE(::testing::Message() << "x=" << x << " expect=" << expect << " got=" << static_cast<double>(r));
    ASSERT_TRUE(static_cast<double>(r) == expect);
  }
}

//---------------------------------------------------------------------------
// snap_double rounding: half away from zero, no predecessor-of-0.5 error, on
// both signs. Exercises exact ties (k+0.5 notches) and random off-grid inputs.
//---------------------------------------------------------------------------
// f64 assignment snaps to nearest grid, ties away from zero
TEST(F64ExactTest, f64_assignment_snaps_to_nearest_grid_ties_away_from_zero)
{
  using R = inside<{{-4, 4}, per<256>}, f64>;    // double-exact, crosses zero
  const double nd = static_cast<double>(notch_of<R>);

  // exact half-way ties on both sides of zero
  for (int k = -1000; k < 1000; ++k)
  {
    const double mid = (k + 0.5) * nd;                // halfway between k·nd and (k+1)·nd
    if (mid > -4.0 && mid < 4.0) check_snap<R>(mid);
  }

  // random off-grid inputs
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0xD1B54A32D192ED03ull);
  std::uniform_real_distribution<double> d(-3.999, 3.999);
  for (int i = 0; i < 20000; ++i) check_snap<R>(d(rng));

  // the classic floor(x+0.5) trap: x just below a tie must round to the lower
  // grid point, not jump up.
  const double justBelow = nd * (3.0 + std::nextafter(0.5, 0.0));
  check_snap<R>(justBelow);
}

//---------------------------------------------------------------------------
// Composition: chained f64 arithmetic must equal the exact rational result
// (catches divergence/storage faults that only appear after a f64 result is
// fed back into another op, possibly after `f64` was dropped).
//---------------------------------------------------------------------------
// chained f64 arithmetic stays exact vs the rational oracle
TEST(F64ExactTest, chained_f64_arithmetic_stays_exact_vs_the_rational_oracle)
{
  using A = inside<{{-4, 4}, per<4096>}, f64>;
  std::mt19937_64 rng(static_cast<unsigned>(::testing::UnitTest::GetInstance()->random_seed()) ^ 0x243F6A8885A308D3ull);

  auto val = [&](){
    const double nd = static_cast<double>(notch_of<A>);
    std::uniform_int_distribution<int> d(-4 * 4096, 4 * 4096);
    return A{ d(rng) * nd };
  };

  for (int i = 0; i < 5000; ++i)
  {
    A a = val(), b = val(), c = val();
    const rational ar = static_cast<rational>(a);
    const rational br = static_cast<rational>(b);
    const rational cr = static_cast<rational>(c);

    SCOPED_TRACE(::testing::Message() << "a=" << to_string(ar) << " b=" << to_string(br) << " c=" << to_string(cr));
    ASSERT_TRUE(static_cast<rational>((a * b) + c) == *(*(ar * br) + cr));
    ASSERT_TRUE(static_cast<rational>((a + b) * c) == *(*(ar + br) * cr));
    ASSERT_TRUE(static_cast<rational>((a - b) * c) == *(*(ar - br) * cr));
  }
}

//---------------------------------------------------------------------------
// A f64 inside's raw is never a NaN/inf/subnormal.
//---------------------------------------------------------------------------
// f64 raw stays clean
TEST(F64ExactTest, f64_raw_stays_clean)
{
  using R = inside<{{-4, 4}, per<1024>}, f64>;
  static_assert(std::is_same_v<R::raw_type, double>);

  R v = 1.5;
  ASSERT_TRUE(std::isnormal(v.raw()));
}

//---------------------------------------------------------------------------
// Real division by zero flows through the error vocabulary (like the integer/
// rational path), instead of silently storing inf. The return type widens to
// expected<result, errc> exactly when the divisor grid can be zero.
//---------------------------------------------------------------------------
// f64 division by zero is reported, not stored as inf
TEST(F64ExactTest, f64_division_by_zero_is_reported_not_stored_as_inf)
{
  using N  = inside<{{1, 4}, per<1024>}, f64>;
  using Dz = inside<{{0, 4}, per<1024>}, f64>;   // divisor grid spans zero

  // divisor can be zero -> return widens to expected; zero divisor -> error
  auto q = N{3.0} / Dz{0.0};
  ASSERT_FALSE(q.has_value());
  ASSERT_EQ(q.error(), errc::division_by_zero);
  ASSERT_TRUE((N{3.0} / Dz{2.0}).has_value());            // nonzero divisor: value present

  // divisor excludes zero -> plain (non-expected) result; double() compiles only
  // because it is an inside, not an expected
  auto p = N{3.0} / N{2.0};
  ASSERT_TRUE(static_cast<double>(p) == 1.5);

  // expected lift surfaces the error code
  auto en = []() -> std::expected<N, errc> { return N{3.0}; };
  auto z = en() / Dz{0.0};
  ASSERT_FALSE(z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}

//---------------------------------------------------------------------------
// An over-fine f64 product (grid slot count > 2^64) deduces a wide integer
// index — exact, total (no expected), no cryptic compile error.
//---------------------------------------------------------------------------
TEST(F64ExactTest, over_fine_f64_product_deduces_wide_index_stays_exact)
{
  using A = inside<{{0, (1u << 17)}, per<(1u << 16)>}, f64>;   // N up to 2^33 < 2^53
  static_assert(std::is_same_v<A::raw_type, double>);

  // product grid {0, 2^34} notch 2^-32 → 2^66 slots → a 67-bit index in a
  // two-limb wide_int. 2^17 * 2^17 = 2^34 is exact.
  A a = static_cast<double>(1u << 17);
  auto p = a * a;
  static_assert(std::is_same_v<decltype(p)::raw_type, detail::wide_uint<2>>);
  ASSERT_TRUE(p == (umax{1} << 34));
  ASSERT_TRUE(p.to<double>().value() == 0x1p34);
}

//---------------------------------------------------------------------------
// A f64 (double-backed) target rejects non-finite assignments: NaN/inf can
// never be an on-grid value, so the store guard reports errc::not_finite rather
// than poisoning the raw double. Exercises the non-finite branch in
// store_checked for fp_raw storage.
//---------------------------------------------------------------------------
// f64 storage rejects non-finite assignment
TEST(F64ExactTest, f64_storage_rejects_non_finite_assignment)
{
  using R = inside<{{-8, 8}, per<65536>}, f64>;
  static_assert(std::is_same_v<R::raw_type, double>);

  auto threw_not_finite = [](auto&& fn) {
    try { fn(); return false; }
    catch (beman::inside::inside_error const& e) { return e.Code == errc::not_finite; }
  };

  ASSERT_TRUE(threw_not_finite([]{ R x = std::nan(""); (void)x; }));
  ASSERT_TRUE(threw_not_finite([]{ R x = std::numeric_limits<double>::infinity(); (void)x; }));
  ASSERT_TRUE(threw_not_finite([]{ R x = -std::numeric_limits<double>::infinity(); (void)x; }));
}

#endif // !BEMAN_INSIDE_MATH_CORDIC
