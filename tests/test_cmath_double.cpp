// Tests for the bnd::math double engine (cmath_double.hpp).
//
// The engine is a reproducible libm in `double` (own polynomials, std::fma,
// no <cmath> transcendentals). These tests pin:
//   * accuracy — within a few double-ULP of std:: over sweeps;
//   * exact special values (reproducibility anchors that hold on every IEEE
//     platform: sin0=0, cos0=1, exp0=1, log1=0, sqrt4=2);
//   * end-to-end on `real` (double-backed) bounds — the value flows in/out with
//     no quantization and no I/O cost.
//
// std:: is used only as the *reference* here (tests), never in the library.

#include "bound/cmath_double.hpp"
#include "bound/cmath.hpp"
#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

// The double engine is the default; under -DBOUND_MATH_FIXED the `real` bounds
// are integer-backed and these double-storage assertions don't apply.
#ifndef BND_MATH_FIXED

using namespace bnd;
namespace d = bnd::math::dbl::detail;

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

TEST_CASE("dbl: exact special values (IEEE reproducibility anchors)", "[dbl]")
{
  REQUIRE(d::d_sin(0.0)  == 0.0);
  REQUIRE(d::d_cos(0.0)  == 1.0);
  REQUIRE(d::d_exp(0.0)  == 1.0);
  REQUIRE(d::d_log(1.0)  == 0.0);
  REQUIRE(d::d_sqrt(4.0) == 2.0);
  REQUIRE(d::d_sqrt(0.0) == 0.0);
  REQUIRE(d::d_atan(0.0) == 0.0);
  REQUIRE(d::d_exp2(0.0) == 1.0);
  REQUIRE(d::d_cbrt(0.0) == 0.0);
}

TEST_CASE("dbl: accuracy within a few ULP of std::", "[dbl]")
{
  // sin/cos with full range reduction (large arguments stay accurate).
  REQUIRE(max_abs(d::d_sin, std::sin, -50.0, 50.0, 7e-4) < 1e-15);
  REQUIRE(max_abs(d::d_cos, std::cos, -50.0, 50.0, 7e-4) < 1e-14);
  REQUIRE(max_abs(d::d_atan, std::atan, -20.0, 20.0, 7e-4) < 1e-15);
  REQUIRE(max_abs(d::d_log, std::log, 1e-3, 1e3, 1e-3) < 1e-14);

  // exp/exp2/pow as relative error.
  double me = 0;
  for (double x = -20; x <= 20; x += 3e-4) me = std::max(me, std::fabs(d::d_exp(x) - std::exp(x)) / std::exp(x));
  REQUIRE(me < 1e-14);

  // asin/acos near the full domain.
  double ma = 0;
  for (double x = -0.999; x <= 0.999; x += 3e-4) ma = std::max(ma, std::fabs(d::d_asin(x) - std::asin(x)));
  REQUIRE(ma < 1e-14);
}

TEST_CASE("dbl: end-to-end on real (double-backed) bounds", "[dbl][real]")
{
  using ang = bound<{{-8, 8}, notch<1, 65536>}, real>;
  using amp = bound<{{-1, 1}, notch<1, 65536>}, real>;
  static_assert(std::is_same_v<ang::raw_type, double>, "real bound must be double-backed in the default build");

  // A `real` bound obeys its grid: the value is stored in double but snapped to
  // the notch (1/65536), so it matches std:: only to ~one notch — and it lands
  // EXACTLY on a grid point (double engine = speed at grid precision, not an
  // escape from the grid).
  ang x = 0.6;
  amp y = math::dbl::sin_core<amp>(x);
  REQUIRE(std::fabs(double(y) - std::sin(0.6)) < 2.0 / 65536);   // ~one notch
  const double scaled = double(y) * 65536.0;
  REQUIRE(scaled == std::trunc(scaled));                         // exact grid point

  // sin(0) round-trips to exactly 0.
  ang z = 0.0;
  REQUIRE(double(math::dbl::sin_core<amp>(z)) == 0.0);

  // the input snaps on the way in, too: 0.6 → nearest 1/65536.
  const double xs = double(x) * 65536.0;
  REQUIRE(xs == std::trunc(xs));
}

