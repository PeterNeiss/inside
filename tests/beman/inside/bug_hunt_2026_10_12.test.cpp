// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the 2026-10-12 differential fuzzing round (grids near the
// int64 limits, unanchored lattices, fine sources). The static_asserts did
// not compile before the fixes; the runtime cases stored wrong values, hit
// UB or threw.

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <utility>

#include <cstdint>
#include <limits>
#include <string>

using namespace beman::inside;

namespace {
constexpr std::int64_t i64_min = INT64_MIN;
constexpr std::int64_t i64_max = INT64_MAX;

using full    = inside<{i64_min, i64_max}>;
using quarter = inside<{{-2, 3}, per<4>}>;
using halves  = inside<{{0.5, 10.5}, 1}>;  // Lower off the notch's multiples
using neg     = inside<{{-3.5, -0.5}, 1}>; // ditto, below 0
using thirds  = inside<{{-(3ll << 60), 3ll << 60}, 3}>;
} // namespace

// A full-int64 source into a fine or offset grid: the raw map's Offset
// (Lower_R − Lower_L)/Notch_L passed the 64-bit rational at compile time.
TEST(BugHunt20261012, full_int64_into_fine_grid) {
    static_assert([] {
        auto a = quarter::try_make(full{2});
        auto b = inside<grid_of<halves>, round_nearest>::try_make(full{3}); // a tie: away from zero
        auto c = quarter::try_make(full{i64_min});
        return *a == 2_ins && *b == 3.5_ins && c.error() == errc::overflow;
    }());
    auto w = inside<grid_of<halves>, wrap | round_floor>::try_make(full{i64_max});
    EXPECT_EQ(to_string(*w), "6.5"); // (2^63 − 1.5) mod 11 from 0.5
}

// Integer division by a divisor reaching INT64_MIN: the quotient grid was
// formed from 7/2^63, past the 64-bit rational (C++23 did not compile).
TEST(BugHunt20261012, integer_division_by_int64_min_divisor) {
    using small = inside<{-7, 5}>;
    static_assert([] {
        auto q  = div(small{-7}, full{2}, rounded_toward_zero);
        auto qr = divmod(small{-7}, full{i64_min});
        return *q == -3_ins && qr->Quotient == 0_ins && qr->Remainder == -7_ins &&
               grid_of<decltype(*q)> == grid{-7, 7};
    }());
    // The range is the rounded corners, as the exact quotient grid gave it.
    static_assert(grid_of<decltype(div(small{}, std::declval<inside<{-3, 4}>>(), rounded_ceil))::value_type> ==
                  grid{-7, 7});
}

// A native integer quotient whose grid reaches 2^63 (INT64_MIN / −1): every
// non-negative quotient wrapped through from_value's imax (10 / 2 gave
// 3.4e38), and INT64_MIN / −1 came back as INT64_MIN.
TEST(BugHunt20261012, integer_quotient_past_int64) {
    static_assert(*div(full{10}, full{2}, rounded_toward_zero) == 5_ins);
    EXPECT_EQ(to_string(*div(full{i64_max}, full{1}, rounded_toward_zero)), "9223372036854775807");
    EXPECT_EQ(to_string(*div(full{i64_min}, full{-1}, rounded_floor)), "9223372036854775808");
    EXPECT_EQ(to_string(*div(full{i64_min}, inside<{-1, 1}>{-1}, rounded_toward_zero)), "9223372036854775808");
    const auto qr = divmod(full{i64_min}, full{-1}, rounded_ceil);
    EXPECT_EQ(to_string(qr->Quotient), "9223372036854775808");
    EXPECT_EQ(qr->Remainder, 0_ins);
}

// The Q-format path's 32-bit work type wrapped on its minimum over −Notch.
TEST(BugHunt20261012, qformat_division_min_by_minus_notch) {
    using q15_8  = inside<{{-32768, 32767}, per<256>}>;
    const auto r = div(q15_8{-32768}, q15_8{rational{-1, 256}}, rounded_toward_zero);
    EXPECT_EQ(to_string(*r), "8388608");
}

