// Bugs surfaced by the 2026-05 post-fix audit. Each TEST_CASE here should
// fail on the unfixed build and pass after the corresponding fix lands.

#include "bound/bound.hpp"
#include "bound/rational.hpp"
#include "bound/grid.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

//---------------------------------------------------------------------------
// Bug A — addition.hpp:90
//
// The rational-mixed `add` branch stores `((sum - Lower<result>) / Notch<result>)`
// directly into `res.Raw`. That's the L-offset, but when the result type is
// IsDirectStorage<result> the Raw must hold the *value*. Same encoding-
// mismatch class as the previously-fixed assignment paths.
//---------------------------------------------------------------------------
TEST_CASE("Bug A: rational-mixed add into direct-storage result", "[bound][addition][regression]")
{
  using L = bound<{-5, 5}>;                          // signed-direct
  using R = bound<{{-10, 10}, rational{0u}}>;        // rational raw

  constexpr L l{2};
  constexpr R r{rational{1u}};
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
// `if consteval { throw }` in assignment::assign fires before the policy
// machinery can react).
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
