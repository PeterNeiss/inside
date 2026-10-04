// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tests for the beman::inside::math double engine (cmath_double.hpp).
//
// The engine is a reproducible libm in `double` (own polynomials, std::fma,
// no <cmath> transcendentals). These tests pin:
//   * accuracy — within a few double-ULP of std:: over sweeps;
//   * exact special values (reproducibility anchors that hold on every IEEE
//     platform: sin0=0, cos0=1, exp0=1, log1=0, sqrt4=2);
//   * end-to-end on `f64` (double-backed) bounds — the value flows in/out with
//     no quantization and no I/O cost.
//
// std:: is used only as the *reference* here (tests), never in the library.

#include <beman/inside/cmath_double.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

// The double engine is the default; under -DBEMAN_INSIDE_MATH_CORDIC the `f64` bounds
// are integer-backed and these double-storage assertions don't apply.
#ifndef BEMAN_INSIDE_MATH_CORDIC

using namespace beman::inside;
namespace d = beman::inside::math::dbl::detail;

namespace
{
  double max_abs(double (*f)(double), double (*ref)(double), double lo, double hi, double step)
  {
    double m = 0;
    for (double x = lo; x <= hi; x += step)
      m = std::max(m, std::fabs(f(x) - ref(x)));
    return m;
  }
}

// dbl: exact special values (IEEE reproducibility anchors)
TEST(CmathDoubleTest, dbl_exact_special_values_ieee_reproducibility_anchors)
{
  ASSERT_EQ(d::fp_sin(0.0), 0.0);
  ASSERT_EQ(d::fp_cos(0.0), 1.0);
  ASSERT_EQ(d::fp_exp(0.0), 1.0);
  ASSERT_EQ(d::fp_log(1.0), 0.0);
  ASSERT_EQ(d::fp_sqrt(4.0), 2.0);
  ASSERT_EQ(d::fp_sqrt(0.0), 0.0);
  ASSERT_EQ(d::fp_atan(0.0), 0.0);
  ASSERT_EQ(d::fp_exp2(0.0), 1.0);
  ASSERT_EQ(d::fp_cbrt(0.0), 0.0);
}

// dbl: accuracy within a few ULP of std::
TEST(CmathDoubleTest, dbl_accuracy_within_a_few_ulp_of_std)
{
  // sin/cos with full range reduction (large arguments stay accurate).
  ASSERT_TRUE((max_abs(d::fp_sin, std::sin, -50.0, 50.0, 7e-4) < 1e-15));
  ASSERT_TRUE((max_abs(d::fp_cos, std::cos, -50.0, 50.0, 7e-4) < 1e-14));
  ASSERT_TRUE((max_abs(d::fp_atan, std::atan, -20.0, 20.0, 7e-4) < 1e-15));
  ASSERT_TRUE((max_abs(d::fp_log, std::log, 1e-3, 1e3, 1e-3) < 1e-14));

  // exp/exp2/pow as relative error.
  double me = 0;
  for (double x = -20; x <= 20; x += 3e-4) me = std::max(me, std::fabs(d::fp_exp(x) - std::exp(x)) / std::exp(x));
  ASSERT_TRUE(me < 1e-14);

  // asin/acos near the full domain.
  double ma = 0;
  for (double x = -0.999; x <= 0.999; x += 3e-4) ma = std::max(ma, std::fabs(d::fp_asin(x) - std::asin(x)));
  ASSERT_TRUE(ma < 1e-14);
}

// dbl: end-to-end on real (double-backed) bounds
TEST(CmathDoubleTest, dbl_end_to_end_on_real_double_backed_bounds)
{
  using ang = inside<{{-8, 8}, notch<1, 65536>}, f64>;
  using amp = inside<{{-1, 1}, notch<1, 65536>}, f64>;
  static_assert(std::is_same_v<ang::raw_type, double>, "f64 inside must be double-backed in the default build");

  // A `f64` inside obeys its grid: the value is stored in double but snapped to
  // the notch (1/65536), so it matches std:: only to ~one notch — and it lands
  // EXACTLY on a grid point (double engine = speed at grid precision, not an
  // escape from the grid).
  ang x = 0.6;
  amp y = math::dbl::sin_into<amp>(x);
  ASSERT_EQ(double(y), 0x1.211ap-1);   // determinism: sin(0.6) snapped to 1/65536 (~0.56476)
  const double scaled = double(y) * 65536.0;
  ASSERT_EQ(scaled, std::trunc(scaled));                         // exact grid point

  // sin(0) round-trips to exactly 0.
  ang z = 0.0;
  ASSERT_EQ(double(math::dbl::sin_into<amp>(z)), 0.0);

  // the input snaps on the way in, too: 0.6 → nearest 1/65536.
  const double xs = double(x) * 65536.0;
  ASSERT_EQ(xs, std::trunc(xs));
}

