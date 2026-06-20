// Corner-case tests closing coverage holes surfaced by the gcov build
// (configure with -DBOUND_COVERAGE=ON, then `make coverage`).
//
// Each TEST_CASE names the library file:line(s) it is meant to exercise. Many
// of these paths were already checked at *compile* time via STATIC_REQUIRE —
// which gcov does not count — so the assertions here are deliberately runtime
// (plain REQUIRE) to drive the instrumented code.

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/cmath_double.hpp"
#include "bound/casts.hpp"
#include "bound/predicates.hpp"
#include "bound/io.hpp"
#include "bound/math.hpp"
#include "bound/detail/rational.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace bnd;
using namespace bnd::detail;

//---------------------------------------------------------------------------
// casts.hpp:66 — checked_cast success return (was STATIC_REQUIRE only)
//---------------------------------------------------------------------------
TEST_CASE("checked_cast returns the value on the in-range happy path (runtime)",
          "[cast][cover]")
{
  using pct = bound<{0, 100}>;
  pct p = checked_cast<pct>(42);          // runtime call, not STATIC_REQUIRE
  REQUIRE(p == 42);

  using coarse = bound<{{0, 10}, 2}>;
  REQUIRE(checked_cast<coarse>(4) == coarse{4});   // on-notch, in-range
}

//---------------------------------------------------------------------------
// predicates.hpp:39 — will_conversion_trunc returns false out of range
//---------------------------------------------------------------------------
TEST_CASE("will_conversion_trunc is false for out-of-range values (runtime)",
          "[predicates][cover]")
{
  using coarse = bound<{{0, 10}, 2}>;
  // Out of range is overflow, not truncation: the predicate is false.
  REQUIRE_FALSE(will_conversion_trunc<coarse>(11));
  REQUIRE_FALSE(will_conversion_trunc<coarse>(-1));
  // Contrast: off-notch but in range is truncation.
  REQUIRE(will_conversion_trunc<coarse>(3));
}

#ifndef BND_MATH_FIXED
namespace d = bnd::math::dbl::detail;

//---------------------------------------------------------------------------
// cmath_double.hpp:162-163 — d_cbrt negative branch
// cmath_double.hpp:205-207 — d_atan2 on the axes (x == 0)
//---------------------------------------------------------------------------
TEST_CASE("dbl: cbrt of negatives and atan2 on the axes", "[dbl][cover]")
{
  // cbrt(x<0) = -cbrt(-x)
  REQUIRE(std::fabs(d::d_cbrt(-8.0)  - (-2.0)) < 1e-12);
  REQUIRE(std::fabs(d::d_cbrt(-27.0) - (-3.0)) < 1e-12);
  REQUIRE(std::fabs(d::d_cbrt(27.0)  -   3.0)  < 1e-12);

  // atan2 with x == 0: the y>0 / y<0 / y==0 axis cases.
  REQUIRE(std::fabs(d::d_atan2(1.0, 0.0)  - std::atan2(1.0, 0.0))  < 1e-12);  // +pi/2
  REQUIRE(std::fabs(d::d_atan2(-1.0, 0.0) - std::atan2(-1.0, 0.0)) < 1e-12);  // -pi/2
  REQUIRE(d::d_atan2(0.0, 0.0) == 0.0);                                        //  0
  // and the x<0 reflective branch for good measure
  REQUIRE(std::fabs(d::d_atan2(1.0, -1.0)  - std::atan2(1.0, -1.0))  < 1e-12);
  REQUIRE(std::fabs(d::d_atan2(-1.0, -1.0) - std::atan2(-1.0, -1.0)) < 1e-12);
}
#endif // !BND_MATH_FIXED

//---------------------------------------------------------------------------
// lift.hpp:116 — binary expected-lift short-circuits on a RIGHT-side error
// (the left-error short-circuit at :114 was already covered).
//---------------------------------------------------------------------------
TEST_CASE("expected-lift propagates a right-hand-side error", "[errors][lift][cover]")
{
  using num_t = bound<{0, 100}>;
  slim::expected<num_t, errc> left {num_t{5}};                       // valid
  slim::expected<num_t, errc> right{slim::unexpected{errc::overflow}};

  auto both = left + right;          // lhs OK, rhs error -> rhs.error() wins
  REQUIRE_FALSE(both.has_value());
  REQUIRE(both.error() == errc::overflow);
}

