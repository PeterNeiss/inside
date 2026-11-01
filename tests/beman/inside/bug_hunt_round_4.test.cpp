// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the fourth fuzzing round (bounded continuous grids, ...). The
// static_asserts did not compile before the fixes; the runtime cases stored
// wrong values or reported the wrong error.

#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

// Whether numeric_limits<T> has epsilon() / round_error(): only where 0 is a value.
template <class T>
concept has_epsilon = requires { std::numeric_limits<T>::epsilon(); };
template <class T>
concept has_round_error = requires { std::numeric_limits<T>::round_error(); };

using namespace beman::inside;

namespace {
constexpr grid sevenths{interval{0, 1}, max_denominator{7}};
constexpr grid wholes{interval{0, 9}, max_denominator{1}};
using thirds = inside<{{rational{1, 3}, rational{31, 3}}, rational{2, 3}}, round_nearest>;
using fine   = inside<{{0, 1}, per<(1ll << 62)>}, round_nearest>;
} // namespace

// A rounding policy rounds before the range check (policies.md), as the
// lattices do: 9.5 floors onto 9. A bounded continuous grid checked the
// range first and reported overflow.
TEST(BugHuntRound4, bounded_grid_rounds_before_the_range_check) {
    static_assert(*inside<wholes, round_floor>::try_make(rational{19, 2}) == 9);
    static_assert(*inside<sevenths, round_nearest>::try_make(rational{22, 21}) == 1);
    static_assert(*inside<sevenths, round_ceil>::try_make(rational{-1, 50}) == 0);
    static_assert(inside<sevenths, round_ceil>::try_make(rational{22, 21}).error() == errc::overflow);
    EXPECT_EQ((*inside<wholes, round_floor>::try_make(9.5)), 9);
    inside<sevenths, round_nearest> x{rational{5, 7}};
    x += rational{1, 3}; // 22/21
    EXPECT_EQ(x, 1);
}

// A value past the 64-bit rational (a double finer than 2^-62) lying within
// a bounded grid's limits reported overflow: it rounds onto the bound, or is
// off the grid (rounding_error).
TEST(BugHuntRound4, bounded_grid_rounds_a_fine_double) {
    EXPECT_EQ((*inside<sevenths, round_nearest>::try_make(5.1e-12)), 0);
    EXPECT_EQ((*inside<sevenths, round_ceil>::try_make(5.1e-12)), rational(1, 7));
    EXPECT_EQ(inside<sevenths>::try_make(5.1e-12).error(), errc::rounding_error);
    EXPECT_EQ(inside<sevenths>::try_make(1e-300).error(), errc::rounding_error);
}

// clamp saturated every out-of-range value but a double finer than 2^-62:
// on a continuous grid it reported overflow.
TEST(BugHuntRound4, continuous_grid_clamps_a_fine_double) {
    EXPECT_EQ(*(inside<{{0, 1}, max_denominator{1000}}, clamp>::try_make(-1e-20)), 0);
    EXPECT_EQ(*(inside<{{1, 2}, max_denominator{1000}}, clamp>::try_make(1e-20)), 1);
    EXPECT_EQ((*inside<sevenths, clamp>::try_make(-1e-20)), 0);
    EXPECT_EQ((inside<{{0, 1}, max_denominator{1000}}>::try_make(-1e-20).error()), errc::overflow);
}

// Two integer raws whose common unit (the gcd of a 2/3 and a 2^-62 notch) is
// past the 64-bit rational: comparing them did not compile (the unit's
// expected was read in a constant expression); they compare by value.
TEST(BugHuntRound4, compare_without_a_common_unit) {
    static_assert(thirds{rational{1, 3}} < fine{rational{1, 2}});
    static_assert(thirds{1} > fine{rational{1, 2}});
    static_assert(thirds{1} == fine{1});
    EXPECT_EQ((thirds{rational{1, 3}} <=> fine{rational{1, 2}}), std::strong_ordering::less);
}

// A deduced output limit k·Notch with the index k past 2^63 (π, √2 on a
// 2^-62 notch) wrapped through imax: atan2's grid came out as [0.86, −0.86]
// and did not compile.
TEST(BugHuntRound4, deduced_limit_past_int64_index) {
    using A = decltype(math::atan2(fine{}, fine{}));
    static_assert(lower_of<A> < -3 && upper_of<A> > 3);
    using H = decltype(math::hypot(fine{}, fine{}));
    static_assert(upper_of<H> > rational(141, 100) && upper_of<H> < rational(142, 100));
    EXPECT_EQ(math::atan2(fine{rational{1, 2}}, fine{rational{1, 2}}), math::atan2(fine{1}, fine{1}));
    EXPECT_GT(math::hypot(fine{1}, fine{1}), rational(141, 100));
}

