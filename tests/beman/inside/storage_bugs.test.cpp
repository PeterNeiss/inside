// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bugs surfaced by the 2026-05 post-fix audit. Each TEST here should
// fail on the unfixed build and pass after the corresponding fix lands.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/grid.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

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
TEST(StorageBugsTest, bug_a_rational_mixed_add_into_direct_storage_result) {
    using L = inside<{-5, 5}>;          // signed-direct
    using R = inside<{{-10, 10}, 0_r}>; // rational raw

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
TEST(StorageBugsTest, bug_b_signed_direct_multiplication_third_quadrant) {
    using L = inside<{-5, 5}>;                      // signed-direct, integer-aligned
    using R = inside<{{-10, 10}, rational{1u, 2}}>; // notch 1/2, not direct, not integer-aligned

    // lower_of<result> = upper_of<L> * lower_of<R> = 5 * -10 = -50 → third quadrant.
    // Without the fix, L{2} * R{1} produces value -3 instead of 2.
    static_assert(L{2} * R{rational{1u}} == rational{2u});
    static_assert(L{3} * R{rational{2u}} == rational{6u});
    static_assert(L{0} * R{rational{5u}} == rational{0u});
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
TEST(StorageBugsTest, bug_c_wrap_policy_fires_for_f64_rhs) {
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
TEST(StorageBugsTest, bug_d_gcd_lcm_overflow_propagates_to_grid_operator_plus) {
    // Use grid arithmetic since gcd's return type changes — the error
    // surfaces at grid::operator+ which already returns expected<grid, errc>.
    constexpr auto big   = rational{1u, imax{1} << 62};
    constexpr auto third = rational{1u, 3};

    constexpr grid g1{interval{0_r, 1_r}, big};
    constexpr grid g2{interval{0_r, 1_r}, third};

#if BEMAN_INSIDE_BIG_GRIDS
    // Big grid numbers: the notch gcd 1/(3·2^62) is exact.
    static_assert((g1 + g2)->Notch ==
                  detail::grid_rational{detail::big_int{1}, detail::big_int{3} * detail::big_int{imax{1} << 62}});
#else
    static_assert(!((g1 + g2).has_value()));
    static_assert((g1 + g2).error() == errc::overflow);
#endif
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
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
TEST(StorageBugsTest, bug_e_f64_stays_exact_drops_f64_when_product_exceeds_2_53) {
    using U = inside<{{0, 4}, per<(1u << 26)>}, f64>; // exact operand (f=26)
    static_assert(std::is_same_v<U::raw_type, double>);

    const U a = 4.0 - std::ldexp(1.0, -26);                        // index 2^28-1, exact
    auto    p = a * a;                                             // product grid f=52 > 53 bits
    static_assert(!std::is_same_v<decltype(p)::raw_type, double>); // f64 dropped
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
TEST(StorageBugsTest, bug_f_f64_div_by_zero_is_reported_not_a_silent_inf) {
    using N  = inside<{{1, 4}, per<1024>}, f64>;
    using Dz = inside<{{0, 4}, per<1024>}, f64>; // divisor grid spans zero

    auto q = N{3.0} / Dz{0.0};
    ASSERT_FALSE(q.has_value()); // an error — not inf
    ASSERT_EQ(q.error(), errc::division_by_zero);

    auto en = []() -> std::expected<N, errc> { return N{3.0}; };
    auto z  = en() / Dz{0.0};
    ASSERT_FALSE(z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}
#endif // !BEMAN_INSIDE_MATH_NO_FP

//---------------------------------------------------------------------------
// 2026-07: fp-derived rational store on a snap grid with |Lower| ≫ 1. The
// cold store path forms (rhs − Lower)/Notch exactly; with a full-mantissa
// double source (den 2^54) that once dereferenced an empty result (terminate
// through the noexcept math engines). Now: offsets that fit 64 bits go
// through the rescued rational path, and offsets beyond it go through the
// exact wide index (exact_index) — both land on the correctly rounded slot.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, fp_derived_rational_store_on_a_wide_snap_grid_uses_the_exact_wide_path) {
    using wide = inside<{{-1024, 1024}, per<16384>}, round_nearest>;

    {
        SCOPED_TRACE("negative value: offset fits after the 128-bit add rescue");
        wide slot{};
        slot = rational{umax{9006646171630191}, imax{-18014398509481984}}; // ≈ -0.4999693
        // ·16384 = -8191.49692… → round_nearest → -8191/16384
        ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{-16384}}));
    }

    {
        SCOPED_TRACE("positive value: exact offset needs > 64 bits → exact wide store");
        wide slot{};
        slot = rational{umax{9006646171630191}, imax{18014398509481984}}; // ≈ +0.4999693
        // (1024 + v)·16384 = 16785407.49692… → round_nearest → slot 16785407
        // → value 8191/16384 (0.49993896…, the nearest grid point)
        ASSERT_EQ(rational{slot}, (rational{umax{8191}, imax{16384}}));
    }

    // (No constexpr section: at constant evaluation the transient rational
    // overflow surfaces as the intentional constexpr_error build diagnostic
    // before the wide fallback can engage — the wide path is runtime-only in
    // practice, though itself constexpr-capable.)

    {
        SCOPED_TRACE("strict policy off-notch in the wide regime → rounding_error");
        using strict = inside<{{-1024, 1024}, per<16384>}>; // checked, no round flag
        strict slot{};
        try {
            slot = rational{umax{9006646171630191}, imax{18014398509481984}};
            FAIL() << "expected the default handler to throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::rounding_error);
        }
    }
}

#ifndef BEMAN_INSIDE_MATH_NO_FP // `f64` storage is compiled out under the integer engine
//---------------------------------------------------------------------------
// A `f64` (double-raw) source through every store path, and one-shot
// policies on a `f64` target. The raw is the value, not a notch index.
//---------------------------------------------------------------------------
namespace {
using RealSmall = beman::inside::inside<{{0, 4}, beman::inside::per<4>}, beman::inside::f64>;
using RealWide  = beman::inside::inside<{{-8, 8}, beman::inside::per<4>}, beman::inside::f64>;
using Int10     = beman::inside::inside<{0, 10}>;
} // namespace

TEST(StorageBugsTest, f64_source_into_integer_grid_reads_the_value) {
    using namespace beman::inside;
    const RealWide half = RealWide::from_raw(2.5);
    EXPECT_EQ(detail::to_value(clamp_round<Int10>(half)), 3);
    Int10 i{0};
    i.with_snap<round_nearest>() = half;
    EXPECT_EQ(detail::to_value(i), 3);
    Int10 j = (half * just<2>).with_snap();
    EXPECT_EQ(detail::to_value(j), 5);
}

TEST(StorageBugsTest, one_shot_policy_applies_to_f64_target) {
    using namespace beman::inside;
    const RealWide big = RealWide::from_raw(7.5), low = RealWide::from_raw(-1.0);
    EXPECT_EQ(clamp_cast<RealSmall>(big).raw(), 4.0);
    EXPECT_EQ(clamp_cast<RealSmall>(low).raw(), 0.0);
    EXPECT_EQ(wrap_cast<RealSmall>(big).raw(), 7.5 - 4.25);
    RealSmall r    = RealSmall::from_raw(1.0);
    r.with_clamp() = big;
    EXPECT_EQ(r.raw(), 4.0);
}

TEST(StorageBugsTest, try_make_reports_on_checked_f64_target) {
    using namespace beman::inside;
    using Checked = inside<{{0, 4}, per<4>}, f64 | checked>;
    EXPECT_EQ(Checked::try_make(RealWide::from_raw(7.5)).error(), errc::overflow);
}
#endif

//---------------------------------------------------------------------------
// Comparing integer storage with a floating or wide unsigned scalar must not
// truncate or wrap the scalar.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, scalar_comparison_does_not_truncate_the_scalar) {
    using namespace beman::inside;
    inside<{-10, 10}> one{1}, minus_one{-1};
    static_assert(detail::value_raw<decltype(one)>);
    EXPECT_FALSE(one == 1.5);
    EXPECT_TRUE(one < 1.5);
    EXPECT_TRUE(minus_one > -1.5);
    EXPECT_TRUE(one == 1.0);
    EXPECT_TRUE(one < std::numeric_limits<std::uint64_t>::max());
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
// ++ / += point on `f64` storage adds the value, not a notch count to the raw.
TEST(StorageBugsTest, increment_on_f64_storage) {
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
TEST(StorageBugsTest, predicates_handle_non_finite_input) {
    using namespace beman::inside;
    using B          = inside<{0, 10}>;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(conversion_overflows<B>(nan));
    EXPECT_TRUE(conversion_overflows<B>(-inf));
    EXPECT_FALSE(conversion_rounds<B>(nan));
    EXPECT_TRUE(conversion_is_lossy<B>(inf));
    EXPECT_FALSE(conversion_is_lossy<B>(3.0));
    EXPECT_TRUE(conversion_rounds<B>(3.5));
}

//---------------------------------------------------------------------------
// 2026-10 defect pass. A grid does not fix the raw encoding: `indexed`,
// `direct`, `f64` and the width flags pick it per policy, so two insides on
// the same grid may store the same value differently.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, same_grid_different_encoding_compares_by_value) {
    using A = inside<{{10, 20}, 1}, indexed>;
    using B = inside<{{10, 20}, 1}, direct>;
    static_assert(A{15} == B{15});
    static_assert(A{12} < B{15});

    using G1 = inside<{{-5, 5}, 1}, indexed>;
    using G2 = inside<{{-5, 5}, 1}>; // deduced: direct int8
    static_assert(G1{2} == G2{2});
    static_assert(G1{-3} < G2{2});
    static_assert(!(G1{2} != G2{2}));

#ifndef BEMAN_INSIDE_MATH_NO_FP
    using C = inside<{{0, 1}, per<4>}, f64>;
    using D = inside<{{0, 1}, per<4>}>; // deduced: index uint8
    static_assert(C{rational{1, 2}} == D{rational{1, 2}});
    static_assert(C{rational{1, 4}} < D{rational{1, 2}});
#endif

    G1 a{-4};
    G2 b{3};
    EXPECT_LT(a, b);
    EXPECT_NE(a, b);
}

TEST(StorageBugsTest, same_grid_different_encoding_assigns_by_value) {
    using G1 = inside<{{-5, 5}, 1}, indexed>;
    using G2 = inside<{{-5, 5}, 1}>;
    static_assert(G1{G2{2}}.as<int>() == 2);
    static_assert(G2{G1{2}}.as<int>() == 2);

    using H1 = inside<{{10, 20}, 1}, direct>;
    using H2 = inside<{{10, 20}, 1}>;
    static_assert(H2{H1{15}}.as<int>() == 15);
    static_assert(H1{H2{15}}.as<int>() == 15);

    static_assert(unchecked_cast<G1>(G2{2}).as<int>() == 2);

    G1 x{0};
    x = G2{-5};
    EXPECT_EQ(x.as<int>(), -5);
    EXPECT_EQ(x.raw(), 0u);
}

//---------------------------------------------------------------------------
// A 64-bit unsigned source above INT64_MAX must not be read as negative.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, uint64_source_above_int64_max) {
    static constexpr unsigned long long big = ~0ull;

    constexpr auto checked = [] {
        errc                   ec{};
        inside<{{-10, 10}, 1}> x{3};
        x.policy(ec) = big;
        return std::pair{x.as<int>(), ec};
    }();
    static_assert(checked.first == 3 && checked.second == errc::overflow);

    static_assert(clamp_cast<inside<{{0, 10}, 1}>>(1ull << 63).as<int>() == 10);
    static_assert(clamp_cast<inside<{{-10, 10}, 1}>>(big).as<int>() == 10);

    // (2^64 − 1 − Lower) mod 21 + Lower
    static_assert(wrap_cast<inside<{{-10, 10}, 1}>>(big).as<long>() == static_cast<long>((big % 21 + 10) % 21) - 10);
    static_assert(wrap_cast<inside<{{5, 9}, 1}>>(big).as<long>() == static_cast<long>((big - 5) % 5) + 5);
    static_assert(wrap_cast<inside<{{5, 9}, 1}>>(-7).as<int>() == 8);

    // The on_clamp overshoot saturates instead of overflowing.
    inside<{{-10, 10}, 1}> c{0};
    imax                   overshoot                  = 0;
    c.on_clamp([&](auto&, imax o) { overshoot = o; }) = big;
    EXPECT_EQ(c.as<int>(), 10);
    EXPECT_EQ(overshoot, std::numeric_limits<imax>::max());
    c.on_clamp([&](auto&, imax o) { overshoot = o; }) = std::numeric_limits<imax>::max();
    EXPECT_EQ(overshoot, std::numeric_limits<imax>::max() - 10);
}

//---------------------------------------------------------------------------
// A double of magnitude 2^64 or more has no 64-bit rational form; it used to
// convert to a wrapped value (2^64 → 0) and be stored silently.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, huge_double_source) {
    constexpr auto rounded = [](double v) {
        errc                                 ec{};
        inside<{{0, 100}, 1}, round_nearest> x{5};
        x.policy(ec) = v;
        return std::pair{x.as<int>(), ec};
    };
    static_assert(rounded(0x1p64).first == 5 && rounded(0x1p64).second == errc::overflow);
    static_assert(rounded(1e300).second == errc::overflow);
    static_assert(rounded(-1e300).second == errc::overflow);

    constexpr auto wrapped = [](double v) {
        errc                      ec{};
        inside<{{0, 9}, 1}, wrap> x{5};
        x.policy(ec) = v;
        return std::pair{x.as<long>(), ec};
    };
    static_assert(wrapped(0x1p64).second == errc::overflow);
    static_assert(wrapped(0x1p62).first == static_cast<long>((1ull << 62) % 10));

    using third = inside<{{0, rational{1, 3}}, rational{1, 3}}, clamp>;
    static_assert(rational{third{1e300}} == rational{1, 3});
    static_assert(rational{third{-1e300}} == 0);

    constexpr inside<{{0, 10}, per<4>}> x{1};
    static_assert(x < 0x1p64 && x > -0x1p64 && x < 1e300 && !(x == 1e300));
    static_assert(conversion_overflows<inside<{0, 10}>>(0x1p64));
    static_assert(!conversion_rounds<inside<{0, 10}>>(0x1p64));

    EXPECT_THROW((void)rational{0x1p64}, inside_error);
    EXPECT_EQ(rational{0x1p63}, rational{1ull << 63});
}

