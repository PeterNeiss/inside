// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Corner-case tests closing coverage holes surfaced by the gcov build
// (configure a Debug build with --coverage, e.g. the CI's Debug.Coverage cell).
//
// Each TEST names the library file:line(s) it is meant to exercise. Many
// of these paths were already checked at *compile* time via static_assert —
// which gcov does not count — so the assertions here are deliberately runtime
// (plain ASSERT_*) to drive the instrumented code.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/predicates.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// casts.hpp:66 — checked_cast success return (was static_assert only)
//---------------------------------------------------------------------------
// checked_cast returns the value on the in-range happy path (runtime)
TEST(CoverageCornersTest, checked_cast_returns_the_value_on_the_in_range_happy_path_runtime) {
    using pct = inside<{0, 100}>;
    pct p     = checked_cast<pct>(42); // runtime call, not static_assert
    ASSERT_EQ(p, 42);

    using coarse = inside<{{0, 10}, 2}>;
    ASSERT_TRUE(checked_cast<coarse>(4) == coarse{4}); // on-notch, in-range
}

//---------------------------------------------------------------------------
// predicates.hpp:39 — conversion_rounds returns false out of range
//---------------------------------------------------------------------------
// conversion_rounds is false for out-of-range values (runtime)
TEST(CoverageCornersTest, will_conversion_trunc_is_false_for_out_of_range_values_runtime) {
    using coarse = inside<{{0, 10}, 2}>;
    // Out of range is overflow, not truncation: the predicate is false.
    ASSERT_FALSE(conversion_rounds<coarse>(11));
    ASSERT_FALSE(conversion_rounds<coarse>(-1));
    // Contrast: off-notch but in range is truncation.
    ASSERT_TRUE(conversion_rounds<coarse>(3));
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
namespace fpk = beman::inside::math::detail::fp;
namespace d {
// The full kernels' values.
double fp_cbrt(double x) {
    double b;
    return fpk::pow_k<fpk::kFullBits>::cbrt(x, b);
}
double fp_atan2(double y, double x) {
    double b;
    return fpk::atan_k<fpk::kFullBits>::atan2(y, x, b);
}
} // namespace d

//---------------------------------------------------------------------------
// detail/math_fp.hpp — cbrt's negative branch
// detail/math_fp.hpp — atan2 on the axes (x == 0)
//---------------------------------------------------------------------------
// dbl: cbrt of negatives and atan2 on the axes
TEST(CoverageCornersTest, dbl_cbrt_of_negatives_and_atan2_on_the_axes) {
    // cbrt(x<0) = -cbrt(-x). Determinism: exact golden outputs (the engine's own
    // polynomial is bit-identical across platforms; the full kernel's log as
    // Hi + Lo lands these cube roots exactly).
    ASSERT_EQ(d::fp_cbrt(-8.0), -2.0);
    ASSERT_EQ(d::fp_cbrt(-27.0), -3.0);
    ASSERT_EQ(d::fp_cbrt(27.0), 3.0);

    // atan2 with x == 0: the y>0 / y<0 / y==0 axis cases (exact constants).
    ASSERT_EQ((d::fp_atan2(1.0, 0.0)), 0x1.921fb54442d18p+0);   // +pi/2
    ASSERT_EQ((d::fp_atan2(-1.0, 0.0)), -0x1.921fb54442d18p+0); // -pi/2
    ASSERT_EQ((d::fp_atan2(0.0, 0.0)), 0.0);                    //  0
    // and the x<0 reflective branch for good measure
    ASSERT_EQ((d::fp_atan2(1.0, -1.0)), 0x1.2d97c7f3321d2p+1);   //  3pi/4
    ASSERT_EQ((d::fp_atan2(-1.0, -1.0)), -0x1.2d97c7f3321d2p+1); // -3pi/4
}
#endif // !BEMAN_INSIDE_MATH_NO_FP

//---------------------------------------------------------------------------
// lift.hpp:116 — binary expected-lift short-circuits on a RIGHT-side error
// (the left-error short-circuit at :114 was already covered).
//---------------------------------------------------------------------------
// expected-lift propagates a right-hand-side error
TEST(CoverageCornersTest, expected_lift_propagates_a_right_hand_side_error) {
    using num_t = inside<{0, 100}>;
    std::expected<num_t, errc> left{num_t{5}}; // valid
    std::expected<num_t, errc> right{std::unexpected{errc::overflow}};

    auto both = left + right; // lhs OK, rhs error -> rhs.error() wins
    ASSERT_FALSE(both.has_value());
    ASSERT_EQ(both.error(), errc::overflow);
}

//---------------------------------------------------------------------------
// rational.hpp:846-866 — operator<=> cross-multiply path at runtime
// (the existing ordering tests are static_assert; the runtime path, and the
//  lhs_neg branch at :863-864, were uncovered.)
//---------------------------------------------------------------------------
// rational spaceship cross-multiply, positive and negative
TEST(CoverageCornersTest, rational_spaceship_cross_multiply_positive_and_negative) {
    using std::strong_ordering;

    // Both denominators != 1, no overflow, positive operands.
    ASSERT_EQ(((rational{1u, 2} <=> rational{1u, 3})), strong_ordering::greater);
    ASSERT_EQ(((rational{1u, 3} <=> rational{1u, 2})), strong_ordering::less);
    ASSERT_EQ(((rational{2u, 6} <=> rational{1u, 3})), strong_ordering::equal);

    // Both negative -> the lhs_neg flip (B <=> A).
    ASSERT_EQ(((rational{1, -2} <=> rational{1, -3})), strong_ordering::less);    // -1/2 < -1/3
    ASSERT_EQ(((rational{1, -3} <=> rational{1, -2})), strong_ordering::greater); // -1/3 > -1/2
}

//---------------------------------------------------------------------------
// rational.hpp:305-306 — gcd()/lcm cofactor multiply overflows umax -> overflow
// (distinct from the imax-cap overflow at :307-308 exercised elsewhere: here
//  the product exceeds 2^64, not merely imax_max.)
//---------------------------------------------------------------------------
// rational gcd: lcm that overflows umax returns overflow
TEST(CoverageCornersTest, rational_gcd_lcm_that_overflows_umax_returns_overflow) {
    // Two odd, coprime denominators near 2^40; their product ~2^80 overflows umax,
    // so the cross-multiplication trap (not the imax cap) fires.
    imax a = (imax{1} << 40) + 1;
    imax b = (imax{1} << 40) + 3;
    ASSERT_FALSE((gcd(rational{1u, a}, rational{1u, b}).has_value()));
    ASSERT_EQ((gcd(rational{1u, a}, rational{1u, b}).error()), errc::overflow);
}

//---------------------------------------------------------------------------
// rational.hpp:610-613 — checked add, numerator SUM overflows after the
// (non-overflowing) cross-multiply on UNEQUAL denominators. The equal-
// denominator overflow (:547-550) and the cross-multiply overflow (:589-595)
// were already covered; this is the third, post-cross-multiply overflow.
//---------------------------------------------------------------------------
// rational add: numerator sum overflow on unequal denominators
TEST(CoverageCornersTest, rational_add_numerator_sum_overflow_on_unequal_denominators) {
    // Denominators 2 and 6 (odd numerators so neither reduces). After cross-
    // multiply A = num_a*3, B = num_b*1 each fit in umax, but A + B exceeds 2^64.
    rational a{5999999999999999999u, 2}; // odd numerator -> stays /2
    rational b{999999999999999997u, 6};  // coprime to 6   -> stays /6
    ASSERT_FALSE((a + b).has_value());
}

//---------------------------------------------------------------------------
// format.hpp:56-58 — decimal fraction needs leading-zero padding.
//---------------------------------------------------------------------------
// rational to_string zero-pads short decimal fractions
TEST(CoverageCornersTest, rational_to_string_zero_pads_short_decimal_fractions) {
    ASSERT_EQ((beman::inside::to_string(rational{1u, 16})), "0.0625"); // frac "625" -> "0625"
    ASSERT_EQ((beman::inside::to_string(rational{1u, 100})), "0.01");  // frac  "1"  -> "01"
    ASSERT_EQ((beman::inside::to_string(rational{3u, 16})), "0.1875");
}

//---------------------------------------------------------------------------
// math.hpp:158-166 — constexpr ldexp into the subnormal range, including the
// round-to-nearest-even ++mantissa at :165-166. Validated against std::ldexp.
//---------------------------------------------------------------------------
// ldexp into subnormal range matches std::ldexp (with rounding)
TEST(CoverageCornersTest, ldexp_into_subnormal_range_matches_std_ldexp_with_rounding) {
    const double mants[] = {1.0,
                            1.5,
                            1.9999999999,
                            0x1.fffffffffffffp0, // all-ones mantissa: forces round-up
                            0x1.5555555555555p0,
                            0x1.0000000000001p0};
    for (int e = -1080; e <= -1020; ++e)
        for (double m : mants)
            ASSERT_EQ((beman::inside::detail::ldexp(m, e)), (std::ldexp(m, e)));
}

//---------------------------------------------------------------------------
// math.hpp:193-194 — abs_fraction on a subnormal (no implicit leading 1)
// math.hpp:215-219 — magnitudes below ~2^-62: significand-drop / flush-to-zero
//---------------------------------------------------------------------------
// rational from subnormal and very small doubles
TEST(CoverageCornersTest, rational_from_subnormal_and_very_small_doubles) {
    // Subnormal input: exercises the e==0 branch, then collapses to 0 (drop>=64).
    ASSERT_EQ(rational{std::numeric_limits<double>::denorm_min()}, 0_r);

    // Normal but below 2^-62: the significand is shifted down and recovered by
    // the trailing reduction, yielding the exact dyadic fraction.
    ASSERT_EQ(rational{0x1p-40}, (rational{1u, imax{1} << 40}));

    // Far below the cap: drops to a hard zero.
    ASSERT_EQ(rational{0x1p-70}, 0_r);
}

//---------------------------------------------------------------------------
// inside.hpp:733-734 — operator/=(integral)   inside.hpp:744-745 — operator%=
//---------------------------------------------------------------------------
// compound /= and %= with a snap inside rhs (runtime)
TEST(CoverageCornersTest, compound_and_with_a_snap_inside_rhs_runtime) {
    // Integer division/modulo now flow through the insidable path; `snap`
    // gives the same C++ trunc-toward-zero / dividend-signed-remainder semantics
    // the old raw-int compound assigns had.
    using sb = inside<{-100, 100}, snap>;

    sb a{20};
    a /= sb{3}; // integer division, truncates toward zero
    ASSERT_EQ(a, 6);

    sb n{-20};
    n /= sb{3};
    ASSERT_EQ(n, -6);

    sb b{20};
    b %= sb{7};
    ASSERT_EQ(b, 6);

    sb m{-20};
    m %= sb{7};
    ASSERT_EQ(m, -6); // C++ remainder keeps the dividend's sign
}

//---------------------------------------------------------------------------
// inside.hpp — operator+=(insidable) result out of range, checked policy with
// no clamp/wrap handler -> report (throws).
//---------------------------------------------------------------------------
// checked += insidable overflow reports (throws)
TEST(CoverageCornersTest, checked_plus_insidable_overflow_reports_throws) {
    using c100 = inside<{0, 100}, checked>;
    c100 a{80};
    c100 b{50};
    ASSERT_THROW((void)(a += b), beman::inside::inside_error); // 130 not in [0,100]
}

//---------------------------------------------------------------------------
// assignment.hpp:500-505 — error_action on an out-of-interval *fractional* rhs.
// The existing on_error test assigns an integer (a different assign overload);
// this drives the fractional (double) overload.
//---------------------------------------------------------------------------
// on_error fires for an out-of-range double assignment
TEST(CoverageCornersTest, on_error_fires_for_an_out_of_range_double_assignment) {
    using c100 = inside<{0, 100}, checked>;
    c100 e{50};
    bool fired = false;
    e.on_error([&](auto& self, errc code, std::string_view msg) {
        fired = (code == errc::overflow) && !msg.empty();
        self  = 0;
    })         = 200.5; // fractional, out of [0,100]
    ASSERT_TRUE(fired);
    ASSERT_EQ(e, 0);
}

//---------------------------------------------------------------------------
// policy.hpp — action-decorated operator-=(insidable) on the NON-overflow path
// (existing tests only exercise the overflowing arm).
//---------------------------------------------------------------------------
// on_overflow compound subtract that does not overflow
TEST(CoverageCornersTest, on_overflow_compound_subtract_that_does_not_overflow) {
    using c100 = inside<{0, 100}, checked>;
    c100 acc{50};
    bool fired = false;
    acc.on_overflow([&](auto&, errc) { fired = true; }) -= 10_ins; // 40, no overflow
    ASSERT_FALSE(fired);
    ASSERT_EQ(acc, 40);
}

//---------------------------------------------------------------------------
// inside.hpp:161-163 — store_real out-of-range with a reporting policy
// (the clamp/wrap arms are covered elsewhere; the range_fail arm was not).
//---------------------------------------------------------------------------
// double store out of range: checked policy
TEST(CoverageCornersTest, double_store_out_of_range_checked_policy) {
    using rbc = inside<{{-1, 1}, per<1024>}, checked>;
    ASSERT_THROW((void)((rbc{5.0})), beman::inside::inside_error); // out of range -> report (throws)
}

//---------------------------------------------------------------------------
// inside -> inside store into a dyadic target.
//---------------------------------------------------------------------------
// inside -> dyadic conversion lands on the grid
TEST(CoverageCornersTest, inside_to_dyadic_conversion_lands_on_the_grid) {
    using src_t = inside<{-2, 2}>;                             // integer-backed source
    using rb    = inside<{{-2, 2}, per<1024>}, round_nearest>; // dyadic target

    src_t src{1};
    rb    dst = src; // insidable -> dyadic store
    ASSERT_EQ(double(dst), 1.0);

    src_t neg{-2};
    rb    dn = neg;
    ASSERT_EQ(double(dn), -2.0);
}

//---------------------------------------------------------------------------
// assignment.hpp:475-476 — off-notch fractional store on a policy with no
// rounding-mode flag and round_check()==false. `snap` would route
// through the has_round_flag arm (it is counted as a round flag); a plain
// `clamp` policy (not checked, not snap) takes the :476 else-branch
// and truncates an in-range off-notch value silently.
//---------------------------------------------------------------------------
// clamp | snap truncates an in-range off-notch fractional assignment; clamp
// alone handles only the range, so the off-notch value is a rounding error.
TEST(CoverageCornersTest, clamp_policy_truncates_an_in_range_off_notch_fractional_assignment) {
    using strict = inside<{{0, 10}, per<2>}, clamp>;
    ASSERT_EQ(strict::try_make(0.3).error(), errc::rounding_error);

    using b = inside<{{0, 10}, per<2>}, clamp | snap>; // notch 1/2
    b x{0};
    x = 0.3; // in range, off the 1/2 grid → truncates to 0
    ASSERT_EQ(x, 0);

    x = 0.9; // 0.9 → 1.8 half-notches → truncates to 1 half → 0.5
    ASSERT_TRUE((static_cast<rational>(x) == rational{1u, 2}));
}

//---------------------------------------------------------------------------
// generic.hpp:355-361 — raw_from_offset(imax) overload, reached from the
// integer fast path of math::fmod (cmath.hpp:1012, fmod_int_fast) with a
// signed offset. Needs non-rational integer grids and a divisor
// that excludes zero. math::fmod was otherwise only static_assert-tested.
//---------------------------------------------------------------------------
// math::fmod integer fast path (raw_from_offset imax)
TEST(CoverageCornersTest, math_fmod_integer_fast_path_raw_from_offset_imax) {
    using in_t  = inside<{{-8, 8}, per<16384>}, round_nearest>; // integer-backed
    using div_t = inside<{{1, 8}, per<16384>}, round_nearest>;  // excludes zero
    using out_t = inside<{{-8, 8}, per<16384>}, round_nearest>;

    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{7_r}, div_t{3_r})) == 1));
    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{-7_r}, div_t{3_r})) == -1)); // signed offset
    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{5.5_r}, div_t{2_r})) == rational{3u, 2}));
}