// dbl: f64-storage arithmetic composes (double, grid-typed)
TEST(CmathDoubleTest, dbl_real_storage_arithmetic_composes_double_grid_typed)
{
  using amp = inside<{{-1, 1}, notch<1, 65536>}, f64>;
  using gn  = inside<{{0, 4},  notch<1, 65536>}, f64>;
  using ang = inside<{{-8, 8}, notch<1, 65536>}, f64>;

  ang ph = 0.6; gn gain = 2.5;
  amp s = math::dbl::sin_into<amp>(ph);

  auto y = gain * s;            // real * real
  auto w = y + s;              // real + real
  auto d = s - amp{0.1};       // real − real (negate + add)

  // results stay double-backed (the `f64` policy propagates through arithmetic)
  static_assert(std::is_same_v<decltype(y)::raw_type, double>);
  static_assert(std::is_same_v<decltype(w)::raw_type, double>);
  static_assert(std::is_same_v<decltype(d)::raw_type, double>);

  // Each operand and result snaps to its grid; pin the exact composed grid values
  // (deterministic across platforms). Ideals: 2.5·sin.6, 3.5·sin.6, sin.6−0.1.
  ASSERT_EQ(double(y), 0x1.69608p+0);   // ~1.41190
  ASSERT_EQ(double(w), 0x1.f9ed8p+0);   // ~1.97644
  ASSERT_EQ(double(d), 0x1.dbccp-2);    // ~0.46484

  ASSERT_TRUE((s > amp{0.5}));     // compares in double, no truncation
  ASSERT_TRUE((s == s));

  // f64 division → double (continuous result grid, double-backed)
  using pos = inside<{{1, 4}, notch<1, 65536>}, f64>;
  pos a3 = 3.0, b2 = 2.0;
  auto q = a3 / b2;
  static_assert(std::is_same_v<decltype(q)::raw_type, double>);
  ASSERT_EQ(double(q), 1.5);
}

// dbl: mixed-sign sqrt returns expected on the double engine
TEST(CmathDoubleTest, dbl_mixed_sign_sqrt_returns_expected_on_the_double_engine)
{
  // Interval crosses zero → the expected-returning overload. A non-negative
  // runtime value yields the root; a negative value surfaces domain_error
  // instead of UB.
  using in  = inside<{{-4, 9}, notch<1, 65536>}, f64>;

  in nine = 9.0;
  auto r = math::sqrt(nine);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(double(*r), 3.0);   // sqrt(9) lands exactly on the grid

  in zero = 0.0;
  auto r0 = math::sqrt(zero);
  ASSERT_TRUE(r0.has_value());
  ASSERT_EQ(double(*r0), 0.0);

  in neg = -1.0;
  auto rn = math::sqrt(neg);
  ASSERT_FALSE(rn.has_value());
  ASSERT_EQ(rn.error(), errc::domain_error);
}

// dbl: circle<M> degree angle uses the double engine
TEST(CmathDoubleTest, dbl_circle_m_degree_angle_uses_the_double_engine)
{
  static_assert(std::is_same_v<math::circle<360>::raw_type, double>, "circle must be double-backed in the default build");
  static_assert(std::is_same_v<math::amp<65536>::raw_type, double>, "amp must be double-backed in the default build");

  math::circle<360> deg = 47.0;
  math::amp<65536> y, c;
  math::sin(deg, y);
  math::cos(deg, c);
  ASSERT_EQ(double(y), 0x1.7674p-1);   // sin(47°) snapped to 1/65536 (~0.73135)
  ASSERT_EQ(double(c), 0x1.5d2ep-1);   // cos(47°) snapped to 1/65536 (~0.68201)

  // exact at cardinal degrees
  math::circle<360> d0 = 0.0, d180 = 180.0;
  math::amp<65536> s0, s180;
  math::sin(d0, s0);  math::sin(d180, s180);
  ASSERT_EQ(double(s0), 0.0);
  ASSERT_EQ(double(s180), 0.0);   // sin(180°) is exactly 0
}

