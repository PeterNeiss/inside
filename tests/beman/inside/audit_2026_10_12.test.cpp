// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the 2026-10-12 audit. The constexpr cases fail to compile on
// the unfixed build: constant evaluation rejects the signed overflow (UB).

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>

#include <gtest/gtest.h>

#include <cstdint>

using namespace beman::inside;

namespace {
constexpr std::int64_t i64_min = INT64_MIN;
constexpr std::int64_t i64_max = INT64_MAX;
constexpr std::int64_t base    = std::int64_t{1} << 48;
} // namespace

// Integer raw mapping into a full-width int64 target: the old map_raw added
// trunc(Offset) (up to 2^63) before raw_lo<L>, overflowing on the way.
TEST(Audit20261012, raw_map_into_full_int64) {
    using full = inside<{i64_min, i64_max}>;
    static_assert([] {
        sqword a = inside<{0, 100}>{5};
        full   b = inside<{0, 100}>{7};
        full   c = inside<{i64_min, 0}>{i64_min};
        return a == 5 && b == 7 && c == i64_min;
    }());
    static_assert([] {
        inside<{-10, 10}, clamp> lo = full{i64_min};
        inside<{-10, 10}, clamp> hi = full{i64_max};
        return lo == -10 && hi == 10;
    }());
}

// Scalar stores into a full-width int64 grid: the Q-format guard took the
// ceiling of a 2^63 bound in imax, and the range check negated −2^63.
TEST(Audit20261012, scalar_store_into_full_int64) {
    using full = inside<{i64_min, i64_max}>;
    static_assert([] {
        sqword a{2.0};
        sqword b{rational{-7}};
        full   c{rational{-3}};
        c += rational{1};
        full d{-9.0e18};
        return a == 2 && b == -7 && c == -2 && d == -9'000'000'000'000'000'000;
    }());
}

// A Q-format grid far from zero: Lower·2^frac passes imax, so the integer
// codec does not apply (it overflowed in decode; the type did not compile).
TEST(Audit20261012, far_qformat_grid) {
    using Q = inside<{{base, base + 1}, per<65536>}>;
    static_assert([] {
        Q x{base};
        Q y{rational{base * 2 + 1, 2}};
        return rational(x) == rational{base} && rational(y) == rational{base * 2 + 1, 2} && y.to<imax>() == base;
    }());
}

// `%` under rounding: INT64_MIN % -1 divided natively (UB), and a quotient
// rounded away from zero made q·b pass imax. The remainder is now adjusted
// from C++'s `%` instead of formed as a − q·b.
TEST(Audit20261012, modulo_at_int64_limits) {
    static_assert([] {
        inside<{i64_min, i64_max}> a{i64_min};
        inside<{-3, -1}>           b{-1};
        return a % b == 0;
    }());
    static_assert([] {
        inside<{1, 2}> two{2};
        return inside<{0, i64_max}, round_ceil>{i64_max} % two == -1 &&
               inside<{0, i64_max}, round_nearest>{i64_max} % two == -1;
    }());
    static_assert([] {
        inside<{1, 3}> two{2};
        return inside<{-7, 7}, round_floor>{-7} % two == 1 && inside<{-7, 7}, round_ceil>{7} % two == -1;
    }());
}

// Quotient grid of a divisor spanning 0: the smallest divisor magnitude was
// taken as the notch, but an unanchored lattice comes closer to 0
// ({{-1.5, 1.5}, 1} holds ±0.5), so 10 / 0.5 = 20 escaped the [-10, 10] grid.
TEST(Audit20261012, quotient_grid_of_unanchored_divisor) {
    static_assert([] {
        auto q  = *(inside<{0, 10}>{10} / inside<{{-1.5, 1.5}, 1}>{0.5});
        using Q = decltype(q);
        return q == 20 && upper_of<Q> >= rational{20} && lower_of<Q> <= rational{-20};
    }());
    static_assert(!(grid{0, 10} / grid{{0, 1}, 0}).has_value()); // unbounded continuous divisor
}

