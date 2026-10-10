// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Cross-path consistency: the same value, policy and grid must give the same
// result whichever code path (storage kind, assignment source, fast path,
// engine) handles it.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
rational q(imax n, imax d = 1) { return rational{n, d}; }
} // namespace

//---------------------------------------------------------------------------
// One tie rule (half away from zero) and honoured rounding modes.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, ties_round_half_away_from_zero) {
    using F = inside<{{-4, 4}, per<2>}, round_nearest>;
    EXPECT_EQ(rational{F{q(-1, 4)}}, rational{-0.5});
    EXPECT_EQ(rational{F{q(1, 4)}}, rational{0.5});
}

TEST(ConsistencyTest, store_honours_rounding_mode) {
    using Fl = inside<{{-4, 4}, per<2>}, round_floor>;
    using Ce = inside<{{-4, 4}, per<2>}, round_ceil>;
    using He = inside<{{-4, 4}, per<2>}, round_half_even>;
    EXPECT_EQ(rational{Fl{q(2, 5)}}, rational{0.0});
    EXPECT_EQ(rational{Fl{q(-1, 10)}}, rational{-0.5});
    EXPECT_EQ(rational{Ce{q(1, 10)}}, rational{0.5});
    EXPECT_EQ(rational{He{q(3, 4)}}, rational{1.0}); // 1.5 notches → 2 (even)
    EXPECT_EQ(rational{He{q(1, 4)}}, rational{0.0}); // 0.5 notches → 0 (even)
}

// A math result exactly on a tie rounds like the assignment path.
TEST(ConsistencyTest, math_tie_rule_matches_assignment) {
    using O  = inside<{-10, 10}, round_nearest>;
    using In = inside<{{-1, 1}, per<64>}, round_nearest>;
    EXPECT_EQ(rational{math::sqrt_into<O>(In{q(1, 4)}).value()}, rational{O{q(1, 2)}}); // √(1/4) = 1/2 → 1
    EXPECT_EQ(rational{math::cbrt_into<O>(In{q(-1, 8)})}, rational{O{q(-1, 2)}});       // ∛(−1/8) = −1/2 → −1
    EXPECT_EQ(rational{math::cbrt_into<O>(In{q(-1, 8)})}, q(-1));
}

//---------------------------------------------------------------------------
// An integer source on a grid it is not on rounds (or reports) exactly like the
// same value given as a rational — never a silent truncation.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, integer_source_off_notch_rounds_like_rational_source) {
    using N2  = inside<{{0, 10}, 2}, round_nearest>;
    using N2f = inside<{{0, 10}, 2}, round_floor>;
    EXPECT_EQ(rational{N2{3}}, rational{N2{q(3)}});
    EXPECT_EQ(rational{N2{3}}, q(4)); // 1.5 notches → 2 (half away)
    EXPECT_EQ(rational{N2f{3}}, q(2));
    EXPECT_EQ(rational{N2{-0}}, q(0));

    using Strict = inside<{{0, 10}, 2}>; // checked, no rounding mode
    EXPECT_EQ(Strict::try_make(3).error(), errc::rounding_error);
    EXPECT_THROW((void)Strict{3}, inside_error);
}

TEST(ConsistencyTest, integer_source_snaps_on_every_path) {
    using F = inside<{{0, 10}, 2}, round_nearest>;
    EXPECT_EQ(rational{F{3}}, q(4));
    EXPECT_EQ(rational{*F::try_make(3)}, q(4));
    F b          = F{0.0};
    b.policy<>() = 3;
    EXPECT_EQ(rational{b}, q(4));
}

//---------------------------------------------------------------------------
// wrap folds modulo span + notch whatever the source type.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, integer_wrap_uses_span_plus_notch) {
    using H = inside<{{0, 10}, per<2>}>; // span 10, notch 1/2 → modulus 10.5
    EXPECT_EQ(rational{wrap_cast<H>(12)}, rational{wrap_cast<H>(q(12))});
    EXPECT_EQ(rational{wrap_cast<H>(12)}, q(3, 2));
    using N2 = inside<{{0, 10}, 2}, wrap>; // modulus 12
    EXPECT_EQ(rational{N2{22}}, rational{N2{q(22)}});
    EXPECT_EQ(rational{N2{22}}, q(10));
    using U = inside<{0, 10}, wrap>; // unit notch: modulus 11
    EXPECT_EQ(rational{U{12}}, q(1));
    EXPECT_EQ(rational{U{-1}}, q(10));
}