//---------------------------------------------------------------------------
// rational.hpp:846-866 — operator<=> cross-multiply path at runtime
// (the existing ordering tests are STATIC_REQUIRE; the runtime path, and the
//  lhs_neg branch at :863-864, were uncovered.)
//---------------------------------------------------------------------------
TEST_CASE("rational spaceship cross-multiply, positive and negative",
          "[rational][cover]")
{
  using std::strong_ordering;

  // Both denominators != 1, no overflow, positive operands.
  REQUIRE((rational{1u, 2} <=> rational{1u, 3}) == strong_ordering::greater);
  REQUIRE((rational{1u, 3} <=> rational{1u, 2}) == strong_ordering::less);
  REQUIRE((rational{2u, 6} <=> rational{1u, 3}) == strong_ordering::equal);

  // Both negative -> the lhs_neg flip (B <=> A).
  REQUIRE((rational{1, -2} <=> rational{1, -3}) == strong_ordering::less);     // -1/2 < -1/3
  REQUIRE((rational{1, -3} <=> rational{1, -2}) == strong_ordering::greater);  // -1/3 > -1/2
}

//---------------------------------------------------------------------------
// rational.hpp:305-306 — gcd()/lcm cofactor multiply overflows umax -> nullopt
// (distinct from the imax-cap nullopt at :307-308 exercised elsewhere: here
//  the product exceeds 2^64, not merely imax_max.)
//---------------------------------------------------------------------------
TEST_CASE("rational gcd: lcm that overflows umax returns nullopt",
          "[rational][overflow][cover]")
{
  // Two odd, coprime denominators near 2^40; their product ~2^80 overflows umax,
  // so the cross-multiplication trap (not the imax cap) fires.
  imax a = (imax{1} << 40) + 1;
  imax b = (imax{1} << 40) + 3;
  REQUIRE_FALSE(gcd(rational{1u, a}, rational{1u, b}).has_value());
}

//---------------------------------------------------------------------------
// rational.hpp:610-613 — checked add, numerator SUM overflows after the
// (non-overflowing) cross-multiply on UNEQUAL denominators. The equal-
// denominator overflow (:547-550) and the cross-multiply overflow (:589-595)
// were already covered; this is the third, post-cross-multiply overflow.
//---------------------------------------------------------------------------
TEST_CASE("rational add: numerator sum overflow on unequal denominators",
          "[rational][overflow][cover]")
{
  // Denominators 2 and 6 (odd numerators so neither reduces). After cross-
  // multiply A = num_a*3, B = num_b*1 each fit in umax, but A + B exceeds 2^64.
  rational a{5999999999999999999u, 2};   // odd numerator -> stays /2
  rational b{ 999999999999999997u, 6};   // coprime to 6   -> stays /6
  REQUIRE_FALSE((a + b).has_value());
}

//---------------------------------------------------------------------------
// format.hpp:56-58 — decimal fraction needs leading-zero padding.
//---------------------------------------------------------------------------
TEST_CASE("rational to_string zero-pads short decimal fractions",
          "[rational][format][cover]")
{
  REQUIRE(bnd::to_string(rational{1u, 16})  == "0.0625");   // frac "625" -> "0625"
  REQUIRE(bnd::to_string(rational{1u, 100}) == "0.01");     // frac  "1"  -> "01"
  REQUIRE(bnd::to_string(rational{3u, 16})  == "0.1875");
}

//---------------------------------------------------------------------------
// math.hpp:158-166 — constexpr ldexp into the subnormal range, including the
// round-to-nearest-even ++mantissa at :165-166. Validated against std::ldexp.
//---------------------------------------------------------------------------
TEST_CASE("ldexp into subnormal range matches std::ldexp (with rounding)",
          "[math][cover]")
{
  const double mants[] = {1.0, 1.5, 1.9999999999,
                          0x1.fffffffffffffp0,    // all-ones mantissa: forces round-up
                          0x1.5555555555555p0,
                          0x1.0000000000001p0};
  for (int e = -1080; e <= -1020; ++e)
    for (double m : mants)
      REQUIRE(bnd::detail::ldexp(m, e) == std::ldexp(m, e));
}

