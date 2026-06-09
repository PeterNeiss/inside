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
#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

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

  // The value flows through at full double precision — no notch snap.
  ang x = 0.6;
  amp y = math::dbl::sin_core<amp>(x);
  REQUIRE(std::fabs(double(y) - std::sin(0.6)) < 1e-15);

  // sin(0) round-trips to exactly 0.
  ang z = 0.0;
  REQUIRE(double(math::dbl::sin_core<amp>(z)) == 0.0);
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

  double sv = std::sin(0.6);
  REQUIRE(std::fabs(double(y) - 2.5 * sv)       < 1e-15);
  REQUIRE(std::fabs(double(w) - 3.5 * sv)       < 1e-15);
  REQUIRE(std::fabs(double(d) - (sv - 0.1))     < 1e-15);

  REQUIRE((s > amp{0.5}));     // compares in double, no truncation
  REQUIRE((s == s));

  // real division → double (continuous result grid, double-backed)
  using pos = bound<{{1, 4}, notch<1, 65536>}, real>;
  pos a3 = 3.0, b2 = 2.0;
  auto q = a3 / b2;
  static_assert(std::is_same_v<decltype(q)::raw_type, double>);
  REQUIRE(double(q) == 1.5);
}

#endif // !BND_MATH_FIXED