//---------------------------------------------------------------------------
// wrap + rounding never leaves the grid: rounding up to Upper + notch is the
// same point as Lower on the circle.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, wrap_then_round_stays_on_the_grid) {
    using W = inside<{0, 8}, wrap | round_nearest>;
    EXPECT_EQ(rational{W{q(17, 2)}}, q(0));  // 8.5 → 9 ≡ 0
    EXPECT_EQ(rational{W{q(-3, 10)}}, q(0)); // -0.3 → 0
    EXPECT_EQ(rational{W{q(-7, 10)}}, q(8)); // -0.7 → -1 ≡ 8
    EXPECT_EQ(rational{W{q(39, 4)}}, q(1));  // 9.75 → 10 ≡ 1
    using F = inside<{{0, 8}, 1}, round_nearest | wrap>;
    EXPECT_EQ(rational{F{8.5}}, rational{0.0});
    EXPECT_EQ(rational{F{-0.3}}, rational{0.0});
    EXPECT_EQ(rational{F{-0.7}}, rational{8.0});
}

// An inside source with off-integer values wraps after rounding by the target's
// policy, exactly like the same value given as a rational.
TEST(ConsistencyTest, inside_source_wrap_rounds_first) {
    using L = inside<{0, 10}, wrap | round_nearest>;
    using R = inside<{{0, 20}, per<2>}>;
    L a{0};
    a = R{q(25, 2)};
    EXPECT_EQ(rational{a}, rational{L{q(25, 2)}});
    EXPECT_EQ(rational{a}, q(2)); // 12.5 → 13 ≡ 2
}

// unchecked_cast keeps the target's storage layout (representation flags).
TEST(ConsistencyTest, unchecked_cast_respects_storage_flags) {
    using D = inside<{5, 100}>;
    EXPECT_EQ(rational{unchecked_cast<D>(7)}, q(7));
    using W = inside<{5, 100}>;
    EXPECT_EQ(rational{unchecked_cast<W>(7)}, q(7));
    using F = inside<{{0, 4}, per<2>}>;
    EXPECT_EQ(rational{unchecked_cast<F>(1.5)}, rational{1.5});
}

// Representation flags carried into a result are dropped when the result grid
// cannot hold them, instead of tripping inside's static_asserts.
TEST(ConsistencyTest, result_drops_invalid_direct_and_indexed) {
    using D  = inside<{0, 10}>;
    using H  = inside<{{0, 1}, per<2>}>;
    using IX = inside<{1, 10}>;
    EXPECT_EQ(rational{D{3} * H{q(1, 2)}}, q(3, 2));
    auto dd = D{6} / D{3};
    EXPECT_EQ(rational{*dd}, q(2));
    auto ii = IX{6} / IX{3};
    EXPECT_EQ(rational{ii}, q(2)); // zero-free divisor, quotient fits: a plain value
}

// Math auto-output types do not inherit fixed-width storage flags.
TEST(ConsistencyTest, math_output_drops_width_flags) {
    using B8 = inside<{-128, 127}, round_nearest>;
    EXPECT_EQ(rational{math::abs(B8{-128})}, q(128));
}

// A dyadic inside compared with an inside whose values are not exact in
// double compares exactly, not after rounding the other side to double.
TEST(ConsistencyTest, dyadic_vs_exact_comparison_is_exact) {
    using F = inside<{{0, 2}, per<2>}>;
    using C = inside<{{0, 2}, rational{0}}>;         // continuous, rational raw
    const C c{(q(1) + q(1, imax{1} << 53)).value()}; // 1 + 2^-53: rounds to 1.0
    const F one{1};
    EXPECT_FALSE(one == c);
    EXPECT_TRUE(one < c);
    EXPECT_TRUE(c > one);
    EXPECT_TRUE(one == C{q(1)}); // equal values still compare equal
}