//---------------------------------------------------------------------------
// math.hpp:193-194 — abs_fraction on a subnormal (no implicit leading 1)
// math.hpp:215-219 — magnitudes below ~2^-62: significand-drop / flush-to-zero
//---------------------------------------------------------------------------
TEST_CASE("rational from subnormal and very small doubles", "[math][rational][cover]")
{
  // Subnormal input: exercises the e==0 branch, then collapses to 0 (drop>=64).
  REQUIRE(rational{std::numeric_limits<double>::denorm_min()} == 0_r);

  // Normal but below 2^-62: the significand is shifted down and recovered by
  // the trailing reduction, yielding the exact dyadic fraction.
  REQUIRE(rational{0x1p-40} == rational{1u, imax{1} << 40});

  // Far below the cap: drops to a hard zero.
  REQUIRE(rational{0x1p-70} == 0_r);
}

//---------------------------------------------------------------------------
// bound.hpp:733-734 — operator/=(integral)   bound.hpp:744-745 — operator%=
//---------------------------------------------------------------------------
TEST_CASE("compound /= and %= with a snap bound rhs (runtime)", "[bound][compound][cover]")
{
  // Integer division/modulo now flow through the boundable path; `snap`
  // gives the same C++ trunc-toward-zero / dividend-signed-remainder semantics
  // the old raw-int compound assigns had.
  using sb = bound<{-100, 100}, snap>;

  sb a{20};
  a /= sb{3};                      // integer division, truncates toward zero
  REQUIRE(a == 6);

  sb n{-20};
  n /= sb{3};
  REQUIRE(n == -6);

  sb b{20};
  b %= sb{7};
  REQUIRE(b == 6);

  sb m{-20};
  m %= sb{7};
  REQUIRE(m == -6);                // C++ remainder keeps the dividend's sign
}

//---------------------------------------------------------------------------
// bound.hpp — operator+=(boundable) result out of range, checked policy with
// no clamp/wrap/sentinel handler -> report (throws).
//---------------------------------------------------------------------------
TEST_CASE("checked += boundable overflow reports (throws)", "[bound][compound][cover]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 a{80};
  c100 b{50};
  REQUIRE_THROWS_AS(a += b, bnd::bound_error);   // 130 not in [0,100]
}

//---------------------------------------------------------------------------
// assignment.hpp:500-505 — error_action on an out-of-interval *fractional* rhs.
// The existing on_error test assigns an integer (a different assign overload);
// this drives the fractional (double) overload.
//---------------------------------------------------------------------------
TEST_CASE("on_error fires for an out-of-range double assignment",
          "[bound][policy][on_error][cover]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 e{50};
  bool fired = false;
  e.on_error([&](auto& self, errc code, std::string_view msg) {
    fired = (code == errc::domain_error) && !msg.empty();
    self = 0;
  }) = 200.5;                      // fractional, out of [0,100]
  REQUIRE(fired);
  REQUIRE(e == 0);
}

//---------------------------------------------------------------------------
// policy.hpp — action-decorated operator-=(boundable) on the NON-overflow path
// (existing tests only exercise the overflowing arm).
//---------------------------------------------------------------------------
TEST_CASE("on_overflow compound subtract that does not overflow",
          "[bound][policy][compound][cover]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 acc{50};
  bool fired = false;
  acc.on_overflow([&](auto&, errc) { fired = true; }) -= 10_b;   // 40, no overflow
  REQUIRE_FALSE(fired);
  REQUIRE(acc == 40);
}

#ifndef BND_MATH_FIXED
//---------------------------------------------------------------------------
// bound.hpp:161-163 — store_real out-of-range with a reporting/sentinel policy
// (the clamp/wrap arms are covered elsewhere; the domain_fail arm was not).
//---------------------------------------------------------------------------
TEST_CASE("real store out of range: sentinel and checked policies",
          "[bound][real][cover]")
{
  using rbs = bound<{{-1, 1}, notch<1, 1024>}, real | sentinel>;
  rbs s = 5.0;                                   // out of range -> sentinel (domain_fail)
  REQUIRE(s == rbs::make_sentinel());            // real sentinel is a finite, comparable value

  using rbc = bound<{{-1, 1}, notch<1, 1024>}, real | checked>;
  REQUIRE_THROWS_AS((rbc{5.0}), bnd::bound_error);   // out of range -> report (throws)
}
#endif // !BND_MATH_FIXED

