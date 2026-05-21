#include "bound/bound.hpp"
#include "bound/math.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace bnd;

TEST_CASE("safe_abs handles INT_MIN without UB", "[math][safe_abs]")
{
  STATIC_REQUIRE(safe_abs(imax{0}) == 0u);
  STATIC_REQUIRE(safe_abs(imax{42}) == 42u);
  STATIC_REQUIRE(safe_abs(imax{-42}) == 42u);

  // INT_MIN: -INT_MIN as signed would overflow; safe_abs computes via umax
  constexpr imax min_v = std::numeric_limits<imax>::min();
  STATIC_REQUIRE(safe_abs(min_v) == static_cast<umax>(std::numeric_limits<imax>::max()) + 1u);
}

TEST_CASE("frexp handles zero", "[math][frexp]")
{
  int e = 99;
  double r = bnd::frexp(0.0, &e);
  REQUIRE(r == 0.0);
  REQUIRE(e == 0);

  // negative zero
  e = 99;
  double rn = bnd::frexp(-0.0, &e);
  REQUIRE(rn == 0.0);
  REQUIRE(e == 0);
}

TEST_CASE("frexp handles infinity and NaN", "[math][frexp]")
{
  int e = 99;
  double inf = std::numeric_limits<double>::infinity();
  double r = bnd::frexp(inf, &e);
  REQUIRE(std::isinf(r));
  REQUIRE(e == 0);

  e = 99;
  double nan = std::numeric_limits<double>::quiet_NaN();
  double rn = bnd::frexp(nan, &e);
  REQUIRE(std::isnan(rn));
  REQUIRE(e == 0);
}

TEST_CASE("frexp matches std::frexp on normals", "[math][frexp]")
{
  for (double v : { 1.0, 0.5, 0.25, 1.5, 1024.0, 0.001, -7.25 })
  {
    int be = 0, se = 0;
    double br = bnd::frexp(v, &be);
    double sr = std::frexp(v, &se);
    REQUIRE(br == sr);
    REQUIRE(be == se);
  }
}

TEST_CASE("frexp on subnormal scales up via recursion", "[math][frexp]")
{
  // smallest positive subnormal — exercises the e==0 branch
  double sub = std::numeric_limits<double>::denorm_min();
  REQUIRE(sub > 0.0);
  int be = 0, se = 0;
  double br = bnd::frexp(sub, &be);
  double sr = std::frexp(sub, &se);
  REQUIRE(br == sr);
  REQUIRE(be == se);
}

TEST_CASE("ldexp identity cases", "[math][ldexp]")
{
  STATIC_REQUIRE(bnd::ldexp(0.0, 5) == 0.0);
  STATIC_REQUIRE(bnd::ldexp(1.5, 0) == 1.5);
}

TEST_CASE("ldexp inf/NaN passthrough", "[math][ldexp]")
{
  double inf = std::numeric_limits<double>::infinity();
  REQUIRE(std::isinf(bnd::ldexp(inf, 3)));

  double nan = std::numeric_limits<double>::quiet_NaN();
  REQUIRE(std::isnan(bnd::ldexp(nan, 3)));
}

TEST_CASE("ldexp overflow goes to infinity", "[math][ldexp]")
{
  // Use volatile inputs so the compiler can't fold the call at compile time.
  volatile double v_pos = 1.0;
  volatile double v_neg = -1.0;
  volatile int    e     = 2048;

  double r = bnd::ldexp(v_pos, e);
  REQUIRE(std::isinf(r));
  REQUIRE(r > 0.0);

  double rn = bnd::ldexp(v_neg, e);
  REQUIRE(std::isinf(rn));
  REQUIRE(rn < 0.0);
}

TEST_CASE("ldexp underflow produces subnormal or zero", "[math][ldexp]")
{
  volatile double v   = 1.0;
  volatile int    big = -1100;
  volatile int    mid = -1050;

  double zero = bnd::ldexp(v, big);
  REQUIRE(zero == 0.0);

  double sub = bnd::ldexp(v, mid);
  REQUIRE(sub > 0.0);
  REQUIRE(sub < std::numeric_limits<double>::min());
}

TEST_CASE("ldexp matches std::ldexp on normal exponents", "[math][ldexp]")
{
  for (double v : { 1.0, 0.5, 1.5, -3.25, 1024.0 })
  {
    for (int e : { 0, 1, -1, 10, -10, 50, -50 })
    {
      double a = bnd::ldexp(v, e);
      double b = std::ldexp(v, e);
      REQUIRE(a == b);
    }
  }
}

TEST_CASE("ldexp subnormal input is normalised first", "[math][ldexp]")
{
  // Scaling a subnormal up should match std::ldexp
  double sub = std::numeric_limits<double>::denorm_min();
  double a = bnd::ldexp(sub, 100);
  double b = std::ldexp(sub, 100);
  REQUIRE(a == b);
}

TEST_CASE("abs_fraction handles values >= 2^53", "[math][abs_fraction]")
{
  // exponent >= 53 path: num <<= (exponent - bits); den = 1
  // Volatile so the compiler can't fold the constexpr call at compile time.
  volatile double v53 = static_cast<double>(1ull << 53);
  volatile double v60 = static_cast<double>(1ull << 60);

  auto [n1, d1] = abs_fraction(v53);
  REQUIRE(d1 == 1u);
  REQUIRE(n1 == (1ull << 53));

  auto [n2, d2] = abs_fraction(v60);
  REQUIRE(d2 == 1u);
  REQUIRE(n2 == (1ull << 60));
}

TEST_CASE("abs_fraction throws on non-finite", "[math][abs_fraction]")
{
  REQUIRE_THROWS(abs_fraction(std::numeric_limits<double>::infinity()));
  REQUIRE_THROWS(abs_fraction(-std::numeric_limits<double>::infinity()));
  REQUIRE_THROWS(abs_fraction(std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("rational from large doubles", "[rational][from_double]")
{
  // Through rational ctor — exercises the exponent>=53 branch in abs_fraction
  rational r1{static_cast<double>(1ull << 53)};
  REQUIRE(r1.Numerator == (1ull << 53));
  REQUIRE(r1.Denominator == 1);

  rational r2{static_cast<double>(1ull << 50)};
  REQUIRE(r2.Numerator == (1ull << 50));
  REQUIRE(r2.Denominator == 1);
}

TEST_CASE("rational rejects non-finite double", "[rational][from_double]")
{
  REQUIRE_THROWS_AS(rational{std::numeric_limits<double>::infinity()},
                    std::system_error);
  REQUIRE_THROWS_AS(rational{std::numeric_limits<double>::quiet_NaN()},
                    std::system_error);
}