// fmod: exact result on the gcd notch, sized by both operands, any divisor sign.
TEST(ConsistencyTest, fmod_output_grid_is_exact_and_large_enough) {
    using X  = inside<{-10, 10}, round_nearest>;
    using Yn = inside<{-10, -1}, round_nearest>; // negative divisor
    EXPECT_EQ(rational{math::fmod(X{7}, Yn{-8})}, q(7));
    using Yh = inside<{{1, 4}, per<2>}, round_nearest>;
    EXPECT_EQ(rational{math::fmod(X{-3}, Yh{q(5, 2)})}, q(-1, 2)); // exact, sign of x
    EXPECT_EQ(rational{math::fmod(X{3}, Yh{q(5, 2)})}, q(1, 2));
    using X2 = inside<{{0, 10}, 2}, round_nearest>; // notch 2 vs divisor notch 1
    using Y3 = inside<{1, 3}, round_nearest>;
    EXPECT_EQ(rational{math::fmod(X2{8}, Y3{3})}, q(2));
}

// fmod with a divisor grid that spans 0 reports a zero divisor like `/`.
TEST(ConsistencyTest, fmod_zero_divisor_is_an_error_value) {
    using X = inside<{-10, 10}, round_nearest>;
    auto r  = math::fmod(X{7}, X{0});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), errc::division_by_zero);
    EXPECT_EQ(rational{*math::fmod(X{7}, X{-3})}, q(1));
}

// Continuous (rational) arithmetic is overflow-checked, and a result is
// checked.
TEST(ConsistencyTest, continuous_arithmetic_is_overflow_checked) {
    using E = inside<{{0, 1}, rational{0}}>;
    static_assert(detail::is_expected_v<decltype(E{q(1, 3)} + E{q(1, 7)})>);
    using D = inside<{0, 10}, checked>;
    static_assert(has_flag(policy_of<decltype(D{1} + D{2})>, checked));
}

//---------------------------------------------------------------------------
// Compound operators report an error result through the type's policy — never
// a silent no-op, never std::bad_expected_access.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, compound_ops_report_errors_through_the_policy) {
    constexpr imax big = (imax{1} << 62) - 1;
    using E            = inside<{{0, 1}, rational{0}}, checked>;
    E e{q(1, big)};
    EXPECT_THROW(e += q(1, big - 2), inside_error);    // rational RHS overflow
    EXPECT_EQ(rational{e}, q(1, big));                 // left unchanged
    EXPECT_THROW(e += E{q(1, big - 2)}, inside_error); // slow path, rational overflow
    EXPECT_THROW(e *= E{q(1, big - 2)}, inside_error);
}

// The raw += fast path honours ignore_range like plain assignment does.
TEST(ConsistencyTest, compound_fast_path_honours_ignore_domain) {
    using X = inside<{0, 10}, checked | ignore_range>;
    X x{10};
    EXPECT_NO_THROW(x += 1_ins);
    X y{10};
    EXPECT_NO_THROW(y = y + 1_ins);
}

// ignore_zero on either operand silences a zero divisor in /= and %=, as in
// div/mod; policy_ref /= routes a zero divisor through on_error / ignore_zero.
TEST(ConsistencyTest, zero_divisor_handling_agrees) {
    using X = inside<{0, 10}, checked | snap>;
    using Z = inside<{0, 10}, checked | snap | ignore_zero>;
    X x{6};
    EXPECT_NO_THROW(x /= Z{0});
    EXPECT_NO_THROW(x %= Z{0});
    EXPECT_THROW(x /= X{0}, inside_error);

    bool called = false;
    X    b{6};
    b.on_error([&](auto&, errc e, auto) { called = (e == errc::division_by_zero); }) /= X{0};
    EXPECT_TRUE(called);
    EXPECT_NO_THROW(b.policy<ignore_zero>() /= X{0});
}

// Assignment rounds, then range-checks: 8.25 into [0, 8] rounds to 8; 8.5
// rounds to 9 and is out of range.
TEST(ConsistencyTest, store_rounds_before_the_range_check) {
    using O = inside<{0, 8}, checked | round_nearest>;
    EXPECT_EQ(rational{O{q(33, 4)}}, q(8));
    EXPECT_THROW((void)O{q(17, 2)}, inside_error);
}