//---------------------------------------------------------------------------
// assignment.hpp:631-634 + generic.hpp:355-361 — inside -> inside store on the
// rational (non-integer-mapping) path where the target offset is NEGATIVE, so
// raw_from_offset() is reached through the negated branch.
//---------------------------------------------------------------------------
// cross-grid conversion of a negative off-notch value
TEST(CoverageCornersTest, cross_grid_conversion_of_a_negative_off_notch_value) {
    // notch 1/2 source -> notch 1/3 target: Factor = 3/2 (non-integer mapping),
    // so the rational store path runs; the negative value drives the
    // negative-denominator branch.
    using src_t = inside<{{-4, 4}, per<2>}>;
    using dst_t = inside<{{-4, 4}, per<3>}, snap>;

    src_t s{-1.5};
    dst_t d = s;
    // snap truncates toward ZERO in value space (matching the scalar store
    // path and div_rounded): -1.5 on the 1/3 grid → -4/3. (Was -5/3 when the old
    // path truncated the non-negative offset — i.e. toward -inf in value space.)
    ASSERT_TRUE((static_cast<rational>(d) == rational{4, -3}));

    src_t s2{1.5};
    dst_t d2 = s2; // +1.5 toward zero → 4/3 (unchanged)
    ASSERT_TRUE((static_cast<rational>(d2) == rational{4u, 3}));
}