// Sums counted in a unit finer than the result's notch (an unanchored
// result): the offset from Lower in that unit passed imax.
TEST(BugHunt20261012, unanchored_sum_near_int64) {
    const auto s = halves{rational{1, 2}} + thirds{3ll << 60};
    EXPECT_EQ(to_string(s), "3458764513820540928.5");
    EXPECT_EQ(to_string(halves{rational{1, 2}} - thirds{-(3ll << 60)}), "3458764513820540928.5");
}

// A lattice whose values pass the 64-bit rational between limits that fit
// ({{−1.2e19, 1.2e19}, 1.5}): reading such a value threw.
TEST(BugHunt20261012, lattice_past_the_rational) {
    const auto p = neg{rational{-7, 2}} * thirds{(3ll << 60) - 3};
    EXPECT_EQ(to_string(p), "-12105675798371893237.5");
    using clamped = inside<grid_of<neg>, clamp | snap>;
    using wrapped = inside<grid_of<neg>, wrap | round_floor>;
    EXPECT_EQ(*clamped::try_make(p), -3.5_ins);
    EXPECT_EQ(to_string(*wrapped::try_make(p)), "-1.5");
}

// The same-notch fast path of a wide store took any equal notch, so an
// offset lattice (0.5 + k) shifted by a truncated 0.5: every mode floored.
TEST(BugHunt20261012, wide_store_from_offset_lattice) {
    constexpr grid wide{{-0x1p63, 0x1p63}, 1};
    using ceil_t    = inside<wide, round_ceil>;
    using nearest_t = inside<wide, round_nearest>;
    using trunc_t   = inside<wide, snap>;
    EXPECT_EQ(*ceil_t::try_make(halves{rational{3, 2}}), 2_ins);
    EXPECT_EQ(*nearest_t::try_make(halves{rational{3, 2}}), 2_ins);
    EXPECT_EQ(*trunc_t::try_make(neg{rational{-5, 2}}), -2_ins);
}

// A double into a grid whose limit no double holds (INT64_MAX): the range
// test built the double as a grid number, whose runtime ops stop at 64 bits
// under C++26 — 2^-20 needed a 2^72 denominator on the way and threw.
TEST(BugHunt20261012, small_double_into_full_int64) {
    using F = inside<{i64_min, i64_max}, round_half_even>;
    EXPECT_EQ(*F::try_make(0x1p-20), 0_ins);
    EXPECT_EQ(*F::try_make(1e-4), 0_ins);
    EXPECT_EQ(*F::try_make(-2.5), -2_ins);
    using C = inside<{i64_min, i64_max}, clamp | round_ceil>;
    EXPECT_EQ(*C::try_make(0x1p-70), 1_ins);
}

// A rational just outside the grid with a denominator near 2^62: rounding
// it onto the lattice threw bad_expected_access (v·64 passed the rational).
TEST(BugHunt20261012, fine_rational_rounds_onto_the_grid) {
    using A        = inside<{{0, 1}, per<64>}, round_nearest>;
    const imax d   = (1ll << 62) + 1;
    const auto top = rational{static_cast<umax>(d + 1), d}; // 1 + 1/d
    EXPECT_EQ(*A::try_make(top), 1_ins);
    EXPECT_EQ(*A::try_make(rational{1, -d}), 0_ins);
    EXPECT_FALSE(conversion_overflows<A>(top));
    EXPECT_TRUE(conversion_rounds<A>(top));
}

// += of a rational on a large Q-format grid: the raw add passed imax (UB).
TEST(BugHunt20261012, add_rational_on_large_qformat) {
    inside<{{0, 1ll << 60}, per<4>}, clamp> c{rational{1ll << 60}};
    c += rational{1ll << 60};
    EXPECT_EQ(to_string(c), "1152921504606846976");
    inside<{{0, 1ll << 60}, per<4>}, wrap> w{rational{1ll << 60}};
    w += rational{1ll << 60};
    EXPECT_EQ(to_string(w), "1152921504606846975.75");
}

