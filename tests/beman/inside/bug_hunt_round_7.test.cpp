// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the seventh review round. The static_asserts did not compile
// before the fixes; the runtime cases stored wrong values, reported the
// wrong error, or ran into undefined behaviour.

#include <beman/inside/inside.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>

using namespace beman::inside;

namespace {
using I64             = std::int64_t;
constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
constexpr I64 I64_MIN = std::numeric_limits<I64>::min();
} // namespace

// A double into uint64 (and any grid whose upper limit rounds to 2^64 as a
// double) did not compile: the range test built a rational of 2^64.
TEST(BugHuntRound7, double_into_a_grid_reaching_2_pow_64) {
    EXPECT_EQ(uint64{3.0}, 3);
    EXPECT_EQ(uint64::try_make(1e19)->to<std::uint64_t>(), 10000000000000000000u);
    EXPECT_EQ(uint64::try_make(0x1p64).error(), errc::overflow);
    EXPECT_EQ(uint64::try_make(-1.0).error(), errc::overflow);
    const inside<{0, UINT64_MAX}, clamp> c{1e30};
    EXPECT_EQ(c.to<std::uint64_t>(), UINT64_MAX);
}

// A double finer than 2^-62 does not fit the rational of an unbounded
// continuous grid (the store reports overflow); the predicates said "fits".
TEST(BugHuntRound7, continuous_predicates_see_the_rational_store) {
    using C        = inside<{{-1, 1}, 0}>;
    const double d = 0x1.644cb4ec9ec46p-20;
    EXPECT_EQ(C::try_make(d).error(), errc::overflow);
    EXPECT_TRUE(conversion_overflows<C>(d));
    EXPECT_TRUE(conversion_loses<C>(d));
    EXPECT_FALSE(conversion_overflows<C>(0.25));
}