// round_to_lattice on a fine notch over a wide range: the notch index of
// 2^61 − 1 on a 1/8 grid is 2^64 − 8, past imax. It must round in place, not
// wrap to a negative index (it returned −1 when the index was narrowed to
// imax), and a result past the rational range is errc::overflow.
TEST(CoverageCornersTest, round_to_lattice_keeps_notch_indices_past_imax) {
    using L       = inside<grid{{0, 1ll << 61}, per<8>}>;
    const auto v  = rational{(1ull << 61) - 1, 1};
    const auto nv = rational{(1ull << 61) - 1, -1};
    ASSERT_TRUE((round_to_lattice<L, policy<round_half_even>>(v) == v));
    ASSERT_TRUE((round_to_lattice<L, policy<round_floor>>(v) == v));
    ASSERT_TRUE((round_to_lattice<L, policy<round_floor>>(nv) == nv));
    ASSERT_TRUE((round_to_lattice<L, policy<round_ceil>>(nv) == nv));
    const auto off = rational{(1ull << 63) - 1, 4}; // a lattice point: index 2^64 − 2
    ASSERT_TRUE((round_to_lattice<L, policy<round_nearest>>(off) == off));
    // ceil(umax) on the 1/8 lattice is umax itself, but its index 8·umax is
    // past every rational: reported, not wrapped.
    ASSERT_TRUE((try_round_to_lattice<L, policy<round_ceil>>(rational{~0ull, 1}).error() == errc::overflow));
}

// The unanchored branch (Lower 1/16 off the 1/8 notch's multiples): small
// values round onto {1/16 + k/8}; a far value whose index passes imax, and
// whose lattice neighbours pass the rational range, reports overflow instead
// of a wrapped index.
TEST(CoverageCornersTest, round_to_lattice_unanchored_rounds_and_reports_overflow) {
    using U = inside<grid{{rational{1, 16}, rational{17, 16}}, per<8>}>;
    ASSERT_TRUE((round_to_lattice<U, policy<round_floor>>(rational{5, 1}) == rational{79, 16}));
    ASSERT_TRUE((round_to_lattice<U, policy<round_ceil>>(rational{5, 1}) == rational{81, 16}));
    ASSERT_TRUE((round_to_lattice<U, policy<round_nearest>>(rational{5, -1}) == rational{81, -16}));
    ASSERT_TRUE((round_to_lattice<U, policy<round_half_even>>(rational{5, 1}) == rational{81, 16})); // index 40
    ASSERT_TRUE(
        (try_round_to_lattice<U, policy<round_floor>>(rational{(1ull << 61) - 1, 1}).error() == errc::overflow));
}