// += of an inside under wrap: the new raw's offset from a negative raw_lo
// passed the work type.
TEST(BugHunt20261012, add_wrap_offset_from_negative_lower) {
    using L = inside<{-(1ll << 62), 1ll << 61}, wrap>;
    using R = inside<{0, 1ll << 62}>;
    L x{1ll << 61};
    x += R{1ll << 62};
    EXPECT_EQ(x, -1_ins);
    L y{-(1ll << 62)};
    y -= R{1ll << 62};
    EXPECT_EQ(to_string(y), "-2305843009213693951");
}

// wrap folds a value whose 64-bit rational steps overflow exactly: it
// reported errc::overflow (a double past 2^63, INT64_MIN into a 1/4 grid, a
// far value on a 2^-62 grid).
TEST(BugHunt20261012, wrap_folds_past_the_rational) {
    using quarter_wrap = inside<{{-2, 3}, per<4>}, wrap>;
    using halves_ceil  = inside<{{0.5, 10.5}, 1}, wrap | round_ceil>;
    using digit        = inside<{0, 9}, wrap>;
    using fine         = inside<{{0, 1}, per<(1ll << 62)>}, wrap>;
    EXPECT_EQ(*quarter_wrap::try_make(full{i64_min}), 2.5_ins);
    EXPECT_EQ(*halves_ceil::try_make(full{i64_min}), 3.5_ins);
    EXPECT_EQ(*digit::try_make(-0x1p63), 2_ins);
    EXPECT_EQ(*digit::try_make(1e30), 6_ins); // the double's exact value, mod 10
    EXPECT_EQ(to_string(*full::try_make<wrap>(1e30)), "5076964154930102272");
    EXPECT_EQ(*fine::try_make(rational{4}), (rational{(1ull << 62) - 3, 1ll << 62})); // 4 mod (1 + 2^-62)
    EXPECT_EQ(digit::try_make(1.5).error(), errc::rounding_error);                    // no rounding flag
}

// Operations are constrained on having a result: a probe sees the answer,
// and an inside does not fall back on its rational conversion (or on its
// point-grid conversion) when its own operator is out.
namespace {
template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };
template <class A, class B>
constexpr bool can_mul = requires(A a, B b) { a * b; };
template <class A, class B>
constexpr bool can_div = requires(A a, B b) { a / b; };
template <class A, class B>
constexpr bool can_mod = requires(A a, B b) { a % b; };
template <class A, class B>
constexpr bool can_divmod = requires(A a, B b) { divmod(a, b); };
template <class T, class S>
constexpr bool can_make = requires(S s) { T::try_make(s); };
template <class T, class S>
constexpr bool can_add_assign = requires(T t, S s) { t += s; };
template <class T, class S>
constexpr bool can_div_assign = requires(T t, S s) { t /= s; };

using digit   = inside<{0, 9}>;
using reals   = inside<{{-2, 2}, max_denominator{1000}}>;
using fourths = inside<{{-2, 3}, per<4>}>;
} // namespace

TEST(BugHunt20261012, operations_are_constraint_friendly) {
    static_assert(can_div<digit, digit> && can_div<reals, digit>);
    static_assert(can_div<digit, reals>);        // a bounded divisor stays 1/1000 from 0
    static_assert(!can_div<digit, inside<{0}>>); // the point 0
    static_assert(can_mod<digit, digit> && !can_mod<fourths, digit>);
    static_assert(can_divmod<digit, inside<{1, 3}>> && !can_divmod<fourths, digit>);
    static_assert(can_add<digit, fourths> && can_mul<fourths, fourths>);
    static_assert(can_add<uint64, uint64> && can_mul<full, full>);
    static_assert(can_make<digit, int> && can_make<digit, inside<{5, 20}>> && !can_make<digit, inside<{20, 30}>>);
    static_assert(!can_make<digit, fourths> && can_make<inside<{0, 9}, snap>, fourths>);
    static_assert(can_add_assign<digit, digit> && can_add_assign<digit, inside<{10}>> &&
                  !can_add_assign<digit, fourths>);
    static_assert(!can_div_assign<digit, inside<{0}>> && can_div_assign<inside<{0, 9}, snap>, digit>);
}
