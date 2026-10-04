// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bugs surfaced by the 2026-05 post-fix audit. Each TEST here should
// fail on the unfixed build and pass after the corresponding fix lands.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/grid.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Bug A — addition.hpp:90
//
// The rational-mixed `add` branch stores `((sum - lower_of<result>) / notch_of<result>)`
// directly into `res.Raw`. That's the L-offset, but when the result type is
// !index_raw<result> the Raw must hold the *value*. Same encoding-
// mismatch class as the previously-fixed assignment paths.
//---------------------------------------------------------------------------
// Bug A: rational-mixed add into direct-storage result
TEST(StorageBugsTest, bug_a_rational_mixed_add_into_direct_storage_result)
{
  using L = inside<{-5, 5}>;                          // signed-direct
  using R = inside<{{-10, 10}, 0_r}>;        // rational raw

  constexpr L l{2};
  constexpr R r{1_r};
  // Result grid: {-15, 15}, notch 1 → signed-direct.
  static_assert(l + r == 3);
}

//---------------------------------------------------------------------------
// Bug B — multiplication.hpp:117
//
// The third-quadrant case (lower_of<result> == upper_of<L> * lower_of<R>) computes
// `negRaw = max_index_v<L> - lhs.Raw`. That formula treats lhs.Raw as a
// notch-offset, which is correct for offset-encoded raws but wrong for
// direct-storage signed raws (where Raw is the value).
//
// The is_integer_aligned fast path (multiplication.hpp:70-87) catches the
// all-integer case, so the bug only surfaces when one operand has a
// fractional notch (which forces the result to be non-integer-aligned and
// skips the fast path). L stays direct (Notch_L = 1, signed lower).
//---------------------------------------------------------------------------
// Bug B: signed-direct multiplication third quadrant
TEST(StorageBugsTest, bug_b_signed_direct_multiplication_third_quadrant)
{
  using L = inside<{-5, 5}>;                            // signed-direct, integer-aligned
  using R = inside<{{-10, 10}, rational{1u, 2}}>;       // notch 1/2, not direct, not integer-aligned

  // lower_of<result> = upper_of<L> * lower_of<R> = 5 * -10 = -50 → third quadrant.
  // Without the fix, L{2} * R{1} produces value -3 instead of 2.
  static_assert(L{ 2} * R{rational{ 1u}} == rational{ 2u});
  static_assert(L{ 3} * R{rational{ 2u}} == rational{ 6u});
  static_assert(L{ 0} * R{rational{ 5u}} == rational{ 0u});
}

//---------------------------------------------------------------------------
// Bug C — assignment.hpp:428
//
// `assign(insidable, f64 R)` checks for `has_policy<L, P, clamp>` but not
// for `has_policy<L, P, wrap>`. An `inside<{...}, wrap>` constructed from a
// double silently stores the unwrapped value (which may be out of range)
// because it falls through `range_fail` without `checked` set.
//
// Runtime-only: wrap policy is bypassed in constant evaluation (the
// `is_constant_evaluated()` throw in assignment::assign fires before the
// policy machinery can react).
//---------------------------------------------------------------------------
// Bug C: wrap policy fires for f64 rhs
TEST(StorageBugsTest, bug_c_wrap_policy_fires_for_f64_rhs)
{
  using L = inside<{0, 100}, wrap>;

  // 120 wraps once into [0, 100] → 19 (since the range is 101 inclusive).
  ASSERT_EQ(L{double{120.0}}, 19);

  // negative wraps to the upper side
  ASSERT_EQ(L{double{-5.0}}, 96);

  // signed-range wrap
  using S = inside<{-50, 50}, wrap>;
  // 75 wraps once: 75 - 101 = -26.
  ASSERT_EQ(S{double{75.0}}, -26);
}

//---------------------------------------------------------------------------
// Bug D — rational.hpp:260
//
// `gcd(rational, rational)` calls `std::lcm` on |Denominator|s without an
// overflow check and casts the result to imax via static_cast. Large
// denominators silently wrap. The fix is to make `gcd` return
// `std::expected<rational, errc>` and detect the overflow.
//
// Trigger: lcm(2^62, 3) = 3 * 2^62. That fits in umax (≈1.38e19) but
// exceeds imax_max (≈9.22e18). After the cast to imax it goes negative,
// producing a bogus rational.
//---------------------------------------------------------------------------
// Bug D: gcd lcm overflow propagates to grid::operator+
TEST(StorageBugsTest, bug_d_gcd_lcm_overflow_propagates_to_grid_operator_plus)
{
  // Use grid arithmetic since gcd's return type changes — the error
  // surfaces at grid::operator+ which already returns expected<grid, errc>.
  constexpr auto big   = rational{1u, imax{1} << 62};
  constexpr auto third = rational{1u, 3};

  constexpr grid g1{interval{0_r, 1_r}, big};
  constexpr grid g2{interval{0_r, 1_r}, third};

  static_assert(!((g1 + g2).has_value()));
  static_assert((g1 + g2).error() == errc::overflow);
}