// NaN / ±inf go through the policy like any other bad value: the error-code
// constructor and try_make report not_finite, clamp saturates an infinity.
TEST(ConsistencyTest, non_finite_input_goes_through_the_policy) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    using X          = inside<{0, 10}, round_nearest | checked>;
    EXPECT_EQ(X::try_make(nan).error(), errc::not_finite);
    auto t = X::try_make(inf);
    ASSERT_FALSE(t.has_value());
    EXPECT_EQ(t.error(), errc::not_finite);
    EXPECT_THROW((void)X{nan}, inside_error);

    using C = inside<{0, 10}, round_nearest | clamp>;
    EXPECT_EQ(rational{C{inf}}, q(10));
    EXPECT_EQ(rational{C{-inf}}, q(0));
    using F = inside<{{0, 10}, per<2>}>;
    EXPECT_EQ(F::try_make(nan).error(), errc::not_finite);
    using FC = inside<{{0, 10}, per<2>}, clamp>;
    EXPECT_EQ(rational{FC{inf}}, rational{10.0});
}

// pow: a result past Out reports overflow, and saturates under clamp.
TEST(ConsistencyTest, pow_past_the_output_range) {
    using B           = inside<{1, 4}, round_nearest>;
    using E           = inside<{0, 20}, round_nearest>;
    using small       = inside<{0, 1000}, round_nearest>;
    using small_clamp = inside<{0, 1000}, round_nearest | clamp>;
    EXPECT_EQ(math::pow_into<small>(B{4}, E{20}).error(), errc::overflow); // 2^40
    EXPECT_EQ(rational{*math::pow_into<small_clamp>(B{4}, E{20})}, q(1000));
    // The deduced output covers every corner: 4^20 fits it.
    EXPECT_EQ(rational{*math::pow(B{4}, E{20})}, q(1) * (umax{1} << 40));
}

// conversion_rounds sees the notch of a non-dyadic notched grid.
TEST(ConsistencyTest, trunc_predicate_on_thirds_grid) {
    using E = inside<{{0, 1}, per<3>}>;
    EXPECT_TRUE(conversion_rounds<E>(0.5));
    EXPECT_FALSE(conversion_rounds<E>(q(1, 3)));
}

// numeric_limits reports the rounding mode stores use, and integer-ness of any
// integer grid.
TEST(ConsistencyTest, numeric_limits_round_style_and_is_integer) {
    static_assert(std::numeric_limits<inside<{0, 10}, round_floor>>::round_style == std::round_toward_neg_infinity);
    static_assert(std::numeric_limits<inside<{0, 10}, round_ceil>>::round_style == std::round_toward_infinity);
    static_assert(std::numeric_limits<inside<{0, 10}, round_nearest>>::round_style == std::round_to_nearest);
    static_assert(std::numeric_limits<inside<{0, 10}, snap>>::round_style == std::round_toward_zero);
    static_assert(std::numeric_limits<inside<{{0, 10}, 2}>>::is_integer);
    static_assert(!std::numeric_limits<inside<{{0, 10}, per<2>}>>::is_integer);
    SUCCEED();
}

//---------------------------------------------------------------------------
// Feature symmetry.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, two_input_math_is_symmetric_in_its_types) {
    using A = inside<{{-1, 1}, per<16>}, round_nearest>;
    using B = inside<{{-2, 2}, per<64>}, round_nearest>;
    auto t  = math::atan2(A{q(1, 2)}, B{q(1, 2)}); // mixed input types
    EXPECT_EQ(rational{t}, rational{math::atan2(B{q(1, 2)}, B{q(1, 2)})});
    static_assert(std::same_as<decltype(math::hypot(A{0}, B{0})), decltype(math::hypot(B{0}, A{0}))>);
}

TEST(ConsistencyTest, compound_ops_accept_expected_rhs) {
    using X = inside<{1, 10}, checked | snap>;
    using Y = inside<{0, 10}, checked | snap>; // divisor may be 0
    X x{2};
    x += Y{6} / Y{2}; // expected<inside>
    EXPECT_EQ(rational{x}, q(5));
    EXPECT_THROW(x += Y{6} / Y{0}, inside_error); // the error is reported
}