//---------------------------------------------------------------------------
// clamp / wrap take an integral source whose whole type range misses the grid.
//---------------------------------------------------------------------------
TEST(StorageBugsTest, clamp_wrap_from_disjoint_integral_type) {
    static_assert(inside<{{1000, 2000}, 1}, clamp>{std::uint8_t{5}}.as<int>() == 1000);
    static_assert(inside<{{1000, 1009}, 1}, wrap>{std::uint8_t{5}}.as<int>() == 1005);
    static_assert(clamp_cast<inside<{1000, 2000}>>(std::uint8_t{5}).as<int>() == 1000);
}

//---------------------------------------------------------------------------
// A fixed-width flag pins a point's wire layout (value storage).
//---------------------------------------------------------------------------
TEST(StorageBugsTest, width_flag_on_point_grid) {
    using P5 = inside<grid{5}, u8>;
    static_assert(sizeof(P5) == 1);
    static_assert(P5{5}.raw() == 5 && P5{5} == 5);
    using M7 = inside<grid{-7}, i16>;
    static_assert(M7{-7}.raw() == -7 && M7{-7} == -7);
    static_assert(P5{5} + inside<{0, 10}>{3} == 8);

    inside<{0, 10}> t{3};
    t += P5{5};
    EXPECT_EQ(t, 8);
}

