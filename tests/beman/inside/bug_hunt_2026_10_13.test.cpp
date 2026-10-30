// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the 2026-10-13 fuzzing round (point grids, the exact store
// path, constraint honesty of casts and math). The static_asserts did not
// compile before the fixes; the runtime cases stored wrong values, reported
// the wrong error or terminated.

#include <beman/inside/inside.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <cstdint>

using namespace beman::inside;

namespace {
using three   = inside<{3}>;              // a point grid
using halves  = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
using digit   = inside<{0, 9}>;
using small   = inside<{-7, 5}>;
using fourths = inside<{{-2, 3}, per<4>}>;
using fine    = inside<{{0, 1}, per<(1ll << 62)>}>;
} // namespace

// An inside stored into a point grid skipped the range check: the raw map
// sends every raw onto the point's one raw, so 4 and 3.5 both "fit" {3}.
TEST(BugHunt20261013, point_grid_checks_the_value) {
    static_assert(three::try_make(small{4}).error() == errc::overflow);
    static_assert(three::try_make(halves{rational{7, 2}}).error() == errc::overflow);
    static_assert(*three::try_make(small{3}) == 3_ins);
    using clamped = inside<{3}, clamp>;
    static_assert(*clamped::try_make(small{4}) == 3_ins);
    three x{};
    EXPECT_THROW(x += small{3}, inside_error); // 6 is not 3
}

// A source the exact path stores (a fine or wide value) into a point grid
// divided by the point's zero notch inside noexcept code: std::terminate.
// wrap onto a point (no period) terminated the same way since the exact wrap
// fold; it reports overflow, as before.
TEST(BugHunt20261013, point_grid_on_the_exact_path) {
    three x{};
    EXPECT_THROW((x += fine{rational{1, 2}}), inside_error);
    EXPECT_EQ(three::try_make(fine{rational{1, 2}} + 2_ins).error(), errc::overflow);
    using wrapped = inside<{3}, wrap>;
    EXPECT_EQ(wrapped::try_make(5).error(), errc::overflow);
    EXPECT_EQ(wrapped::try_make(3.5).error(), errc::overflow);
    EXPECT_EQ(*wrapped::try_make(3), 3_ins);
}

// The exact store (mul_into, div_into, wide sums) tested the notch before the
// range: an off-notch value past a limit reported rounding_error, and clamp
// did not clamp it — the builtin stores range-check the value first.
TEST(BugHunt20261013, exact_store_checks_range_before_notch) {
    const fourths a{rational{-1, 4}}, b{rational{11, 4}}; // −11/16
    using clamped = inside<{0, 9}, clamp>;
    EXPECT_EQ(mul_into<clamped>(a, b), 0_ins);
    try {
        (void)mul_into<digit>(a, b);
        ADD_FAILURE() << "no error";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::overflow);
    }
    try {
        (void)mul_into<digit>(b, b); // 121/16: in range, off the notch
        ADD_FAILURE() << "no error";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::rounding_error);
    }
}

// A slot count past 2^63 over a negative base: the value index overflowed
// imax on the way (UB; constant evaluation rejects it), though the index fits.
TEST(BugHunt20261013, value_index_past_int64_raw) {
    using wide_q = inside<{{-(1ll << 40), 1ll << 40}, per<(1ll << 20)>}, clamp>;
    static_assert([] {
        wide_q c{rational{1ll << 39}};
        c *= small{5}; // 5·2^39: the product's raw passes 2^63
        return c == wide_q{rational{1ll << 40}};
    }());
}

// Casts, abs_into and pown are constrained like the operations they wrap,
// so a requires-probe answers whether the call compiles.
namespace {
template <class B, class S>
constexpr bool can_clamp_cast = requires(S s) { clamp_cast<B>(s); };
template <class B, class S>
constexpr bool can_checked_cast = requires(S s) { checked_cast<B>(s); };
template <class O, class I>
constexpr bool can_abs_into = requires(I x) { math::abs_into<O>(x); };
template <imax E, class I>
constexpr bool can_pown = requires(I x) { math::pown<E>(x); };
template <class A, class B>
constexpr bool can_mul = requires(A a, B b) { a * b; };
} // namespace

TEST(BugHunt20261013, casts_and_math_are_constraint_friendly) {
    static_assert(can_clamp_cast<digit, int> && can_clamp_cast<digit, small>);
    static_assert(!can_clamp_cast<halves, three> && can_checked_cast<halves, three>); // 3 is off the .5 lattice
    static_assert(can_abs_into<digit, small> && !can_abs_into<inside<{1, 10}>, small>);
    static_assert(can_pown<5, digit> && can_pown<0, inside<{{-2, 2}, max_denominator{1000}}>>);
    EXPECT_EQ(math::pown<3>(small{-2}), -8_ins);
    EXPECT_EQ(clamp_cast<digit>(small{-3}), 0_ins);
}

// A named policy (`clamped`, `rounded_floor`, a const policy(ec)) as the
// constructor's one-shot policy did not compile: report() was non-const.
TEST(BugHunt20261013, named_policy_in_constructor) {
    static_assert(digit{12, clamped} == 9_ins && digit{12, wrapped} == 2_ins);
    static_assert(halves{7, rounded_floor} == 6.5_ins);
    errc       ec{};
    const auto pol = make_policy<checked>(ec);
    digit      y{12, pol};
    (void)y;
    EXPECT_EQ(ec, errc::overflow);
}