TEST_CASE("dbl: real-storage arithmetic composes (double, grid-typed)", "[dbl][real]")
{
  using amp = bound<{{-1, 1}, notch<1, 65536>}, real>;
  using gn  = bound<{{0, 4},  notch<1, 65536>}, real>;
  using ang = bound<{{-8, 8}, notch<1, 65536>}, real>;

  ang ph = 0.6; gn gain = 2.5;
  amp s = math::dbl::sin_core<amp>(ph);

  auto y = gain * s;            // real * real
  auto w = y + s;              // real + real
  auto d = s - amp{0.1};       // real − real (negate + add)

  // results stay double-backed (the `real` policy propagates through arithmetic)
  static_assert(std::is_same_v<decltype(y)::raw_type, double>);
  static_assert(std::is_same_v<decltype(w)::raw_type, double>);
  static_assert(std::is_same_v<decltype(d)::raw_type, double>);

  // Each operand and result snaps to its grid, so the composed value tracks the
  // ideal to ~a notch (not full double precision — that's the grid's job).
  double sv = std::sin(0.6);
  REQUIRE(std::fabs(double(y) - 2.5 * sv)       < 1e-4);
  REQUIRE(std::fabs(double(w) - 3.5 * sv)       < 1e-4);
  REQUIRE(std::fabs(double(d) - (sv - 0.1))     < 1e-4);

  REQUIRE((s > amp{0.5}));     // compares in double, no truncation
  REQUIRE((s == s));

  // real division → double (continuous result grid, double-backed)
  using pos = bound<{{1, 4}, notch<1, 65536>}, real>;
  pos a3 = 3.0, b2 = 2.0;
  auto q = a3 / b2;
  static_assert(std::is_same_v<decltype(q)::raw_type, double>);
  REQUIRE(double(q) == 1.5);
}

TEST_CASE("dbl: mixed-sign sqrt returns expected on the double engine", "[dbl][real]")
{
  // Interval crosses zero → the expected-returning overload. A non-negative
  // runtime value yields the root; a negative value surfaces domain_error
  // instead of UB.
  using in  = bound<{{-4, 9}, notch<1, 65536>}, real>;

  in nine = 9.0;
  auto r = math::sqrt(nine);
  REQUIRE(r.has_value());
  REQUIRE(std::fabs(double(*r) - 3.0) < 1e-15);

  in zero = 0.0;
  auto r0 = math::sqrt(zero);
  REQUIRE(r0.has_value());
  REQUIRE(double(*r0) == 0.0);

  in neg = -1.0;
  auto rn = math::sqrt(neg);
  REQUIRE_FALSE(rn.has_value());
  REQUIRE(rn.error() == errc::domain_error);
}

TEST_CASE("dbl: circle<M> degree angle uses the double engine", "[dbl][real][circle]")
{
  static_assert(std::is_same_v<math::circle<360>::raw_type, double>, "circle must be double-backed in the default build");
  static_assert(std::is_same_v<math::amp<65536>::raw_type, double>, "amp must be double-backed in the default build");

  math::circle<360> deg = 47.0;
  math::amp<65536> y, c;
  math::sin(deg, y);
  math::cos(deg, c);
  REQUIRE(std::fabs(double(y) - std::sin(47.0 * std::numbers::pi / 180.0)) < 2.0 / 65536);
  REQUIRE(std::fabs(double(c) - std::cos(47.0 * std::numbers::pi / 180.0)) < 2.0 / 65536);

  // exact at cardinal degrees
  math::circle<360> d0 = 0.0, d180 = 180.0;
  math::amp<65536> s0, s180;
  math::sin(d0, s0);  math::sin(d180, s180);
  REQUIRE(double(s0) == 0.0);
  REQUIRE(std::fabs(double(s180)) < 1e-15);
}

// The algebraic tier (abs/floor/ceil/round/trunc/fmod) is exercised at compile
// time in test_cmath.cpp — but that whole file is `#ifdef BND_MATH_FIXED`, so on
// the default double engine these functions had NO runtime coverage at all. They
// route a power-of-two-denominator result through `store_grid`, whose integer
// fast path used to mis-store a `real` (double-backed) result as its grid INDEX
// (e.g. fmod(7,3) came out 147448 instead of 1). This pins the double-engine
// algebraic tier on `real` bounds against std::.
TEST_CASE("dbl: algebraic tier on real bounds matches std::", "[dbl][real][algebraic]")
{
  using in_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using int_t = bound<{{-8, 8}, notch<1>},        round_nearest | real>;
  using abs_t = bound<{{0, 8},  notch<1, 16384>}, round_nearest | real>;

  SECTION("fmod keeps the dividend's sign (truncated division)")
  {
    REQUIRE(double(in_t{math::fmod(in_t{7.0},  in_t{3.0})}) ==  std::fmod(7.0, 3.0));   // 1
    REQUIRE(double(in_t{math::fmod(in_t{-7.0}, in_t{3.0})}) ==  std::fmod(-7.0, 3.0));  // -1
    REQUIRE(double(in_t{math::fmod(in_t{5.5},  in_t{2.0})}) ==  std::fmod(5.5, 2.0));   // 1.5
    REQUIRE(double(in_t{math::fmod(in_t{7.0},  in_t{2.5})}) ==  std::fmod(7.0, 2.5));   // 2
  }

  SECTION("floor / ceil / round / trunc")
  {
    REQUIRE(double(int_t{math::floor(in_t{1.7})})  == std::floor(1.7));   //  1
    REQUIRE(double(int_t{math::floor(in_t{-1.3})}) == std::floor(-1.3));  // -2
    REQUIRE(double(int_t{math::ceil(in_t{-1.3})})  == std::ceil(-1.3));   // -1
    REQUIRE(double(int_t{math::ceil(in_t{1.2})})   == std::ceil(1.2));    //  2
    REQUIRE(double(int_t{math::round(in_t{1.5})})  == 2.0);               // half away from 0
    REQUIRE(double(int_t{math::trunc(in_t{-1.7})}) == std::trunc(-1.7));  // -1
  }

  SECTION("abs")
  {
    REQUIRE(double(abs_t{math::abs(in_t{-2.5})}) == 2.5);
    REQUIRE(double(abs_t{math::abs(in_t{ 2.5})}) == 2.5);
    REQUIRE(double(abs_t{math::abs(in_t{ 0.0})}) == 0.0);
  }
}

