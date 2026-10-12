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