// rational::to<T> checks that the quotient fits T.
TEST(StorageBugsTest, rational_to_checks_width) {
    static_assert(rational{300}.to<std::uint8_t>().error() == errc::overflow);
    static_assert(rational{255}.to<std::uint8_t>().value() == 255);
    static_assert(rational{-1}.to<std::uint8_t>().error() == errc::domain_error);
}

// A policy_ref compound /= or %= with a zero divisor reports (or, under
// ignore_zero, no-ops) like the member operator, and never divides by zero.
TEST(StorageBugsTest, policy_ref_zero_divisor_does_not_divide) {
    using X = inside<{0, 10}, checked | snap>;
    using Z = inside<{0, 10}, checked | snap | ignore_zero>;
    X b{6};
    b.policy<ignore_zero>() /= X{0};
    b.policy<ignore_zero>() %= X{0};
    b.policy() /= Z{0};
    EXPECT_EQ(b, 6);
    EXPECT_THROW(b.policy() /= X{0}, inside_error);
    EXPECT_THROW(b.policy() %= X{0}, inside_error);
    errc ec{};
    b.policy(ec) %= X{0};
    EXPECT_EQ(ec, errc::division_by_zero);
    EXPECT_EQ(b, 6);
    b.policy() /= X{3};
    EXPECT_EQ(b, 2);
}