TEST(ConsistencyTest, casts_take_inside_sources) {
    using Src = inside<{{0, 20}, per<2>}>;
    using Dst = inside<{0, 10}>;
    EXPECT_EQ(rational{checked_cast<Dst>(Src{q(4)})}, q(4));
    EXPECT_THROW((void)checked_cast<Dst>(Src{q(9, 2)}), inside_error); // off notch
    try {
        (void)checked_cast<Dst>(Src{q(12)});
        FAIL();
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::overflow);
    } // out of range
    EXPECT_EQ(rational{unchecked_cast<Dst>(Src{q(4)})}, q(4));
}

TEST(ConsistencyTest, midpoint_across_grids) {
    using A = inside<{0, 10}>;
    using B = inside<{{0, 10}, per<2>}>;
    EXPECT_EQ(rational{midpoint(A{3}, B{q(4)})}, q(7, 2));
}

// Round, then range-check: a value just outside the interval that the policy
// rounds onto an endpoint is stored; one that rounds outside still reports.
TEST(ConsistencyTest, rounding_runs_before_the_range_check) {
    using bin = inside<{0, 9}, round_floor>;
    EXPECT_EQ(rational{bin{9.55}}, q(9));       // double source
    EXPECT_EQ(rational{bin{q(191, 20)}}, q(9)); // rational source
    using ms = inside<{{0, 100}, per<10>}, round_nearest>;
    EXPECT_EQ(rational{bin{ms{95.5} / just<10>}}, q(9));    // inside source
    EXPECT_EQ(bin::try_make(10.0).error(), errc::overflow); // floors to 10: outside

    using db = inside<{{-24, 12}, per<2>}, round_nearest>;
    EXPECT_EQ(rational{db{-24.1}}, q(-24));
    EXPECT_EQ(db::try_make(-24.3).error(), errc::overflow); // rounds to -24.5

    // Without a rounding mode nothing rounds: out of range is an overflow.
    using strict = inside<{0, 9}>;
    EXPECT_EQ(strict::try_make(9.5).error(), errc::overflow);

    // A clamp policy sees no overshoot when rounding lands inside.
    using cl = inside<{0, 9}, clamp | round_floor>;
    cl  c{0};
    int fired                                 = 0;
    c.on_clamp([&](auto&, auto) { ++fired; }) = 9.5;
    EXPECT_EQ(rational{c}, q(9));
    EXPECT_EQ(fired, 0);
}

TEST(ConsistencyTest, double_source_rounds_before_the_range_check) {
    using F = inside<{{0, 1}, per<4>}, round_nearest>;
    EXPECT_EQ(rational{F{1.1}}, rational{1.0});
    EXPECT_EQ(F::try_make(1.2).error(), errc::overflow); // rounds to 1.25
    using Ff = inside<{{0, 1}, per<4>}, round_floor>;
    EXPECT_EQ(rational{Ff{1.2}}, rational{1.0}); // floors in range
}

TEST(ConsistencyTest, predicates_round_before_the_range_check) {
    using bin = inside<{0, 9}, round_floor>;
    EXPECT_FALSE(conversion_overflows<bin>(9.5));
    EXPECT_TRUE(conversion_rounds<bin>(9.5));
    EXPECT_TRUE(conversion_overflows<bin>(10.0));
    EXPECT_FALSE(conversion_rounds<bin>(10.0));
    using strict = inside<{0, 9}>;
    EXPECT_TRUE(conversion_overflows<strict>(9.5));
    EXPECT_FALSE(conversion_rounds<strict>(9.5));
}

// A point inside's value is its type: its raw is empty (1 byte standalone, 0 as
// a [[no_unique_address]] member), and an integer point is integer-stored, so
// `%` and snap `/` with a literal take the native integer paths.
namespace {
struct with_point {
    [[no_unique_address]] decltype(just<440>) Pitch;
    std::uint32_t                             Frames;
};
} // namespace