// The algebraic tier (abs/floor/ceil/round/trunc/fmod) is exercised at compile
// time in test_cmath.cpp — but that whole file is `#ifdef BEMAN_INSIDE_MATH_CORDIC`, so on
// the default double engine these functions had NO runtime coverage at all. They
// route a power-of-two-denominator result through `store_grid`, whose integer
// fast path used to mis-store a `f64` (double-backed) result as its grid INDEX
// (e.g. fmod(7,3) came out 147448 instead of 1). This pins the double-engine
// algebraic tier on `f64` bounds against std::.
// dbl: algebraic tier on f64 bounds matches std::
TEST(CmathDoubleTest, dbl_algebraic_tier_on_real_bounds_matches_std)
{
  using in_t  = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f64>;
  using int_t = inside<{{-8, 8}, notch<1>},        round_nearest | f64>;
  using abs_t = inside<{{0, 8},  notch<1, 16384>}, round_nearest | f64>;

  {
    SCOPED_TRACE("fmod keeps the dividend's sign (truncated division)");
    ASSERT_EQ((double(in_t{math::fmod(in_t{7.0},  in_t{3.0})})), (std::fmod(7.0, 3.0)));   // 1
    ASSERT_EQ((double(in_t{math::fmod(in_t{-7.0}, in_t{3.0})})), (std::fmod(-7.0, 3.0)));  // -1
    ASSERT_EQ((double(in_t{math::fmod(in_t{5.5},  in_t{2.0})})), (std::fmod(5.5, 2.0)));   // 1.5
    ASSERT_EQ((double(in_t{math::fmod(in_t{7.0},  in_t{2.5})})), (std::fmod(7.0, 2.5)));   // 2
  }

  {
    SCOPED_TRACE("floor / ceil / round / trunc");
    ASSERT_EQ(double(int_t{math::floor(in_t{1.7})}), std::floor(1.7));   //  1
    ASSERT_EQ(double(int_t{math::floor(in_t{-1.3})}), std::floor(-1.3));  // -2
    ASSERT_EQ(double(int_t{math::ceil(in_t{-1.3})}), std::ceil(-1.3));   // -1
    ASSERT_EQ(double(int_t{math::ceil(in_t{1.2})}), std::ceil(1.2));    //  2
    ASSERT_EQ(double(int_t{math::round(in_t{1.5})}), 2.0);               // half away from 0
    ASSERT_EQ(double(int_t{math::trunc(in_t{-1.7})}), std::trunc(-1.7));  // -1
  }

  {
    SCOPED_TRACE("abs");
    ASSERT_EQ(double(abs_t{math::abs(in_t{-2.5})}), 2.5);
    ASSERT_EQ(double(abs_t{math::abs(in_t{ 2.5})}), 2.5);
    ASSERT_EQ(double(abs_t{math::abs(in_t{ 0.0})}), 0.0);
  }
}