//---------------------------------------------------------------------------
// 2026-10 defect pass, round 2: integer grids reaching past int64. {0, 2^64−1}
// stores in a uint64 (value raw); its negation, {−(2^64−1), 0}, in a uint64
// index. Every imax fast path must step aside for them.
//---------------------------------------------------------------------------
namespace {
constexpr umax kUM   = std::numeric_limits<umax>::max();
constexpr imax kIMin = std::numeric_limits<imax>::min();
constexpr imax kIMax = std::numeric_limits<imax>::max();
using U64            = inside<{{0, rational{kUM}}, 1}>;
using U64W           = inside<{{0, rational{kUM}}, 1}, wrap>;
using I64            = inside<{{kIMin, kIMax}, 1}>;
using I64W           = inside<{{kIMin, kIMax}, 1}, wrap>;
using I64C           = inside<{{kIMin, kIMax}, 1}, clamp>;
using Small          = inside<{0, 10}>;
} // namespace

TEST(StorageBugsTest, grid_past_int64_constructs_compares_and_converts) {
    static_assert(U64{5} == 5 && U64{5} < 6 && U64{5} > 4); // signed source
    static_assert(U64{kUM} > 5 && U64{kUM} == kUM && U64{kUM} > Small{3});
    static_assert(U64{1ull << 63} < U64{kUM});
    static_assert(U64{Small{7}} == 7);
    static_assert(U64{kUM}.to<unsigned long long>().value() == kUM);
    static_assert(U64{3}.to<int>().value() == 3);
    static_assert(U64{kUM}.to<long long>().error() == errc::overflow);
    static_assert(clamp_cast<Small>(U64{kUM}) == 10);

    constexpr auto narrowed = [] {
        errc  ec{};
        Small s{1};
        s.policy(ec) = U64{kUM};
        return std::pair{s == 1, ec};
    }();
    static_assert(narrowed.first && narrowed.second == errc::overflow);

    U64 big{kUM};
    EXPECT_GT(big, Small{10});
    EXPECT_EQ(big, kUM);
}