// rational{unsigned, unsigned} with a den past imax: the cast to the signed
// denominator flipped the sign silently.
TEST(Audit20261012, two_unsigned_rational_rejects_large_den) {
    EXPECT_THROW((void)(rational{1u, 0x8000'0000'0000'0001ull}), inside_error);
    static_assert(rational{1u, 0x7FFF'FFFF'FFFF'FFFFull} > rational{0});
}

// A double finer than 2^-62 lost its low bits (truncated toward zero) on the
// way to rational: ceil stored 0, a strict grid stored 0 without
// rounding_error, and comparisons saw the truncated value.
TEST(Audit20261012, tiny_double_is_exact) {
    using cent_ceil = inside<{{0, 1}, per<100>}, round_ceil>;
    static_assert(cent_ceil{1e-20} == rational{1, 100});
    static_assert(inside<{{0, 1}, per<100000>}, round_ceil>{1e-5} == rational{2, 100000}); // 1e-5 is above 10^-5
    static_assert(inside<{{0, 1}, per<100>}, round_floor>{1e-20} == 0);
    using cent = inside<{{0, 1}, per<100>}>;
    EXPECT_FALSE(cent::try_make(1e-20).has_value());
    EXPECT_FALSE((inside<{{0, 1}, per<100>}, round_floor>::try_make(-1e-20).has_value())); // floors to -0.01
    EXPECT_EQ((inside<{{0, 1}, per<100>}, round_ceil>{-1e-20}), 0);
    EXPECT_FALSE((inside<grid{interval{0, 1}, 0}>::try_make(1e-20).has_value())); // no 64-bit rational holds it
    constexpr inside<grid{interval{0, 1}, 0}> zero{0};
    static_assert(zero < 1e-20 && zero != 1e-20);
}

TEST(Audit20261012, tiny_double_predicates) {
    using unit = inside<{{0, 1}, per<100>}>;
    static_assert(conversion_rounds<unit>(1e-20) && !conversion_overflows<unit>(1e-20));
    static_assert(conversion_overflows<unit>(-1e-20) && !conversion_rounds<unit>(-1e-20));
    static_assert(conversion_rounds<inside<{{0, 1}, per<100>}, round_nearest>>(-1e-20));
}

// An unanchored sum counted in half units passes 63 bits: the wrapping work
// type then divided a wrapped value (−1 came out as INT64_MAX).
TEST(Audit20261012, unanchored_sum_past_63_bits) {
    static_assert([] {
        constexpr std::int64_t p = std::int64_t{1} << 61;
        constexpr rational     hi{2 * p - 1, 2}, lo{-2 * p - 1, 2};
        using L = inside<{{lo, hi}, 1}>;
        return L{-0.5} + L{-0.5} == -1 && L{hi} + L{hi} == 2 * p - 1 && L{lo} + L{lo} == -2 * p - 1 &&
               L{hi} - L{lo} == 2 * p;
    }());
}

// Fixed-point division on a 32-bit boundary: ja·K needs 32 value bits plus
// the sign, but the work type was int32.
TEST(Audit20261012, fixed_division_work_type) {
    using L = inside<{{0, rational{2147483647, 2}}, per<2>}, round_nearest>;
    using R = inside<{{rational{1, 2}, 1}, per<2>}>;
    static_assert(L{rational{2147483647, 2}} / R{rational{1, 2}} == 2147483647);
}

// Checked rational / and + reported overflow for results that fit.
TEST(Audit20261012, rational_ops_reduce_before_overflow) {
    constexpr umax big = (umax{1} << 63) + 2;
    static_assert(*(rational{big} / rational{big}) == rational{1});
    static_assert(*(rational{big} / rational{big, 3u}) == rational{3});
    static_assert(*detail::try_add(rational{(umax{1} << 63) + 1, 2u}, rational{1, 6}) ==
                  rational{3 * (umax{1} << 62) + 2, 3u});
    static_assert(*detail::try_add(rational{~umax{0}, 2u}, rational{1u, 2u}) == rational{umax{1} << 63});
    static_assert(!detail::try_add(rational{~umax{0}}, rational{1}).has_value());
}

// INT64_MIN / -1 reaches 2^63: the quotient grid's endpoint wrapped in imax.
TEST(Audit20261012, quotient_grid_reaches_2_pow_63) {
    static_assert([] {
        inside<{i64_min, 0}, snap> a{i64_min};
        inside<{-2, -1}>           b{-1};
        auto                       q = a / b;
        using Q                      = decltype(q);
        return rational(q) == rational{umax{1} << 63} && upper_of<Q> == rational{umax{1} << 63};
    }());
}