// The transcendental tier (log/exp/asin/.../cbrt) had NO runtime coverage on the
// double engine — its only tests are the `#ifdef BEMAN_INSIDE_MATH_CORDIC` static_asserts
// in test_cmath.cpp. These cross-check the double engine against std:: to ~a
// notch, the same oracle a cross-engine diff would use.
// dbl: transcendental tier on f64 bounds matches std::
TEST(CmathDoubleTest, dbl_transcendental_tier_on_real_bounds_matches_std)
{
  constexpr double tol = 4.0 / 16384;   // a few notches

  {
    SCOPED_TRACE("log / log2 / log10 — strictly positive domain");
    using p = inside<{{1, 16}, notch<1, 16384>}, round_nearest | f64>;
    using o = inside<{{-4, 4}, notch<1, 16384>}, round_nearest | f64>;
    for (double x : {1.0, 1.5, 2.0, std::numbers::e, 8.0, 10.0, 16.0})
    {
      ASSERT_TRUE(std::fabs(double(o{math::log(p{x})})   - std::log(x))   < tol);
      ASSERT_TRUE(std::fabs(double(o{math::log2(p{x})})  - std::log2(x))  < tol);
      ASSERT_TRUE(std::fabs(double(o{math::log10(p{x})}) - std::log10(x)) < tol);
    }
    // Anchors that must be exact-ish on the grid.
    ASSERT_TRUE(std::fabs(double(o{math::log(p{1.0})}))   < tol);   // log 1 = 0
    ASSERT_TRUE(std::fabs(double(o{math::log2(p{8.0})})  - 3.0) < tol);
    ASSERT_TRUE(std::fabs(double(o{math::log10(p{10.0})}) - 1.0) < tol);
  }

  {
    SCOPED_TRACE("exp / exp2");
    using e_in  = inside<{{-2, 2}, notch<1, 16384>}, round_nearest | f64>;
    using e_out = inside<{{0, 8},  notch<1, 16384>}, round_nearest | f64>;
    for (double x : {-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0})
    {
      ASSERT_TRUE(std::fabs(double(e_out{math::exp(e_in{x})})  - std::exp(x))  < tol);
      ASSERT_TRUE(std::fabs(double(e_out{math::exp2(e_in{x})}) - std::exp2(x)) < tol);
    }
  }

  {
    SCOPED_TRACE("asin / acos / atan on their domains");
    using u = inside<{{-1, 1}, notch<1, 16384>}, round_nearest | f64>;
    using o = inside<{{-4, 4}, notch<1, 16384>}, round_nearest | f64>;   // acos(-0.9) ≈ 2.69
    for (double x : {-0.9, -0.5, 0.0, 0.25, 0.5, 0.9})
    {
      ASSERT_TRUE(std::fabs(double(o{math::asin(u{x})}) - std::asin(x)) < tol);
      ASSERT_TRUE(std::fabs(double(o{math::acos(u{x})}) - std::acos(x)) < tol);
      ASSERT_TRUE(std::fabs(double(o{math::atan(o{x})}) - std::atan(x)) < tol);
    }
  }

  {
    SCOPED_TRACE("sinh / cosh / tanh / cbrt");
    using s_in  = inside<{{-2, 2}, notch<1, 16384>}, round_nearest | f64>;
    using s_out = inside<{{-4, 4}, notch<1, 16384>}, round_nearest | f64>;
    using c_in  = inside<{{1, 8},  notch<1, 16384>}, round_nearest | f64>;
    for (double x : {-2.0, -1.0, 0.0, 1.0, 2.0})
    {
      ASSERT_TRUE(std::fabs(double(s_out{math::sinh(s_in{x})}) - std::sinh(x)) < tol);
      ASSERT_TRUE(std::fabs(double(s_out{math::cosh(s_in{x})}) - std::cosh(x)) < tol);
      ASSERT_TRUE(std::fabs(double(s_out{math::tanh(s_in{x})}) - std::tanh(x)) < tol);
    }
    for (double x : {1.0, 2.0, 3.375, 8.0})
      ASSERT_TRUE(std::fabs(double(s_out{math::cbrt(c_in{x})}) - std::cbrt(x)) < tol);
  }
}

//---------------------------------------------------------------------------
// 2026-07 regression: full-mantissa engine results must store onto plain
// integer-index snap grids (not just `f64` ones). tan's auto output grid
// has |Lower| ~ 1024, so a full-mantissa double result once overflowed the
// exact 64-bit (rhs - Lower)/Notch store and terminated through the noexcept
// engine; the 128-bit rounded store now lands the correctly rounded slot.
//---------------------------------------------------------------------------
// dbl engine stores full-mantissa results onto integer-index snap grids
TEST(CmathDoubleTest, dbl_engine_stores_full_mantissa_results_onto_integer_index_snap_grids)
{
  using Ang = inside<{{-8, 8}, notch<1, 16384>}, round_nearest>;   // integer-index storage
  constexpr double half_notch = 0.5 / 16384.0;

  for (double x : {1.0, 1.5, -1.5, 0.4636})
  {
    auto t = math::dbl::tan(Ang{x});
    ASSERT_TRUE(t.has_value());
    ASSERT_TRUE(std::fabs(static_cast<double>(*t) - std::tan(x)) <= half_notch * 1.01);
  }
}

#endif // !BEMAN_INSIDE_MATH_CORDIC