TEST(StorageBugsTest, grid_past_int64_increments_and_negates) {
    constexpr auto inc = [] {
        U64 w{kUM - 1};
        ++w;
        return w == kUM;
    }();
    static_assert(inc);
    constexpr auto wrap_up = [] {
        U64W w{kUM};
        ++w;
        return w == 0;
    }();
    constexpr auto wrap_down = [] {
        U64W w{0};
        --w;
        return w == kUM;
    }();
    constexpr auto wrap_sub = [] {
        U64W w{kUM};
        w -= Small{10};
        return w == kUM - 10;
    }();
    constexpr auto wrap_neg = [] {
        U64W w{5};
        w = inside<{-20, 20}>{-1};
        return w == kUM;
    }();
    static_assert(wrap_up && wrap_down && wrap_sub && wrap_neg);

    using N = U64::negative;                 // {−(2^64−1), 0}
    static_assert(sizeof(N::raw_type) == 8); // was int8: trunc(Lower) wrapped
    static_assert(-U64{5} == -5 && -U64{kUM} == -rational{kUM});
    static_assert(-(-U64{kUM}) == U64{kUM});

    static_assert(U64{kUM} - U64{kUM - 3} == 3); // result grid spans 2^65
    static_assert(U64{kUM} * just<1> == kUM);

    U64 x{kUM};
    EXPECT_THROW(++x, inside_error);
    EXPECT_EQ(x, kUM);
}