// includes(interval, double) went through rational{d}: bits past 2^-62 were
// dropped and |d| ≥ 2^64 terminated.
TEST(BugHuntRound7, includes_a_double_exactly) {
    EXPECT_FALSE(includes(interval{0, 1}, -1e-300));
    EXPECT_FALSE(includes(interval{-1, 0}, 0x1p-80));
    EXPECT_TRUE(includes(interval{0, 1}, 0x1p-80));
    EXPECT_FALSE(includes(interval{0, 1}, 1e300));
    EXPECT_FALSE(includes(interval{0, 1}, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE((grid{interval{0, 1}, 1}.representable(-1e-300)));
    EXPECT_FALSE((grid{interval{0, 1}, rational{1, 4}}.representable(0x1p-70)));
    EXPECT_TRUE((grid{interval{0, 1}, rational{1, 4}}.representable(0.75)));
}

// Literals with a leading zero but in floating or radix form stay decimal
// (010_ins itself is refused: fail/octal_literal).
TEST(BugHuntRound7, literal_forms_with_a_leading_zero) {
    static_assert(0_ins == 0);
    static_assert(01.5_r == rational{3, 2});
    static_assert(0x10_r == 16);
    static_assert(0.5_r == rational{1, 2});
}

// Text past the wide parse ignored clamp (1e1300 overflowed) and N/D text
// had no tiny stand-in.
TEST(BugHuntRound7, from_chars_stand_ins_for_huge_and_fraction_text) {
    using price = inside<{{0, 1'000'000}, per<100>}, round_nearest | clamp>;
    EXPECT_EQ(*parse<price>("1e1300"), 1'000'000);
    EXPECT_EQ(*parse<price>("-1e1300"), 0);
    EXPECT_EQ(*parse<price>("1/3e5000"), 0);
    EXPECT_EQ(*parse<price>("1e-5000"), 0);
    using checked_price = inside<{{0, 1'000'000}, per<100>}>;
    EXPECT_EQ(parse<checked_price>("1e1300").error(), errc::overflow);
    EXPECT_EQ(parse_exact<price>("1/3e5000").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<price>("1e1300").error(), errc::overflow);
}

// parse_exact rounded off-grid wide text onto a bounded continuous
// grid under a rounding policy instead of reporting rounding_error.
TEST(BugHuntRound7, from_chars_exact_bounded_continuous) {
    using B = inside<grid{{0, 1}, max_denominator{5}}, round_nearest>;
    EXPECT_EQ(parse_exact<B>("0.40000000000000000000000000001").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<B>("1e-5000").error(), errc::rounding_error);
    EXPECT_EQ(parse_exact<B>("0.41").error(), errc::rounding_error);
    EXPECT_EQ(*parse_exact<B>("0.4"), rational(2, 5));
}

// The predicates did not compile for a wide-valued inside argument.
TEST(BugHuntRound7, predicates_take_a_wide_inside) {
    using W = inside<{{-rational{1ll << 62}, rational{1ll << 62}}, rational{1, 1ll << 62}}>;
    using T = inside<{{0, 2}, rational{1, 1ll << 62}}>;
    const W w{rational{1, 4}};
    EXPECT_FALSE(conversion_overflows<T>(w));
    EXPECT_FALSE(conversion_rounds<T>(w));
    EXPECT_TRUE(conversion_overflows<T>(W{rational{-1, 4}}));
}

// A long double was narrowed to double: a store lost its low bits silently
// and the predicates disagreed with the range check.
TEST(BugHuntRound7, long_double_is_taken_exactly) {
    if constexpr (sizeof(long double) > sizeof(double)) {
        using B             = inside<{0, 10}>;
        const long double x = 1.0L + 0x1p-60L;
        EXPECT_EQ(B::try_make(x).error(), errc::rounding_error);
        EXPECT_TRUE(conversion_rounds<B>(x));
        EXPECT_TRUE(conversion_overflows<B>(10.0L + 0x1p-60L));
        EXPECT_EQ(B::try_make(10.0L + 0x1p-60L).error(), errc::overflow);
        EXPECT_EQ((inside<{0, 100}, round_ceil>{x}), 2);
        EXPECT_EQ((inside<{0, 100}, round_floor>{1.0L - 0x1p-60L}), 0);
        EXPECT_EQ((inside<{0, 100}, wrap>{0x1p60L + 3.0L}), static_cast<int>(((1ull << 60) + 3) % 101));
        EXPECT_EQ((inside<{0, 100}, clamp>{1e4000L}), 100);
        EXPECT_EQ((inside<{0, 100}, wrap>::try_make(1e4000L).error()), errc::overflow);
    }
    EXPECT_EQ((inside<{0, 100}>{2.0L}), 2);
}

// numerator() returned imax whenever the values fit it, and wrapped a
// reduced numerator past it.
TEST(BugHuntRound7, numerator_past_int64) {
    using X      = inside<grid{{I64{1} << 62, (I64{1} << 62) + 1}, rational{1, 2}}>;
    const X    x = rational{(std::uint64_t{1} << 63) + 1, 2};
    const auto n = x.numerator();
    using N      = std::remove_cvref_t<decltype(n)>;
    static_assert(!std::same_as<N, I64>);
    EXPECT_TRUE(n == N{std::uint64_t{1} << 63} + N{1});
    using Z      = inside<grid{{-I64_MAX, -I64_MAX + 1}, rational{1, 2}}>;
    const Z    z = rational{UINT64_MAX - 2, -2};
    const auto m = z.numerator();
    EXPECT_TRUE(m == -std::remove_cvref_t<decltype(m)>{UINT64_MAX - 2});
    // A grid whose numerators fit keeps imax.
    static_assert(std::same_as<decltype(inside<{{0, 10}, rational{1, 4}}>{}.numerator()), I64>);
    static_assert(std::same_as<decltype(inside<{{0, 1}, 0}>{}.numerator()), I64>);
}

// raw·notch passed imax in to_value although the value fits.
TEST(BugHuntRound7, to_value_on_a_coarse_int64_grid) {
    using A = inside<grid{{I64_MIN, I64_MAX - 1}, 2}>;
    static_assert([] {
        constexpr A x = I64_MAX - 1;
        return static_cast<I64>(x) == I64_MAX - 1 && x.to<I64>() == I64_MAX - 1;
    }());
    A y = I64_MIN;
    y   = I64_MAX - 1;
    EXPECT_EQ(static_cast<I64>(y), I64_MAX - 1);
}

// The raw += / -= fast paths added R's raw and the bias before the delta
// was formed, overflowing their work type.
TEST(BugHuntRound7, compound_add_raw_delta) {
    static_assert([] {
        using L = inside<grid{{0, I64_MAX - 1}, 2}, wrap>;
        using R = inside<grid{{I64_MIN, I64_MAX - 1}, 2}>;
        L x     = I64_MAX - 1;
        x += R{I64_MAX - 1};
        return x == I64_MAX - 3;
    }());
    constexpr std::uint64_t TOP = (std::uint64_t{1} << 63) + (std::uint64_t{1} << 62);
    using L2                    = inside<grid{{0, (I64{1} << 62) - 2}, 2}, wrap>;
    using R2                    = inside<grid{{rational{I64_MIN}, rational{TOP}}, 2}>;
    L2 z                        = 0;
    z -= R2{rational{TOP}};
    EXPECT_EQ(z, 0);
}

// A cross-grid comparison of an unanchored grid overflowed imax forming
// raw·scale.
TEST(BugHuntRound7, unanchored_cross_grid_comparison) {
    constexpr I64 P61 = I64{1} << 61;
    using X           = inside<grid{{rational{-2 * P61 - 1, 2}, rational{2 * P61 - 1, 2}}, 1}>;
    using Y           = inside<grid{{0, 1}, rational{1, 2}}>;
    constexpr X cx    = rational{2 * P61 - 1, 2};
    constexpr Y cy    = 0;
    static_assert(cx > cy);
    static_assert(X{rational{-2 * P61 - 1, 2}} < cy);
}

// The one-shot casts OR-ed their flags with the type's, so the type's
// rounding mode or clamp won.
TEST(BugHuntRound7, casts_override_the_declared_policy) {
    EXPECT_EQ((clamp_cast<inside<{0, 100}, round_floor>, round_nearest>(2.5)), 3);
    EXPECT_EQ((clamp_cast<inside<{0, 100}, round_half_even>, round_nearest>(2.5)), 3);
    EXPECT_EQ((clamp_cast<inside<{0, 100}, round_floor>, round_ceil>(2.5)), 3);
    EXPECT_EQ((clamp_cast<inside<{0, 100}, round_ceil>, round_nearest>(2.2)), 2);
    EXPECT_EQ((wrap_cast<inside<{0, 100}, clamp>>(150)), 49);
    EXPECT_EQ((clamp_cast<inside<{0, 100}, wrap>>(150)), 100);
    EXPECT_EQ((clamp_cast<inside<{0, 100}, round_floor>>(7.9)), 7); // the type's rounding stays
}

// wrap on a bounded continuous grid folded before it rounded.
TEST(BugHuntRound7, wrap_rounds_first_on_a_bounded_grid) {
    using S = inside<{{0, 1}, max_denominator{2}}, wrap | snap>;
    using N = inside<{{0, 1}, max_denominator{2}}, wrap | round_nearest>;
    using C = inside<{{0, 1}, max_denominator{7}}, wrap | round_ceil>;
    EXPECT_EQ(S{-1.3}, 0);
    EXPECT_EQ(N{-1.25}, rational(1, 2));
    EXPECT_EQ(C{rational(-97, 8)}, 0);
    EXPECT_EQ(N{2.75}, 0);
}

// on_clamp did not fire for an infinity, though it was clamped.
TEST(BugHuntRound7, on_clamp_hears_of_infinities) {
    inside<{0, 100}> x{50};
    int              fired = 0;
    double           over  = 0;
    x.on_clamp([&](auto&, auto o) {
        ++fired;
        over = static_cast<double>(o);
    })                     = std::numeric_limits<double>::infinity();
    EXPECT_EQ(x, 100);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(over, std::numeric_limits<double>::infinity());
    x.on_clamp([&](auto&, auto) { ++fired; }) = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(x, 0);
    EXPECT_EQ(fired, 2);
}

// An integer division past int64 ignored snap and the rounding mode (the
// exact path returned a continuous quotient).
TEST(BugHuntRound7, integer_division_past_int64_rounds) {
    using u64 = inside<{0, UINT64_MAX}, snap>;
    EXPECT_EQ(u64{7u} / u64{2u}, 3);
    EXPECT_EQ(div(uint64{7u}, uint64{2u}, rounded_floor), 3);
    EXPECT_EQ(div(uint64{UINT64_MAX}, inside<{-3, -1}>{-2}, rounded_ceil), rational(-I64_MAX));
    using i64     = inside<{I64_MIN, I64_MAX}, snap>;
    const auto q1 = i64{-7} / i64{-1};
    using small   = inside<{1, 10}, snap>;
    EXPECT_EQ(q1.value() / small{2}, 3);
    const auto d = uint64{0} - uint64{UINT64_MAX};
    EXPECT_EQ(div(d, inside<{1, 1000}>{7}, rounded_floor), rational(-2635249153387078803));
    using around_zero = inside<{-2, 3}, snap>;
    EXPECT_EQ((u64{5u} / around_zero{0}).error(), errc::division_by_zero);
    // (a / b)·b + a % b == a, as for the native widths.
    const uint64                  a{18446744073709551557u};
    const inside<{1, 1000}, snap> b{1000};
    const auto                    quot = div(a, b, rounded_floor);
    EXPECT_EQ(rational(quot) * 1000 + rational(a % b), rational(a));
}

// x % <point 0> passed its constraint and failed inside the grid.
template <class L, class R>
concept can_mod = requires(L l, R r) { l % r; };
TEST(BugHuntRound7, modulo_by_a_zero_grid_is_constrained) {
    using digit = inside<{0, 9}>;
    static_assert(!can_mod<digit, inside<{0}>>);
    static_assert(can_mod<digit, inside<{1, 9}>>);
}

// asinh formed x² + 1 in a word sized for |x|: past about 2^(S/2) the square
// overflowed and the result came out near log|x|, ln 2 short.
TEST(BugHuntRound7, asinh_of_a_large_argument) {
    using O = inside<{{0, 64}, 0x1p-56}, round_floor>;
    // floor(asinh(1e9)·2^56) (mpmath, 300 bits).
    const auto r = math::asinh_into<O>(inside<{0, 1000000000}>{1000000000});
    EXPECT_EQ(rational(r), (rational{1543215194964068268ull, 1ull << 56}));
    static_assert([] {
        using P      = inside<{{0, 64}, 0x1p-20}, round_nearest>;
        const auto c = math::asinh_into<P>(inside<{0, I64{1} << 40}>{I64{1} << 40});
        return c > 28.41 && c < 28.43; // asinh(2^40) = 28.4190…; log(2^40) is 27.7
    }());
}

// A deduced exp (or pow) whose range reaches a tiny result (e^-57 ≈ 2^-82)
// divided by zero while forming its bound.
TEST(BugHuntRound7, deduced_exp_with_a_tiny_endpoint) {
    using X = inside<{-57, 10}, round_nearest>;
    EXPECT_EQ(math::exp(X{-57}), 0);
    EXPECT_EQ(math::exp(X{0}), 1);
}

// abs(INT64_MIN) negated it in imax.
TEST(BugHuntRound7, abs_of_int64_min) {
    using X = inside<{I64_MIN, 0}>;
    static_assert(math::abs(X{I64_MIN}) == rational{std::uint64_t{1} << 63});
    EXPECT_EQ(math::abs(X{-5}), 5);
}