TEST(ConsistencyTest, point_insides_store_nothing) {
    static_assert(sizeof(5_ins) == 1);
    static_assert(sizeof(with_point) == sizeof(std::uint32_t));
    static_assert(std::is_same_v<decltype(5_ins)::raw_type, point_slot>);
    static_assert(std::is_same_v<decltype(just<frac<1, 3>>)::raw_type, point_slot>);
    static_assert(inside_assignable<inside<{{0, 9}, 3}>, decltype(3_ins)>);
    static_assert(!inside_assignable<inside<{{0, 9}, 3}>, decltype(1_ins)>);

    using val = inside<{0, 100}, snap>;
    auto r    = val{17} % 5_ins; // divisor excludes 0: plain
    static_assert(!is_expected_v<decltype(r)>);
    static_assert(grid_of<decltype(r)> == grid{0, 4});
    EXPECT_EQ(rational{r}, q(2));
    auto d = val{17} / 5_ins;
    static_assert(!is_expected_v<decltype(d)>);
    EXPECT_EQ(rational{d}, q(3));
    EXPECT_EQ(rational{-5_ins}, q(-5));
    EXPECT_EQ((rational{just<frac<1, 3>>}), q(1, 3));
    EXPECT_EQ((rational{val{17} * just<frac<1, 2>>}), q(17, 2));
}

//---------------------------------------------------------------------------
// The double fast paths of integer storage (store from a double, decode to a
// double) agree with the exact rational path on every input.
//---------------------------------------------------------------------------
namespace {
// Grid points, ties, one ulp either side of each, and pseudo-random values.
template <typename T>
std::vector<double> probe_doubles() {
    constexpr double lo  = static_cast<double>(lower64<T>);
    constexpr double hi  = static_cast<double>(upper64<T>);
    constexpr double nd  = static_cast<double>(notch64<T>);
    auto             ulp = [](double v, int s) {
        return std::bit_cast<double>(std::bit_cast<std::int64_t>(v) + (v < 0 ? -s : s));
    };
    std::vector<double> out;
    for (double v = lo; v <= hi; v += nd * 37)
        for (double w : {v, v + nd / 2, v - nd / 2})
            for (double x : {w, ulp(w, 1), ulp(w, -1)})
                if (x >= lo && x <= hi)
                    out.push_back(x);
    std::uint64_t s = 0x9E3779B97F4A7C15;
    for (int i = 0; i < 2000; ++i) {
        s = s * 6364136223846793005u + 1442695040888963407u;
        out.push_back(lo + (hi - lo) * static_cast<double>(s >> 11) * 0x1p-53);
    }
    return out;
}

template <typename T>
void expect_double_store_exact() {
    for (double d : probe_doubles<T>())
        EXPECT_EQ(T{d}.raw(), T{rational{d}}.raw()) << "d = " << d;
}

template <typename T>
void expect_double_decode_exact() {
    for (double d : probe_doubles<T>()) {
        const T t{rational{d}};
        EXPECT_EQ(static_cast<double>(t), static_cast<double>(rational{t})) << "d = " << d;
    }
}
} // namespace

TEST(ConsistencyTest, double_store_into_integer_storage_matches_rational) {
    expect_double_store_exact<inside<{{-8, 8}, per<16384>}, round_nearest>>();
    expect_double_store_exact<inside<{{-8, 8}, per<16384>}, round_floor>>();
    expect_double_store_exact<inside<{{-8, 8}, per<16384>}, round_ceil>>();
    expect_double_store_exact<inside<{{-8, 8}, per<16384>}, round_half_even>>();
    expect_double_store_exact<inside<{{-8, 8}, per<16384>}, snap>>();
    expect_double_store_exact<inside<{{0, 4}, per<65536>}, round_nearest>>();
    expect_double_store_exact<inside<{{-4, 4}, per<2>}, round_half_even>>();
    expect_double_store_exact<inside<{{-100, 100}, per<1000>}, round_nearest>>();             // not dyadic
    expect_double_store_exact<inside<{{-6, 6}, rational{3, 4}}, round_nearest>>();            // dyadic, notch not 2^k
    expect_double_store_exact<inside<{{rational{1, 4}, rational{17, 4}}, 1}, round_floor>>(); // unanchored
}

TEST(ConsistencyTest, double_store_into_integer_storage_reports_off_grid) {
    using strict = inside<{{-8, 8}, per<16384>}>;
    EXPECT_EQ(strict::try_make(0.1).error(), errc::rounding_error);
    EXPECT_EQ(rational{strict::try_make(0.25).value()}, q(1, 4));
    EXPECT_EQ(rational{strict::try_make(-8.0).value()}, q(-8));
}