// += / -= / ++ on the full int64 range: the raw add must not overflow imax.
TEST(StorageBugsTest, full_int64_grid_compound_ops) {
    constexpr auto a = [] {
        I64W x{kIMax};
        ++x;
        return x.raw();
    }();
    constexpr auto b = [] {
        I64W x{kIMin};
        --x;
        return x.raw();
    }();
    constexpr auto c = [] {
        I64C x{kIMax};
        ++x;
        return x.raw();
    }();
    constexpr auto d = [] {
        I64W x{kIMax};
        x += I64{5};
        return x.raw();
    }();
    constexpr auto e = [] {
        I64W x{kIMin};
        x -= I64{1};
        return x.raw();
    }();
    static_assert(a == kIMin && b == kIMax && c == kIMax && d == kIMin + 4 && e == kIMax);
    static_assert(I64W{kUM} == -1);

    I64 y{kIMax};
    EXPECT_THROW(++y, inside_error);
    EXPECT_EQ(y, kIMax);
}

// A value-raw source with Lower ≠ 0 mapped onto a coarser notch: the affine
// map read the raw (the value) as a 0-based offset.
TEST(StorageBugsTest, value_raw_source_affine_mapping) {
    using R = inside<{-5, 5}>; // int8 value raw
    using L = inside<{{-6, 6}, 3}, round_nearest>;
    static_assert(detail::value_raw<R>);
    static_assert(rational{L{R{-5}}} == -6);
    static_assert(rational{L{R{-4}}} == -3);
    static_assert(rational{L{R{4}}} == 3);
    static_assert(rational{L{R{5}}} == 6);
    using LX = inside<{{-6, 6}, 3}, round_nearest | exact>;
    static_assert(rational{LX{R{-5}}} == -6);

    L l{0};
    l = R{-2};
    EXPECT_EQ(rational{l}, -3);
}

// An exact value stored into f64 storage rounds once, onto the grid: the
// double nearest to it may sit on a rounding boundary the exact value is not
// on (4096/961 · 2^48 is …523.388, its double …523.5, a tie).
TEST(StorageBugs, f64_storage_rounds_the_exact_value_not_its_double) {
#ifndef BEMAN_INSIDE_MATH_NO_FP
    using f48 = inside<{{-8, 8}, per<(std::uint64_t{1} << 48)>}, f64>;
    const rational want{1199710202504523ull, std::int64_t{1} << 48};
    EXPECT_EQ((rational{f48{rational{4096, 961}}}), want);
    f48 a{0};
    a = rational{4096, 961};
    EXPECT_EQ(rational{a}, want);
    // An inside source whose values are not doubles exactly takes the same route.
    using thirds = inside<{{0, 8}, rational{1, 961}}>;
    a = thirds{rational{4096, 961}};
    EXPECT_EQ(rational{a}, want);

    // Directed modes: 1 + 2^-60 is above the grid point 1.0, its double is 1.0.
    using up   = inside<{{0, 2}, per<1024>}, f64 | round_ceil>;
    using down = inside<{{0, 2}, per<1024>}, f64 | round_floor>;
    const rational above{(std::uint64_t{1} << 60) + 1, std::int64_t{1} << 60};
    const rational below{(std::uint64_t{1} << 60) - 1, std::int64_t{1} << 60};
    EXPECT_EQ(rational{up{above}}, (rational{1025, 1024}));
    EXPECT_EQ(rational{down{below}}, (rational{1023, 1024}));
    EXPECT_EQ(rational{up{rational{1}}}, rational{1});
#endif
}