#ifndef BEMAN_INSIDE_MATH_CORDIC
//---------------------------------------------------------------------------
// Bug E — grid.hpp double_exact / arithmetic.
//
// `f64` (double-backed) arithmetic silently diverged from the exact grid
// arithmetic whenever a result needed more than double's 53-bit significand.
// `dyadic_grid<G>` (the old storage guard) checks only power-of-two
// denominators; it ignores the significand. A f64 `×` whose product grid
// outgrows 2^53 (notch = N_L·N_R) dropped the low bits.
//
// Fix: `f64` is selected only on `double_exact` grids; an op whose result
// grid isn't double-exact drops `f64` and falls back to exact storage, so the
// result equals the exact rational product.
//---------------------------------------------------------------------------
// Bug E: f64 * stays exact (drops f64 when product exceeds 2^53)
TEST(StorageBugsTest, bug_e_f64_stays_exact_drops_f64_when_product_exceeds_2_53)
{
  using U = inside<{{0, 4}, per<(1u << 26)>}, f64>;   // exact operand (f=26)
  static_assert(std::is_same_v<U::raw_type, double>);

  const U a = 4.0 - std::ldexp(1.0, -26);                  // index 2^28-1, exact
  auto p = a * a;                                          // product grid f=52 > 53 bits
  static_assert(!std::is_same_v<decltype(p)::raw_type, double>);   // f64 dropped
  const rational ar = static_cast<rational>(a);
  ASSERT_TRUE(static_cast<rational>(p) == *(ar * ar));
}

//---------------------------------------------------------------------------
// Bug F — division.hpp f64 path.
//
// Real `÷0` stored a bare `inf` (snap_double then did static_cast<imax>(inf),
// UB), bypassing the error vocabulary. Fix: f64 division reports zero like
// every other path — the return widens to expected<result, errc> when the
// divisor grid can be zero (errc::division_by_zero on a zero divisor), and the
// expected-lift carries that cause on through a chain.
//---------------------------------------------------------------------------
// Bug F: f64 div-by-zero is reported, not a silent inf
TEST(StorageBugsTest, bug_f_f64_div_by_zero_is_reported_not_a_silent_inf)
{
  using N  = inside<{{1, 4}, per<1024>}, f64>;
  using Dz = inside<{{0, 4}, per<1024>}, f64>;   // divisor grid spans zero

  auto q = N{3.0} / Dz{0.0};
  ASSERT_FALSE(q.has_value());                       // an error — not inf
  ASSERT_EQ(q.error(), errc::division_by_zero);

  auto en = []() -> std::expected<N, errc> { return N{3.0}; };
  auto z = en() / Dz{0.0};
  ASSERT_FALSE(z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}
#endif // !BEMAN_INSIDE_MATH_CORDIC

//---------------------------------------------------------------------------
// 2026-07: fp-derived rational store on a snap grid with |Lower| ≫ 1. The
// cold store path forms (rhs − Lower)/Notch exactly; with a full-mantissa
// double source (den 2^54) that once dereferenced an empty result (terminate
// through the noexcept math engines). Now: offsets that fit 64 bits go
// through the rescued rational path, and offsets beyond it go through the
// 128-bit rounded store (wide_offset_quotient) — both land on the correctly
// rounded slot.
//---------------------------------------------------------------------------
// fp-derived rational store on a wide snap grid uses the 128-bit path
TEST(StorageBugsTest, fp_derived_rational_store_on_a_wide_snap_grid_uses_the_128_bit_path)
{
  using wide = inside<{{-1024, 1024}, per<16384>}, round_nearest>;

  {
    SCOPED_TRACE("negative value: offset fits after the 128-bit add rescue");
    wide slot{};
    slot = rational{umax{9006646171630191}, imax{-18014398509481984}};  // ≈ -0.4999693
    // ·16384 = -8191.49692… → round_nearest → -8191/16384
    ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{-16384}}));
  }

  {
    SCOPED_TRACE("positive value: exact offset needs > 64 bits → 128-bit rounded store");
    wide slot{};
    slot = rational{umax{9006646171630191}, imax{18014398509481984}};   // ≈ +0.4999693
    // (1024 + v)·16384 = 16785407.49692… → round_nearest → slot 16785407
    // → value 8191/16384 (0.49993896…, the nearest grid point)
    ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{16384}}));
  }

  // (No constexpr section: at constant evaluation the transient rational
  // overflow surfaces as the intentional constexpr_error build diagnostic
  // before the wide fallback can engage — the 128-bit path is runtime-only
  // in practice, though itself constexpr-capable.)

  {
    SCOPED_TRACE("strict policy off-notch in the wide regime → rounding_error");
    using strict = inside<{{-1024, 1024}, per<16384>}>;   // checked, no round flag
    strict slot{};
    try
    {
      slot = rational{umax{9006646171630191}, imax{18014398509481984}};
      FAIL() << "expected the default handler to throw";
    }
    catch (inside_error const& e) { ASSERT_EQ(e.Code, errc::rounding_error); }
  }
}