// The transcendental tier (log/exp/asin/.../cbrt) had NO runtime coverage on the
// double engine — its only tests are the `#ifdef BND_MATH_FIXED` static_asserts
// in test_cmath.cpp. These cross-check the double engine against std:: to ~a
// notch, the same oracle a cross-engine diff would use.
TEST_CASE("dbl: transcendental tier on real bounds matches std::", "[dbl][real][transcendental]")
{
  constexpr double tol = 4.0 / 16384;   // a few notches

  SECTION("log / log2 / log10 — strictly positive domain")
  {
    using p = bound<{{1, 16}, notch<1, 16384>}, round_nearest | real>;
    using o = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
    for (double x : {1.0, 1.5, 2.0, std::numbers::e, 8.0, 10.0, 16.0})
    {
      REQUIRE(std::fabs(double(o{math::log(p{x})})   - std::log(x))   < tol);
      REQUIRE(std::fabs(double(o{math::log2(p{x})})  - std::log2(x))  < tol);
      REQUIRE(std::fabs(double(o{math::log10(p{x})}) - std::log10(x)) < tol);
    }
    // Anchors that must be exact-ish on the grid.
    REQUIRE(std::fabs(double(o{math::log(p{1.0})}))   < tol);   // log 1 = 0
    REQUIRE(std::fabs(double(o{math::log2(p{8.0})})  - 3.0) < tol);
    REQUIRE(std::fabs(double(o{math::log10(p{10.0})}) - 1.0) < tol);
  }

  SECTION("exp / exp2")
  {
    using e_in  = bound<{{-2, 2}, notch<1, 16384>}, round_nearest | real>;
    using e_out = bound<{{0, 8},  notch<1, 16384>}, round_nearest | real>;
    for (double x : {-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0})
    {
      REQUIRE(std::fabs(double(e_out{math::exp(e_in{x})})  - std::exp(x))  < tol);
      REQUIRE(std::fabs(double(e_out{math::exp2(e_in{x})}) - std::exp2(x)) < tol);
    }
  }

  SECTION("asin / acos / atan on their domains")
  {
    using u = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;
    using o = bound<{{-2, 2}, notch<1, 16384>}, round_nearest | real>;
    for (double x : {-0.9, -0.5, 0.0, 0.25, 0.5, 0.9})
    {
      REQUIRE(std::fabs(double(o{math::asin(u{x})}) - std::asin(x)) < tol);
      REQUIRE(std::fabs(double(o{math::acos(u{x})}) - std::acos(x)) < tol);
      REQUIRE(std::fabs(double(o{math::atan(o{x})}) - std::atan(x)) < tol);
    }
  }

  SECTION("sinh / cosh / tanh / cbrt")
  {
    using s_in  = bound<{{-2, 2}, notch<1, 16384>}, round_nearest | real>;
    using s_out = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
    using c_in  = bound<{{1, 8},  notch<1, 16384>}, round_nearest | real>;
    for (double x : {-2.0, -1.0, 0.0, 1.0, 2.0})
    {
      REQUIRE(std::fabs(double(s_out{math::sinh(s_in{x})}) - std::sinh(x)) < tol);
      REQUIRE(std::fabs(double(s_out{math::cosh(s_in{x})}) - std::cosh(x)) < tol);
      REQUIRE(std::fabs(double(s_out{math::tanh(s_in{x})}) - std::tanh(x)) < tol);
    }
    for (double x : {1.0, 2.0, 3.375, 8.0})
      REQUIRE(std::fabs(double(s_out{math::cbrt(c_in{x})}) - std::cbrt(x)) < tol);
  }
}

#endif // !BND_MATH_FIXED