TEST(ConsistencyTest, integer_storage_decodes_to_the_nearest_double) {
    expect_double_decode_exact<inside<{{-8, 8}, per<16384>}, round_nearest>>();
    expect_double_decode_exact<inside<{{-100, 100}, per<1000>}, round_nearest>>();
    expect_double_decode_exact<inside<{{-10, 10}, per<3>}, round_nearest>>();
    expect_double_decode_exact<inside<{{0, 7}, rational{7, 1024}}, round_nearest>>();
}

// A fine-denominator source whose signed value index overflows imax rounds in
// value space too: `snap` alone truncates toward zero, also below zero.
TEST(ConsistencyTest, snap_truncates_fine_negative_rationals_toward_zero) {
    using T              = inside<{{-8, 8}, per<16384>}, snap>;
    constexpr imax k2_61 = imax{1} << 61;
    EXPECT_EQ((rational{T{rational{-55 * (imax{1} << 47) + 1, k2_61}}}), q(-54, 16384));
    EXPECT_EQ((rational{T{rational{-55 * (imax{1} << 47) - 1, k2_61}}}), q(-55, 16384));
    EXPECT_EQ((rational{T{rational{55 * (imax{1} << 47) + 1, k2_61}}}), q(55, 16384));
}

//---------------------------------------------------------------------------
// Continuous grids after the storage change (2026-10-10 sweep).
//---------------------------------------------------------------------------
// A continuous quotient is exact: the integer division paths need integer
// values, which a continuous grid does not have whatever its limits.
TEST(ConsistencyTest, continuous_division_is_exact) {
    using C = inside<{{1, 10}, 0}, round_nearest>;
    EXPECT_EQ(rational{*(C{q(7, 2)} / C{q(3, 2)})}, q(7, 3));
}

// Wrapping a continuous source folds its value, not its numerator.
TEST(ConsistencyTest, continuous_source_wraps_by_value) {
    using W = inside<{0, 3}, wrap | round_nearest>;
    W w{0};
    w = inside<{{0, 10}, 0}>{q(11, 2)}; // 5.5 ≡ 1.5 (mod 4) → 2
    EXPECT_EQ(rational{w}, q(2));
}

// hull with a point keeps the lattice: max/min with a constant stay integer.
TEST(ConsistencyTest, hull_with_a_point_keeps_the_lattice) {
    const auto m = max(inside<{0, 100}>{3}, just<0>);
    static_assert(integer_storage<std::remove_cvref_t<decltype(m)>>);
    EXPECT_EQ(rational{m}, q(3));
    const auto two = max(just<5>, just<rational{1, 2}>);
    static_assert(notch_of<std::remove_cvref_t<decltype(two)>> == rational{9, 2});
    EXPECT_EQ(rational{two}, q(5));
}

// Clamping an inside into a continuous target stores the exact endpoint.
TEST(ConsistencyTest, clamp_into_continuous_stores_the_endpoint) {
    inside<{{rational{1, 2}, rational{21, 2}}, 0}, clamp> c{q(1, 2)};
    c = inside<{0, 20}>{15};
    EXPECT_EQ(rational{c}, q(21, 2));
    c = inside<{-5, 0}>{-3};
    EXPECT_EQ(rational{c}, q(1, 2));
}

// Notched → continuous needs no snap; continuous → notched is checked on store.
TEST(ConsistencyTest, continuous_sides_are_assignable) {
    inside<{{0, 1}, 0}> c{0};
    c = inside<{{0, 1}, per<10>}>{q(3, 10)};
    EXPECT_EQ(rational{c}, q(3, 10));
    inside<{{0, 10}, 2}> e{0};
    e = inside<{{0, 10}, 0}>{4};
    EXPECT_EQ(rational{e}, q(4));
    EXPECT_THROW((e = inside<{{0, 10}, 0}>{3}), inside_error);
}

TEST(ConsistencyTest, hash_covers_every_storage) {
    EXPECT_EQ(std::hash<std::remove_cvref_t<decltype(just<5>)>>{}(just<5>),
              std::hash<std::remove_cvref_t<decltype(just<5>)>>{}(just<5>));
    using C = inside<{{0, 1}, 0}>;
    EXPECT_EQ(std::hash<C>{}(C{q(1, 3)}), std::hash<C>{}(C{q(1, 3)}));
}
