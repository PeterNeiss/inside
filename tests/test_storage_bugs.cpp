// Bugs surfaced by the 2026-05 post-fix audit. Each TEST_CASE here should
// fail on the unfixed build and pass after the corresponding fix lands.

#include "bound/bound.hpp"
#include "bound/detail/rational.hpp"
#include "bound/grid.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace bnd;
using namespace bnd::detail;

//---------------------------------------------------------------------------
// Bug A — addition.hpp:90
//
// The rational-mixed `add` branch stores `((sum - Lower<result>) / Notch<result>)`
// directly into `res.Raw`. That's the L-offset, but when the result type is
// !index_raw<result> the Raw must hold the *value*. Same encoding-
// mismatch class as the previously-fixed assignment paths.
//---------------------------------------------------------------------------
TEST_CASE("Bug A: rational-mixed add into direct-storage result", "[bound][addition][regression]")
{
  using L = bound<{-5, 5}>;                          // signed-direct
  using R = bound<{{-10, 10}, 0_r}>;        // rational raw

  constexpr L l{2};
  constexpr R r{1_r};
  // Result grid: {-15, 15}, notch 1 → signed-direct.
  STATIC_REQUIRE(l + r == 3);
}

//---------------------------------------------------------------------------
// Bug B — multiplication.hpp:117
//
// The third-quadrant case (Lower<result> == Upper<L> * Lower<R>) computes
// `negRaw = NotchCount<L> - lhs.Raw`. That formula treats lhs.Raw as a
// notch-offset, which is correct for offset-encoded raws but wrong for
// direct-storage signed raws (where Raw is the value).
//
// The IsIntegerAligned fast path (multiplication.hpp:70-87) catches the
// all-integer case, so the bug only surfaces when one operand has a
// fractional notch (which forces the result to be non-integer-aligned and
// skips the fast path). L stays direct (Notch_L = 1, signed lower).
//---------------------------------------------------------------------------
TEST_CASE("Bug B: signed-direct multiplication third quadrant", "[bound][multiplication][regression]")
{
  using L = bound<{-5, 5}>;                            // signed-direct, integer-aligned
  using R = bound<{{-10, 10}, rational{1u, 2}}>;       // notch 1/2, not direct, not integer-aligned

  // Lower<result> = Upper<L> * Lower<R> = 5 * -10 = -50 → third quadrant.
  // Without the fix, L{2} * R{1} produces value -3 instead of 2.
  STATIC_REQUIRE(L{ 2} * R{rational{ 1u}} == rational{ 2u});
  STATIC_REQUIRE(L{ 3} * R{rational{ 2u}} == rational{ 6u});
  STATIC_REQUIRE(L{ 0} * R{rational{ 5u}} == rational{ 0u});
}

//---------------------------------------------------------------------------
// Bug C — assignment.hpp:428
//
// `assign(boundable, real R)` checks for `HasPolicy<L, P, clamp>` but not
// for `HasPolicy<L, P, wrap>`. A `bound<{...}, wrap>` constructed from a
// double silently stores the unwrapped value (which may be out of range)
// because it falls through `domain_fail` without `checked` set.
//
// Runtime-only: wrap policy is bypassed in constant evaluation (the
// `is_constant_evaluated()` throw in assignment::assign fires before the
// policy machinery can react).
//---------------------------------------------------------------------------
TEST_CASE("Bug C: wrap policy fires for real rhs", "[bound][wrap][regression]")
{
  using L = bound<{0, 100}, wrap>;

  // 120 wraps once into [0, 100] → 19 (since the range is 101 inclusive).
  REQUIRE(L{double{120.0}} == 19);

  // negative wraps to the upper side
  REQUIRE(L{double{-5.0}} == 96);

  // signed-range wrap
  using S = bound<{-50, 50}, wrap>;
  // 75 wraps once: 75 - 101 = -26.
  REQUIRE(S{double{75.0}} == -26);
}

//---------------------------------------------------------------------------
// Bug D — rational.hpp:260
//
// `gcd(rational, rational)` calls `std::lcm` on |Denominator|s without an
// overflow check and casts the result to imax via static_cast. Large
// denominators silently wrap. The fix is to make `gcd` return
// `slim::optional<rational>` and detect the overflow.
//
// Trigger: lcm(2^62, 3) = 3 * 2^62. That fits in umax (≈1.38e19) but
// exceeds imax_max (≈9.22e18). After the cast to imax it goes negative,
// producing a bogus rational.
//---------------------------------------------------------------------------
TEST_CASE("Bug D: gcd lcm overflow propagates to grid::operator+", "[bound][grid][rational][regression]")
{
  // Use grid arithmetic since gcd's return type changes — the optional
  // surfaces at grid::operator+ which already returns slim::optional<grid>.
  constexpr auto big   = rational{1u, imax{1} << 62};
  constexpr auto third = rational{1u, 3};

  constexpr grid g1{interval{0_r, 1_r}, big};
  constexpr grid g2{interval{0_r, 1_r}, third};

  STATIC_REQUIRE_FALSE((g1 + g2).has_value());
}

#ifndef BND_MATH_FIXED
//---------------------------------------------------------------------------
// Bug E — grid.hpp double_exact / arithmetic.
//
// `real` (double-backed) arithmetic silently diverged from the exact grid
// arithmetic whenever a result needed more than double's 53-bit significand.
// `dyadic_grid<G>` (the old storage guard) checks only power-of-two
// denominators; it ignores the significand. A real `×` whose product grid
// outgrows 2^53 (notch = N_L·N_R) dropped the low bits.
//
// Fix: `real` is selected only on `double_exact` grids; an op whose result
// grid isn't double-exact drops `real` and falls back to exact storage, so the
// result equals the exact rational product.
//---------------------------------------------------------------------------
TEST_CASE("Bug E: real * stays exact (drops real when product exceeds 2^53)",
          "[bound][real][regression]")
{
  using U = bound<{{0, 4}, notch<1, (1u << 26)>}, real>;   // exact operand (f=26)
  static_assert(std::is_same_v<U::raw_type, double>);

  const U a = 4.0 - std::ldexp(1.0, -26);                  // index 2^28-1, exact
  auto p = a * a;                                          // product grid f=52 > 53 bits
  static_assert(!std::is_same_v<decltype(p)::raw_type, double>);   // real dropped
  const rational ar = static_cast<rational>(a);
  REQUIRE(static_cast<rational>(p) == *(ar * ar));
}

//---------------------------------------------------------------------------
// Bug F — division.hpp real path.
//
// Real `÷0` stored a bare `inf` (snap_double then did static_cast<imax>(inf),
// UB), bypassing the error vocabulary. Fix: real division reports zero like
// every other path — the return widens to optional<result> when the divisor
// grid can be zero (nullopt on a zero divisor), and the expected-lift surfaces
// errc::division_by_zero. The real sentinel stays a finite, comparable value
// (DBL_MAX), used only for out-of-range stores.
//---------------------------------------------------------------------------
TEST_CASE("Bug F: real div-by-zero is reported, not a silent inf",
          "[bound][real][regression]")
{
  using N  = bound<{{1, 4}, notch<1, 1024>}, real>;
  using Dz = bound<{{0, 4}, notch<1, 1024>}, real>;   // divisor grid spans zero

  auto q = N{3.0} / Dz{0.0};
  REQUIRE_FALSE(q.has_value());                       // optional, nullopt — not inf

  slim::expected<N, errc> en{N{3.0}};
  auto z = en / Dz{0.0};
  REQUIRE_FALSE(z.has_value());
  REQUIRE(z.error() == errc::division_by_zero);
}
#endif // !BND_MATH_FIXED