//---------------------------------------------------------------------------
// assignment.hpp:609-613 — bound -> bound store into a `real` (double-backed)
// target decodes the source and snaps to the dyadic grid.
//---------------------------------------------------------------------------
#ifndef BND_MATH_FIXED
TEST_CASE("bound -> real conversion snaps onto the double grid",
          "[bound][real][convert][cover]")
{
  using src_t = bound<{-2, 2}>;                          // integer-backed source
  using rb    = bound<{{-2, 2}, notch<1, 1024>}, real>;  // double-backed target

  src_t src{1};
  rb dst = src;                                          // boundable -> real store
  REQUIRE(double(dst) == 1.0);

  src_t neg{-2};
  rb dn = neg;
  REQUIRE(double(dn) == -2.0);
}
#endif // !BND_MATH_FIXED

//---------------------------------------------------------------------------
// assignment.hpp:475-476 — off-notch fractional store on a policy with no
// rounding-mode flag and round_check()==false. `snap` would route
// through the has_round_flag arm (it is counted as a round flag); a plain
// `clamp` policy (not checked, not snap) takes the :476 else-branch
// and truncates an in-range off-notch value silently.
//---------------------------------------------------------------------------
TEST_CASE("clamp policy truncates an in-range off-notch fractional assignment",
          "[bound][policy][round][cover]")
{
  using b = bound<{{0, 10}, notch<1, 2>}, clamp>;          // notch 1/2
  b x{0};
  x = 0.3;                          // in range, off the 1/2 grid → truncates to 0
  REQUIRE(x == 0);

  x = 0.9;                          // 0.9 → 1.8 half-notches → truncates to 1 half → 0.5
  REQUIRE(static_cast<rational>(x) == rational{1u, 2});
}

//---------------------------------------------------------------------------
// generic.hpp:355-361 — raw_from_offset(imax) overload, reached from the
// integer fast path of math::fmod (cmath.hpp:1012, fmod_int_fast) with a
// signed offset. Needs non-real, non-rational integer grids and a divisor
// that excludes zero. math::fmod was otherwise only static_assert-tested.
//---------------------------------------------------------------------------
TEST_CASE("math::fmod integer fast path (raw_from_offset imax)",
          "[cmath][fmod][cover]")
{
  using in_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;  // integer-backed
  using div_t = bound<{{ 1, 8}, notch<1, 16384>}, round_nearest>;  // excludes zero
  using out_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;

  REQUIRE(static_cast<rational>(math::fmod<out_t>(in_t{7_r},  div_t{3_r})) == 1);
  REQUIRE(static_cast<rational>(math::fmod<out_t>(in_t{-7_r}, div_t{3_r})) == -1);  // signed offset
  REQUIRE(static_cast<rational>(math::fmod<out_t>(in_t{5.5_r}, div_t{2_r})) == rational{3u, 2});
}

#ifndef BND_MATH_FIXED
//---------------------------------------------------------------------------
// generic.hpp:469 — domain_fail returns false for an unchecked policy
// (not sentinel, domain_check()==false): the value is stored as-is.
//---------------------------------------------------------------------------
TEST_CASE("unsafe real store out of range falls through (no report)",
          "[bound][real][unsafe][cover]")
{
  using rb = bound<{{-1, 1}, notch<1, 1024>}, real | unsafe>;
  rb x = 5.0;                       // out of range, unsafe: stored as-is, no throw
  REQUIRE(double(x) == 5.0);
}
#endif // !BND_MATH_FIXED

//---------------------------------------------------------------------------
// assignment.hpp:631-634 + generic.hpp:355-361 — bound -> bound store on the
// rational (non-integer-mapping) path where the target offset is NEGATIVE, so
// raw_from_offset() is reached through the negated branch.
//---------------------------------------------------------------------------
TEST_CASE("cross-grid conversion of a negative off-notch value",
          "[bound][convert][cover]")
{
  // notch 1/2 source -> notch 1/3 target: Factor = 3/2 (non-integer mapping),
  // so the rational store path runs; the negative value drives the
  // negative-denominator branch.
  using src_t = bound<{{-4, 4}, notch<1, 2>}>;
  using dst_t = bound<{{-4, 4}, notch<1, 3>}, snap>;

  src_t s{-1.5};
  dst_t d = s;
  // snap truncates toward ZERO in value space (matching the scalar store
  // path and div_rounded): -1.5 on the 1/3 grid → -4/3. (Was -5/3 when the old
  // path truncated the non-negative offset — i.e. toward -inf in value space.)
  REQUIRE(static_cast<rational>(d) == rational{4, -3});

  src_t s2{1.5};
  dst_t d2 = s2;                    // +1.5 toward zero → 4/3 (unchanged)
  REQUIRE(static_cast<rational>(d2) == rational{4u, 3});
}