#ifndef BEMAN_INSIDE_MATH_CORDIC   // `f64` storage is compiled out under the integer engine
//---------------------------------------------------------------------------
// A `f64` (double-raw) source through every store path, and one-shot
// policies on a `f64` target. The raw is the value, not a notch index.
//---------------------------------------------------------------------------
namespace
{
  using RealSmall = beman::inside::inside<{{0, 4}, beman::inside::per<4>}, beman::inside::f64>;
  using RealWide  = beman::inside::inside<{{-8, 8}, beman::inside::per<4>}, beman::inside::f64>;
  using Int10     = beman::inside::inside<{0, 10}>;
}

TEST(StorageBugsTest, f64_source_into_integer_grid_reads_the_value)
{
  using namespace beman::inside;
  const RealWide half = RealWide::from_raw(2.5);
  EXPECT_EQ(detail::to_value(clamp_round<Int10>(half)), 3);
  Int10 i{0};
  i.with_snap<round_nearest>() = half;
  EXPECT_EQ(detail::to_value(i), 3);
  Int10 j = (half * just<2>).with_snap();
  EXPECT_EQ(detail::to_value(j), 5);
}

TEST(StorageBugsTest, one_shot_policy_applies_to_f64_target)
{
  using namespace beman::inside;
  const RealWide big = RealWide::from_raw(7.5), low = RealWide::from_raw(-1.0);
  EXPECT_EQ(clamp_cast<RealSmall>(big).raw(), 4.0);
  EXPECT_EQ(clamp_cast<RealSmall>(low).raw(), 0.0);
  EXPECT_EQ(wrap_cast<RealSmall>(big).raw(), 7.5 - 4.25);
  RealSmall r = RealSmall::from_raw(1.0);
  r.with_clamp() = big;
  EXPECT_EQ(r.raw(), 4.0);
}

TEST(StorageBugsTest, try_make_reports_on_checked_f64_target)
{
  using namespace beman::inside;
  using Checked = inside<{{0, 4}, per<4>}, f64 | checked>;
  EXPECT_EQ(Checked::try_make(RealWide::from_raw(7.5)).error(), errc::overflow);
}
#endif

//---------------------------------------------------------------------------
// Comparing integer storage with a floating or wide unsigned scalar must not
// truncate or wrap the scalar.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, scalar_comparison_does_not_truncate_the_scalar)
{
  using namespace beman::inside;
  inside<{-10, 10}> one{1}, minus_one{-1};
  static_assert(detail::value_raw<decltype(one)>);
  EXPECT_FALSE(one == 1.5);
  EXPECT_TRUE(one < 1.5);
  EXPECT_TRUE(minus_one > -1.5);
  EXPECT_TRUE(one == 1.0);
  EXPECT_TRUE(one < std::numeric_limits<std::uint64_t>::max());
}

#ifndef BEMAN_INSIDE_MATH_CORDIC
// ++ / += point on `f64` storage adds the value, not a notch count to the raw.
TEST(StorageBugsTest, increment_on_f64_storage)
{
  using namespace beman::inside;
  using rl = inside<{{-4, 4}, per<256>}, f64 | round_nearest>;
  rl x{rational{3, 2}};
  ++x;
  EXPECT_EQ(x.raw(), 2.5);
  x += 1_ins;
  EXPECT_EQ(x.raw(), 3.5);
  --x;
  EXPECT_EQ(x.raw(), 2.5);
}
#endif

// The noexcept conversion predicates classify non-finite input instead of raising.
TEST(StorageBugsTest, predicates_handle_non_finite_input)
{
  using namespace beman::inside;
  using B = inside<{0, 10}>;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_TRUE(conversion_overflows<B>(nan));
  EXPECT_TRUE(conversion_overflows<B>(-inf));
  EXPECT_FALSE(conversion_rounds<B>(nan));
  EXPECT_TRUE(conversion_is_lossy<B>(inf));
  EXPECT_FALSE(conversion_is_lossy<B>(3.0));
  EXPECT_TRUE(conversion_rounds<B>(3.5));
}