// numeric_limits built min/max/lowest/epsilon from the 64-bit rational: on
// a grid past 64 bits they did not compile.
TEST(BugHuntRound4, numeric_limits_of_a_big_grid) {
    using big = inside<{-1237940039285380274899124224_g, 1237940039285380274899124224_g}>; // ±2^90
    using far = inside<{1267650600228229401496703205376_g, 1267650600228229401496703205386_g}>;
    static_assert(std::numeric_limits<big>::lowest() < 0 && std::numeric_limits<big>::max() > 0);
    static_assert(std::numeric_limits<big>::epsilon() == 0);
    static_assert(!has_epsilon<far>); // 0 is no value of far
    EXPECT_EQ(to_string(std::numeric_limits<big>::lowest()), "-1237940039285380274899124224");
    EXPECT_EQ(to_string(std::numeric_limits<far>::max()), "1267650600228229401496703205386");
    EXPECT_EQ(to_string(std::numeric_limits<big>::max()), "1237940039285380274899124224");
}

// epsilon of a lattice that spans 0 but misses it: none, 0 is no value.
TEST(BugHuntRound4, numeric_limits_epsilon_off_zero) {
    static_assert(!has_epsilon<inside<{{-0.5, 0.5}, 1}>>);
    static_assert(!has_round_error<inside<{{rational{-7, 3}, rational{5, 3}}, rational{2, 3}}>>);
}

// A failed store of a double finer than 2^-62 through errc& or on_error
// replaced the destination with a value-initialised raw (0/0 on a
// continuous grid) instead of leaving it intact.
TEST(BugHuntRound4, failed_fine_double_store_keeps_the_value) {
    using C = inside<{{0, 1}, max_denominator{1000}}>;
    C    z{rational{1, 2}};
    errc ec{};
    z.policy(ec) = 1e-30; // finer than the bound: off the grid
    EXPECT_EQ(ec, errc::rounding_error);
    EXPECT_EQ(z, rational(1, 2));
    C    w{rational{1, 2}};
    errc cb{};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) = 1e-30;
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(w, rational(1, 2));
}

// The errc& / on_error proxy's compound ops called add/sub/... with no
// constraint: on an int64 grid (whose sum grid passes 64 bits) a requires
// probe said yes and the call did not compile. Plain += takes the raw path.
// Nor did it check that the result stores back: a 1/16 sum into an integer
// grid needs snap (plain += says no; the proxy said yes, then static_assert).
template <class T, class U = T>
constexpr bool proxy_add_ok = requires(T a, U b, errc& e) { a.policy(e) += b; };
template <class T, class U = T>
constexpr bool plain_add_ok = requires(T a, U b) { a += b; };
TEST(BugHuntRound4, policy_proxy_compound_is_constrained) {
    using sixteenth = inside<{{0, 8}, per<16>}>;
    using whole     = inside<{-100, 100}>;
    static_assert(!proxy_add_ok<whole, sixteenth> && !plain_add_ok<whole, sixteenth>);
    static_assert(proxy_add_ok<inside<{0, 100}>> && proxy_add_ok<sixteenth, whole>);
}

// on_error was dropped when the value to store came from a continuous inside
// (a quotient) and was off the notch: the store reported to the policy, which
// threw, instead of calling the handler.
TEST(BugHuntRound4, on_error_sees_rounding_from_a_continuous_source) {
    using I = inside<{0, 100}>;
    I    w{7};
    errc cb{};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) /= I{2}; // 7/2
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(w, 7);
}

// A double finer than 2^-62 stored into a default-constructed inside copied
// its uninitialized raw (not a constant expression; indeterminate at run time).
TEST(BugHuntRound4, fine_double_into_an_uninitialized_inside) {
    using M          = inside<sevenths, round_nearest>;
    constexpr auto f = [] {
        M x;
        x = 0x1p-70;
        return x;
    };
    static_assert(f() == 0);
    M x;
    x = 0x1p-70;
    EXPECT_EQ(x, 0);
    // A failure still reaches on_error with the destination's value; a clamp
    // policy saturates instead (no failure).
    using U = inside<{{0, 1}, max_denominator{1000}}>;
    U    u{rational{1, 2}};
    errc cb{};
    u.on_error([&](auto& v, errc c, const char*) {
        cb = c;
        EXPECT_EQ(v, rational(1, 2));
    }) = 1e-30;
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(u, rational(1, 2));
    using C = inside<{{1, 2}, max_denominator{1000}}, clamp>;
    C w{rational{3, 2}};
    cb                                                      = {};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) = 1e-30;
    EXPECT_EQ(cb, errc{});
    EXPECT_EQ(w, 1);
}

// A bound of 2^63 or more: the range prefilter's step 1/MaxDenominator
// narrowed to imax and did not compile.
TEST(BugHuntRound4, bound_past_imax) {
    using B = inside<{{0, 1}, max_denominator{rational{umax{1} << 63}}}, snap>;
    B x{};
    x = 0.3;
    EXPECT_EQ(x, rational(0.3));
    x = 0x1p-70;
    EXPECT_EQ(x, 0);
    errc ec{};
    x.policy(ec) = 1.0 + 0x1p-52;
    EXPECT_EQ(ec, errc::overflow);
}
