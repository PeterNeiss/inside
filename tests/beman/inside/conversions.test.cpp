// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Out of an inside: to<T>, as<T>, the conversion predicates and casts,
// numerator/denominator, points (just<…>) and their stores.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/math.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "support.hpp"

// inside_to
namespace inside_to_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// inside::to<unsigned T>
// to<T>() is an expected only where it can fail: a grid of integers within
// T's limits, a requested rounding, and every floating T give a plain T.
TEST(InsideToTest, to_is_plain_where_it_cannot_fail) {
    using digits = inside<{0, 9}>;
    using money  = inside<{{0, 100}, per<100>}>;
    static_assert(std::same_as<decltype(digits{}.to<std::uint8_t>()), std::uint8_t>);
    static_assert(std::same_as<decltype(inside<{0, 1000}>{}.to<std::uint8_t>()), std::expected<std::uint8_t, errc>>);
    static_assert(std::same_as<decltype(inside<{-5, 5}>{}.to<unsigned>()), std::expected<unsigned, errc>>);
    static_assert(std::same_as<decltype(money{}.to<int>()), std::expected<int, errc>>); // 0.25 does not round
    static_assert(std::same_as<decltype(money{}.to<int, round_floor>()), int>);
    static_assert(std::same_as<decltype(money{}.to<double>()), double>);
    EXPECT_EQ((money{2.75}.to<int, round_floor>()), 2);
    EXPECT_EQ(digits{7}.as<int>(), 7);
}

TEST(InsideToTest, inside_to_unsigned_t) {
    {
        SCOPED_TRACE("trivially-fits grid takes the fast path");
        using B = inside<{0, 100}>;
        ASSERT_TRUE(B{42}.to<std::uint8_t>() == 42);
        ASSERT_TRUE(B{0}.to<std::uint32_t>() == 0);
        ASSERT_TRUE(B{100}.to<std::size_t>() == 100);
    }

    {
        SCOPED_TRACE("runtime overflow when Upper > T::max");
        using B = inside<{0, 1000}>;
        auto r  = B{500}.to<std::uint8_t>();
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::overflow);
    }

    {
        SCOPED_TRACE("negative inside to unsigned -> domain_error");
        using B = inside<{-10, 10}>;
        auto r  = B{-5}.to<std::uint8_t>();
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::domain_error);
    }

    {
        SCOPED_TRACE("a fractional value needs permission to round");
        using B = inside<{{0, 1}, per<2>}>;
        ASSERT_EQ(B{0.5}.to<std::uint8_t>().error(), errc::rounding_error); // no rounding named
        ASSERT_TRUE((B{0.5}.to<std::uint8_t, round_toward_zero>() == 0));   // 0.5 -> 0
        ASSERT_TRUE(B{1}.to<std::uint8_t>().value() == 1);
    }

    {
        SCOPED_TRACE("Q-format fast path");
        using B = inside<{{0, 255}, per<256>}, round_nearest>;
        ASSERT_TRUE(B{42.5}.to<std::uint8_t>() == 43); // the type's mode: a tie away from zero
    }
}

// inside::to<signed T>
TEST(InsideToTest, inside_to_signed_t) {
    {
        SCOPED_TRACE("trivially-fits grid takes the fast path");
        using B = inside<{-50, 50}>;
        ASSERT_TRUE(B{-7}.to<std::int8_t>() == -7);
        ASSERT_TRUE(B{42}.to<std::int32_t>() == 42);
    }

    {
        SCOPED_TRACE("overflow upward -> errc::overflow");
        using B = inside<{0, 1000}>;
        auto r  = B{500}.to<std::int8_t>();
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::overflow);
    }

    {
        SCOPED_TRACE("overflow downward -> errc::overflow");
        using B = inside<{-1000, 0}>;
        auto r  = B{-500}.to<std::int8_t>();
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::overflow);
    }
}

// inside::to<floating T>
TEST(InsideToTest, inside_to_floating_t) {
    {
        SCOPED_TRACE("ordinary value");
        using B = inside<{{0, 1}, per<2>}>;
        ASSERT_TRUE(B{0.5}.to<double>() == 0.5);
    }

    {
        SCOPED_TRACE("to<double> works even on a strict-policy inside (no policy gate)");
        using B = inside<{0, 10}, checked>;
        ASSERT_TRUE(B{3}.to<double>() == 3.0);
    }
}

// inside -> rational conversion
TEST(InsideToTest, inside_to_rational_conversion) {
    {
        SCOPED_TRACE("ordinary value");
        using B = inside<{0, 10}>;
        ASSERT_TRUE(static_cast<rational>(B{5}) == 5);
    }

    {
        SCOPED_TRACE("fractional grid is preserved exactly");
        using B = inside<{{0, 1}, per<2>}>;
        ASSERT_TRUE(static_cast<rational>(B{0.5}) == 0.5_r);
    }
}

// No implicit integer conversion, whatever the grid: as<T>() / to<T>() read it.
TEST(InsideToTest, integers_are_read_with_as_or_to) {
    using narrow = inside<{0, 100}>;
    using wide   = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;
    static_assert(!std::is_convertible_v<narrow, imax>);
    static_assert(!std::is_convertible_v<narrow, std::size_t>);
    static_assert(!std::is_convertible_v<wide, imax>);
    static_assert(!std::is_convertible_v<wide, std::size_t>);
    ASSERT_EQ(narrow{42}.as<imax>(), 42);
    // A grid past T's range reports the value that does not fit.
    ASSERT_EQ(narrow{42}.to<std::int8_t>(), std::int8_t{42});
    ASSERT_EQ((inside<{0, 300}>{300}.to<std::uint8_t>()), std::unexpected{errc::overflow});
}

// An index reads as a size_t: as<std::size_t>() keeps no check on an
// index-shaped grid.
TEST(InsideToTest, as_size_t_indexes_a_container) {
    using idx_t = inside<{0, 9}>;
    std::vector<int> v{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    idx_t            i{3};
    ASSERT_EQ(v[i.as<std::size_t>()], 30);
    std::size_t s = i.as<std::size_t>();
    ASSERT_EQ(s, 3u);
}

// operator double is gated on rounding policy
TEST(InsideToTest, operator_double_is_gated_on_rounding_policy) {
    using B_round  = inside<{{0, 1}, per<2>}, round_nearest>;
    using B_ignore = inside<{{0, 1}, per<2>}, round_toward_zero>;
    using B_strict = inside<{{0, 1}, per<2>}, checked>;
    using B_floor  = inside<{{0, 1}, per<2>}, round_floor>;

    // operator double() is explicit, so use is_constructible_v to detect it.
    static_assert(std::is_constructible_v<double, B_round>);
    static_assert(std::is_constructible_v<double, B_ignore>);
    static_assert(std::is_constructible_v<double, B_floor>);
    static_assert(!(std::is_constructible_v<double, B_strict>));

    // The typed-error path is always available.
    ASSERT_TRUE(B_strict{0.5}.to<double>() == 0.5);
}

// as<T>() is a non-expected shortcut for to<T>().value()
TEST(InsideToTest, as_t_is_a_non_expected_shortcut_for_to_t_value) {
    using narrow = inside<{0, 100}>;
    using frac   = inside<{{0, 100}, per<10>}, round_nearest>;

    // Matches the integer-extraction value path.
    ASSERT_TRUE(narrow{42}.as<imax>() == 42);
    ASSERT_TRUE(narrow{42}.as<std::size_t>() == 42u);

    // Fractional notch: extracting to imax rounds by the type's mode (matches to<imax>()).
    ASSERT_TRUE(frac{7.5}.as<imax>() == 8);

    // Double target behaves the same as `to<T>().value()`; the inside -> rational
    // conversion is the implicit operator.
    ASSERT_TRUE(static_cast<rational>(narrow{42}) == 42);
    ASSERT_TRUE(frac{7.5}.as<double>() == 7.5);

    // Free-function forms — template-friendly (no `.template` disambiguator),
    // found by ADL, same semantics as the members.
    ASSERT_TRUE(as<imax>(narrow{42}) == 42);
    ASSERT_TRUE(as<double>(frac{7.5}) == 7.5);
    ASSERT_TRUE(to<imax>(frac{7.5}) == 8);
    ASSERT_TRUE((to<imax, round_floor>(frac{7.5}) == 7));
    ASSERT_TRUE(to<double>(narrow{3}) == 3.0);
}

// Probes must stay dependent so a gated call yields `false` instead of a
// hard error (requires-expressions only SFINAE during substitution).
template <typename B, typename T>
concept has_member_as = requires(B b) { b.template as<T>(); };
template <typename B, typename T>
concept has_free_as = requires(B b) { as<T>(b); };
template <typename B, typename T>
concept has_member_to = requires(B b) { b.template to<T>(); };

// as<floating> shares operator double's policy gate
TEST(InsideToTest, as_floating_shares_operator_double_s_policy_gate) {
    using strict_b  = inside<{0, 100}>; // no rounding flag
    using rounded_b = inside<{0, 100}, round_nearest>;

    // Strict: both spellings of the direct double read are rejected;
    // to<double>() stays the explicit opt-in.
    static_assert(!has_member_as<strict_b, double>);
    static_assert(!has_free_as<strict_b, double>);
    static_assert(has_member_to<strict_b, double>);

    // Integral as<> is not policy-gated.
    static_assert(has_member_as<strict_b, imax>);

    // A rounding flag opens the gate.
    static_assert(has_member_as<rounded_b, double>);
    ASSERT_TRUE(rounded_b{42}.as<double>() == 42.0);
}

// as<T>() reports a failed conversion like a checked store: inside_error with
// to<T>()'s code, through the replaceable handler.
TEST(InsideToTest, as_reports_through_inside_error) {
    using s = inside<{-5, 5}>;
    try {
        (void)s{-1}.as<unsigned>();
        FAIL() << "as<unsigned>() of -1 did not report";
    } catch (const inside_error& e) {
        ASSERT_EQ(e.Code, errc::domain_error);
    }
    ASSERT_EQ(s{3}.as<int>(), 3);
}
} // namespace inside_to_tests

// constant
namespace constant_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Regression: a single-point source inside (e.g. `0_ins`, any `just<N>`) must
// assign into ANY grid that can exactly represent the value — including grids
// with a non-unit notch, where the whole-range notch mapping is inexact. Before
// the fix, `inside_assignable`'s notch clause rejected these even though the one
// value lands on a notch. The integer path (`b = 0`) already worked, so the
// insidable path was simply inconsistent with it.
//---------------------------------------------------------------------------
// point-inside assignment onto non-unit notch grids
TEST(ConstantTest, point_inside_assignment_onto_non_unit_notch_grids) {
    // values {0,3,6,9}
    static_assert([] {
        inside<{{0, 9}, 3}> b = 0_ins;
        return b == 0;
    }());
    static_assert([] {
        inside<{{0, 9}, 3}> b = 3_ins;
        return b == 3;
    }());
    static_assert([] {
        inside<{{0, 9}, 3}> b = 9_ins;
        return b == 9;
    }());
    // values {0,2,..,10}
    static_assert([] {
        inside<{{0, 10}, 2}> b = 0_ins;
        return b == 0;
    }());
    static_assert([] {
        inside<{{0, 10}, 2}> b = 4_ins;
        return b == 4;
    }());
    // offset grid (non-zero Lower), values {3,6,9}
    static_assert([] {
        inside<{{3, 9}, 3}> b = 6_ins;
        return b == 6;
    }());

    // The insidable path now matches the integer path for the same grid.
    static_assert([] {
        inside<{{0, 9}, 3}> b = 0;
        return b == 0;
    }());
    static_assert([] {
        inside<{{0, 9}, 3}> b = 0_ins;
        return b == 0;
    }());
}

// point-inside assignment rejected when not representable
TEST(ConstantTest, point_inside_assignment_rejected_when_not_representable) {
    // not on a notch
    static_assert(!inside_assignable<inside<{{0, 9}, 3}>, decltype(1_ins)>);
    static_assert(!inside_assignable<inside<{{0, 10}, 2}>, decltype(3_ins)>);
    // out of range (valid grids: Lower divisible by notch)
    static_assert(!inside_assignable<inside<{{3, 9}, 3}>, decltype(0_ins)>);
    static_assert(!inside_assignable<inside<{6, 12}>, decltype(0_ins)>);

    // still admitted where representable
    static_assert(inside_assignable<inside<{{0, 9}, 3}>, decltype(0_ins)>);
    static_assert(inside_assignable<inside<{{3, 9}, 3}>, decltype(6_ins)>);
}

//---------------------------------------------------------------------------
// just<0> / just<1> — exact point constants.
//---------------------------------------------------------------------------
// just<0> / just<1> assign across storage kinds
TEST(ConstantTest, just_0_and_just_1_assign_across_storage_kinds) {
    static_assert([] {
        inside<{0, 200}> b = just<0>;
        return b == 0;
    }());
    static_assert([] {
        inside<{0, 200}> b = just<1>;
        return b == 1;
    }());
    static_assert([] {
        inside<{-40, 60}> b = just<0>;
        return b == 0;
    }()); // signed value storage
    static_assert([] {
        inside<{-40, 60}> b = just<1>;
        return b == 1;
    }());
    static_assert([] {
        inside<{{0, 9}, 3}> b = just<0>;
        return b == 0;
    }()); // non-unit notch
    // Q8.8: value 1 is raw 256.
    static_assert([] {
        inside<{{0, 1}, 0x1p-8_r}> b = just<1>;
        return static_cast<imax>(b.raw()) == 256;
    }());

    // assignment operator (not just construction)
    static_assert([] {
        inside<{0, 200}> b = 5;
        b                  = just<0>;
        return b == 0;
    }());
    static_assert([] {
        inside<{0, 200}> b = 5;
        b                  = just<1>;
        return b == 1;
    }());
}

// just<0> / just<1> rejected where not representable
TEST(ConstantTest, just_0_and_just_1_rejected_where_not_representable) {
    static_assert(!inside_assignable<inside<{5, 10}>, decltype(just<0>)>);     // 0 out of range
    static_assert(!inside_assignable<inside<{{0, 9}, 3}>, decltype(just<1>)>); // 1 not on notch
}

// just<0> / just<1> in comparison and arithmetic
TEST(ConstantTest, just_0_and_just_1_in_comparison_and_arithmetic) {
    using B = inside<{0, 200}>;

    // comparison (both orders — C++20 rewritten/reversed candidates)
    static_assert(B{0} == just<0>);
    static_assert(just<0> == B{0});
    static_assert(B{1} == just<1>);
    static_assert(B{5} > just<0>);
    static_assert(just<0> < B{5});
    static_assert(B{0} < just<1>);

    // arithmetic (forwards through the ordinary inside operators)
    static_assert((B{5} + just<1>) == 6);
    static_assert((just<1> + B{5}) == 6);
    static_assert((B{5} - just<0>) == 5);
    static_assert((B{5} - just<1>) == 4);

    // a runtime check too, for good measure
    B b = 41;
    b   = b + just<1>;
    ASSERT_EQ(b, 42);
}

// just<N, D> is the point N/D.
TEST(ConstantTest, just_numerator_denominator) {
    static_assert(rational{just<1, 3>} == rational(1, 3));
    static_assert(rational{just<-6, 5>} == rational(-6, 5));
    static_assert(std::is_same_v<decltype(just<4, 2>), decltype(just<2>)>);
    static_assert(std::is_same_v<decltype(just<7>), decltype(just<7, 1>)>);
    static_assert(lower_of<inside<{{just<-1, 10>, just<1, 10>}, per<1000>}>> == rational(-1, 10));
}
} // namespace constant_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

#define WIDE_LONG_DOUBLE (LDBL_MANT_DIG > DBL_MANT_DIG)

template <class T, class F>
concept compares_with = requires(T t, F f) { t == f; } || requires(T t, F f) { t <=> f; } ||
                        requires(T t, F f) { f == t; } || requires(T t, F f) { f < t; };
template <class B, class S>
constexpr bool can_clamp_cast = requires(S s) { B{s, clamp}; };
template <class B, class S>
constexpr bool can_checked_cast = requires(S s) { checked_cast<B>(s); };
template <class O, class I>
constexpr bool can_abs_into = requires(I x) { math::abs_into<O>(x); };
template <imax E, class I>
constexpr bool can_pown = requires(I x) { math::pown<E>(x); };
template <class T, class F>
concept compares_with_fp = requires(T t, F f) { t == f; } || requires(T t, F f) { t <=> f; };
template <class T>
concept casts_to_imax = requires(T t) { static_cast<imax>(t); };
template <class T>
concept indexes_a_vector = requires(std::vector<int>& v, T t) { v[t]; };

TEST(ConversionTest, predicates_on_tiny_doubles) {
    using unit = inside<{{0, 1}, per<100>}>;
    static_assert(conversion_rounds<unit>(1e-20) && !conversion_overflows<unit>(1e-20));
    static_assert(conversion_overflows<unit>(-1e-20) && !conversion_rounds<unit>(-1e-20));
    static_assert(conversion_rounds<inside<{{0, 1}, per<100>}, round_nearest>>(-1e-20));
}

TEST(ConversionTest, no_comparison_with_floating_point) {
    using C      = inside<grid{interval{0, 1}, max_denominator{1000}}>;
    using thirds = inside<{{0, 1}, rational{1, 3}}>;
    static_assert(!compares_with<C, double> && !compares_with<thirds, float> && !compares_with<C, long double>);
    static_assert(compares_with<C, int> && compares_with<C, rational>);
    static_assert(C{rational{1, 2}} == 0.5_r && thirds{rational{1, 3}} == just<1, 3>);
}

// to<T, round_toward_zero>() truncates first, then range-checks: 127.5 →
// int8 is 127, −0.5 → unsigned is 0. Without a rounding a fractional value
// is a rounding_error.
TEST(ConversionTest, to_rounds_before_the_range_check) {
    using H = inside<{{-0.5, 200}, 0.5}>;
    static_assert(*H{-0.5}.to<unsigned, round_toward_zero>() == 0u);
    static_assert(*H{127.5}.to<std::int8_t, round_toward_zero>() == 127);
    static_assert(H{127.5}.to<std::int8_t>().error() == errc::rounding_error);
    static_assert(H{127.5}.to<int, round_nearest>() == 128 && H{127.5}.to<int, round_floor>() == 127);
    static_assert(inside<{{0, 10}, per<4>}, round_nearest>{2.75}.to<int>() == 3); // the type's own mode
    static_assert(H{128}.to<std::int8_t>().error() == errc::overflow);
    static_assert(inside<{-5, 5}>{-1}.to<unsigned>().error() == errc::domain_error);
}

// A bounded continuous grid rejects a denominator past its bound: so does
// conversion_rounds (it said false, and parse_exact relied on it).
TEST(ConversionTest, conversion_rounds_on_a_bounded_continuous_grid) {
    using B = inside<grid{interval{0, 1}, max_denominator{2}}>;
    static_assert(conversion_rounds<B>(rational{1, 3}) && !conversion_rounds<B>(rational{1, 2}));
    static_assert(!conversion_rounds<B>(0.5) && conversion_rounds<B>(0.25));
    static_assert(!conversion_rounds<B>(rational{3}) && conversion_overflows<B>(rational{3}));
    EXPECT_FALSE(parse_exact<B>(std::string_view{"1/3"}).has_value());
}

TEST(ConversionTest, casts_and_math_are_constraint_friendly) {
    using three  = inside<{3}>;              // a point grid
    using halves = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
    using digit  = inside<{0, 9}>;
    using small  = inside<{-7, 5}>;
    static_assert(can_clamp_cast<digit, int> && can_clamp_cast<digit, small>);
    static_assert(!can_clamp_cast<halves, three> && can_checked_cast<halves, three>); // 3 is off the .5 lattice
    static_assert(can_abs_into<digit, small> && !can_abs_into<inside<{1, 10}>, small>);
    static_assert(can_pown<5, digit> && can_pown<0, inside<{{-2, 2}, max_denominator{1000}}>>);
    EXPECT_EQ(math::pown<3>(small{-2}), -8_ins);
    EXPECT_EQ((digit{small{-3}, clamp}), 0_ins);
}

// conversion_rounds divided loudly (a compile error for a quotient past 64
// bits) and guessed for doubles past the 64-bit rationals.
TEST(ConversionTest, predicates_decide_exactly) {
    using F = inside<{{0, 1}, rational{1, 1ll << 61}}>;
    using C = inside<{{0, 2}, rational{1, 1ll << 61}}, round_nearest>;
    using N = inside<{{0, 10}, rational{1, 3}}, round_nearest>;
    EXPECT_FALSE(conversion_rounds<F>(0x1p-61));
    EXPECT_TRUE(conversion_rounds<F>(0x1p-62));
    EXPECT_TRUE(conversion_overflows<N>(1e30));
    EXPECT_FALSE(conversion_rounds<N>(1e30));
    EXPECT_FALSE(conversion_overflows<N>(1e-300));
    EXPECT_TRUE(conversion_rounds<N>(1e-300));
    EXPECT_TRUE(conversion_rounds<N>(-1e-300)); // rounds onto 0
    static_assert(conversion_rounds<C>(rational{1, (1ll << 62) + 1}));
    static_assert(!conversion_rounds<C>(rational{3, 1ll << 61}));
    // A bounded continuous grid rounds before its range check: 22/21 rounds
    // onto 1 in sevenths (rounding, not overflow), as the exact path decides.
    using S = inside<grid{interval{0, 1}, max_denominator{7}}, round_nearest>;
    static_assert(conversion_rounds<S>(rational{22, 21}) && !conversion_overflows<S>(rational{22, 21}));
    static_assert(!conversion_rounds<S>(rational{3, 2}) && conversion_overflows<S>(rational{3, 2}));
}

// A double finer than a continuous grid's bound is off the grid; the
// predicates agree with the store.
TEST(ConversionTest, continuous_predicates_see_the_rational_store) {
    using C        = inside<{{-1, 1}, max_denominator{imax{1} << 62}}>;
    const double d = 0x1.644cb4ec9ec46p-20;
    EXPECT_EQ(C::try_make(d).error(), errc::rounding_error);
    EXPECT_FALSE(conversion_overflows<C>(d));
    EXPECT_TRUE(conversion_loses<C>(d));
    EXPECT_FALSE(conversion_overflows<C>(0.25));
}

// numerator() returned imax whenever the values fit it, and wrapped a
// reduced numerator past it. It is an inside on the numerators' grid now.
TEST(ConversionTest, numerator_past_int64) {
    using I64             = std::int64_t;
    constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
    using detail::big_int, detail::grid_rational;
    using X      = inside<grid{{I64{1} << 62, (I64{1} << 62) + 1}, rational{1, 2}}>;
    const X    x = rational{(std::uint64_t{1} << 63) + 1, 2};
    const auto n = x.numerator();
    EXPECT_TRUE(n == (std::uint64_t{1} << 63) + 1);
    using Z      = inside<grid{{-I64_MAX, -I64_MAX + 1}, rational{1, 2}}>;
    const Z    z = rational{UINT64_MAX - 2, -2};
    const auto m = z.numerator();
    EXPECT_TRUE(m == -just<grid_rational{big_int{UINT64_MAX - 2}}>);
    // Numerators and denominators get the tightest integer grids.
    static_assert(grid_of<decltype(std::declval<inside<{{0, 10}, rational{1, 4}}>>().numerator())> == grid{0, 40});
    static_assert(grid_of<decltype(std::declval<inside<{{0, 1}, max_denominator{1000}}>>().numerator())> ==
                  grid{0, 1000});
    static_assert(grid_of<decltype(std::declval<inside<{{0, 1}, max_denominator{1000}}>>().denominator())> ==
                  grid{1, 1000});
}

// The one-shot casts OR-ed their flags with the type's, so the type's
// rounding mode or clamp won.
TEST(ConversionTest, casts_override_the_declared_policy) {
    EXPECT_EQ((inside<{0, 100}, round_floor>{2.5, clamp | round_nearest}), 3);
    EXPECT_EQ((inside<{0, 100}, round_half_even>{2.5, clamp | round_nearest}), 3);
    EXPECT_EQ((inside<{0, 100}, round_floor>{2.5, clamp | round_ceil}), 3);
    EXPECT_EQ((inside<{0, 100}, round_ceil>{2.2, clamp | round_nearest}), 2);
    EXPECT_EQ((inside<{0, 100}, clamp>{150, wrap}), 49);
    EXPECT_EQ((inside<{0, 100}, wrap>{150, clamp}), 100);
    EXPECT_EQ((inside<{0, 100}, round_floor>{7.9, clamp}), 7); // the type's rounding stays
}

TEST(ConversionTest, inside_against_long_double) {
    using Q = inside<{{-100, 100}, per<4>}>;
    static_assert(!compares_with_fp<Q, long double> && !compares_with_fp<Q, double>);
}

// to<long double>() went through a double: 1 + 2^-60 came back as 1. It is
// now the long double nearest the exact value, rounded once.
TEST(ConversionTest, to_long_double_is_rounded_once) {
    using I64 = std::int64_t;
#if WIDE_LONG_DOUBLE
    using F = inside<{{-2, 2}, per<(I64{1} << 62)>}>;
    using T = inside<{{-1, 1}, per<3>}>;
    static_assert(F{(rational{1} + rational{1, I64{1} << 60})}.to<long double>() == 1.0L + 0x1p-60L);
    static_assert(T{rational{1, 3}}.to<long double>() == 1.0L / 3);
    const F f{(rational{-1} - rational{1, I64{1} << 62})};
    EXPECT_EQ(f.to<long double>(), -1.0L - 0x1p-62L);
    using Q = inside<{{-1000000, 1000000}, per<1000000007>}>;
    const Q q{rational{1, 1000000007}};
    EXPECT_EQ(q.to<long double>(), 1.0L / 1000000007);
    using S = inside<{{-9223372036854775807, 9223372036854775807}, 1}>;
    EXPECT_EQ(S{test::at_run_time(I64{9223372036854775807})}.to<long double>(), 9223372036854775807.0L);
#endif
}

// to<float>() rounded through a double: 1 + 2^-24 + 2^-60 rounded to the
// double 1 + 2^-24, a tie the float broke to even (1), not up.
TEST(ConversionTest, to_float_rounds_once) {
    using T     = inside<{{0, 2}, per<(1ll << 60)>}>;
    const T x   = T::from_raw(test::at_run_time((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) + 1));
    const T low = T::from_raw(test::at_run_time((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) - 1));
    const T tie = T::from_raw(test::at_run_time((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36)));
    EXPECT_EQ(x.to<float>(), 1.0f + 0x1p-23f);
    EXPECT_EQ(low.to<float>(), 1.0f);
    EXPECT_EQ(tie.to<float>(), 1.0f); // a true tie: to even
    static_assert(T::from_raw((std::uint64_t{1} << 60) + (std::uint64_t{1} << 36) + 1).to<float>() == 1.0f + 0x1p-23f);
}

// The noexcept conversion predicates classify non-finite input instead of raising.
TEST(ConversionTest, predicates_handle_non_finite_input) {
    using namespace beman::inside;
    using B          = inside<{0, 10}>;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(conversion_overflows<B>(nan));
    EXPECT_TRUE(conversion_overflows<B>(-inf));
    EXPECT_FALSE(conversion_rounds<B>(nan));
    EXPECT_TRUE(conversion_loses<B>(inf));
    EXPECT_FALSE(conversion_loses<B>(3.0));
    EXPECT_TRUE(conversion_rounds<B>(3.5));
}

//---------------------------------------------------------------------------
// checked_cast (casts.hpp) — the success return (was static_assert only)
//---------------------------------------------------------------------------
// checked_cast returns the value on the in-range happy path (runtime)
TEST(ConversionTest, checked_cast_in_range_at_run_time) {
    using pct = inside<{0, 100}>;
    pct p     = checked_cast<pct>(42); // runtime call, not static_assert
    ASSERT_EQ(p, 42);

    using coarse = inside<{{0, 10}, 2}>;
    ASSERT_TRUE(checked_cast<coarse>(4) == coarse{4}); // on-notch, in-range
}

//---------------------------------------------------------------------------
// conversion_rounds (predicates.hpp) — false out of range
//---------------------------------------------------------------------------
// conversion_rounds is false for out-of-range values (runtime)
TEST(ConversionTest, conversion_rounds_is_false_out_of_range) {
    using coarse = inside<{{0, 10}, 2}>;
    // Out of range is overflow, not truncation: the predicate is false.
    ASSERT_FALSE(conversion_rounds<coarse>(11));
    ASSERT_FALSE(conversion_rounds<coarse>(-1));
    // Contrast: off-notch but in range is truncation.
    ASSERT_TRUE(conversion_rounds<coarse>(3));
}

//---------------------------------------------------------------------------
// inside -> inside store into a dyadic target.
//---------------------------------------------------------------------------
// inside -> dyadic conversion lands on the grid
TEST(ConversionTest, to_a_dyadic_grid_lands_on_the_grid) {
    using src_t = inside<{-2, 2}>;                             // integer-backed source
    using rb    = inside<{{-2, 2}, per<1024>}, round_nearest>; // dyadic target

    src_t src{1};
    rb    dst = src; // insidable -> dyadic store
    ASSERT_EQ(double(dst), 1.0);

    src_t neg{-2};
    rb    dn = neg;
    ASSERT_EQ(double(dn), -2.0);
}

TEST(ConversionTest, no_implicit_integer_conversion) {
    using idx = inside<{0, 9}>;
    static_assert(!std::is_convertible_v<idx, imax>);
    static_assert(!std::is_convertible_v<idx, int>);
    static_assert(!std::is_constructible_v<int, idx>);
    static_assert(!std::is_convertible_v<inside<{{0, 9}, per<2>}, round_nearest>, imax>);
    static_assert(!std::is_convertible_v<inside<{0, 10'000'000'000}>, int>); // would narrow silently
    static_assert(!casts_to_imax<idx>);
    static_assert(!indexes_a_vector<idx>);

    idx  a{5};
    imax val = a.as<imax>();
    ASSERT_EQ(val, 5);

    using idx2 = inside<{1, 10}>;
    idx2 b{7};
    ASSERT_EQ(b.as<int>(), 7);

    // an array subscript names the index type
    int arr[] = {10, 20, 30, 40, 50};
    using ai  = inside<{0, 4}>;
    ai c{3};
    ASSERT_EQ(arr[c.as<std::size_t>()], 40);
}

//---------------------------------------------------------------------------
// Case 3 — operator rational() fast path correctness
//---------------------------------------------------------------------------
// operator rational() round-trips through Q-format fast path
TEST(ConversionTest, rational_of_a_q_format_value) {
    using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;

    // Bit-for-bit exactness over a sampling of values.
    for (int v : {0, 1, 50, 127, 254, 255}) {
        fp       b{v};
        rational r = b;
        ASSERT_EQ(r, rational{static_cast<unsigned>(v)});
        // round-trip back to fp.value
        ASSERT_EQ(b, v);
    }
}

// operator rational() handles fractional Q-format value
TEST(ConversionTest, rational_of_a_fractional_q_format_value) {
    using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
    // Raw=128 → value 128/256 = 0.5.
    auto     b = fp::from_raw(128);
    rational r = b;
    ASSERT_EQ(r, 0.5_r);
}

//---------------------------------------------------------------------------
// numerator()/denominator() and to_value fast paths agree with the rational view.
//---------------------------------------------------------------------------
TEST(ConversionTest, fraction_and_to_value_fast_paths_match_rational) {
    using Q = inside<{-4, 4, just<1, 8>}>; // dyadic, negative Lower
    static_assert(index_storage<Q> && q_format_fits<Q>);
    for (imax k = 0; k <= 64; ++k) {
        Q              q = Q::from_raw(static_cast<Q::raw_type>(k));
        const rational r{q};
        const imax     num = static_cast<imax>((r.Den < 0) ? umax{0} - r.Num : r.Num);
        EXPECT_EQ(q.numerator(), num);
        EXPECT_EQ(q.denominator(), static_cast<imax>(abs_den(r.Den)));
        EXPECT_EQ(to_value(q), trunc(r));
    }
    using I = inside<{-30, 90, 3}>; // integer notch 3, index storage
    static_assert(index_storage<I> && integer_lattice<I>);
    for (imax v = -30; v <= 90; v += 3) {
        I i{v};
        EXPECT_EQ(to_value(i), v);
        EXPECT_EQ(i.numerator(), v);
        EXPECT_EQ(i.denominator(), 1);
        I j;
        from_value(j, v);
        EXPECT_EQ(j.raw(), i.raw());
    }
}

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// The exact paths of the conversion predicates: a double past the 64-bit
// rationals, a long double that is no double, grid numbers past 64 bits.
TEST(ConversionsArmTest, predicates_on_the_exact_paths) {
    using I = inside<{0, 10}>;
    EXPECT_TRUE(conversion_overflows<I>(1e30) && !conversion_rounds<I>(1e30));
    EXPECT_TRUE(conversion_overflows<I>(1e30L));
    // Just past Upper, onto the bound by the grid's rounding: rounds.
    using C = inside<{{0, 1}, max_denominator{10}}, round_nearest>;
    EXPECT_TRUE(!conversion_overflows<C>(1.0L + 0x1p-63L) && conversion_rounds<C>(1.0L + 0x1p-63L));
    using P = inside<{1180591620717411303424_g, 1180591620717411303424_g}>; // the point 2^70
    EXPECT_TRUE(!conversion_overflows<P>(0x1p70) && !conversion_rounds<P>(0x1p70));
    using W = inside<{0, 1180591620717411303424_g}>;
    EXPECT_TRUE(!conversion_overflows<W>(5) && !conversion_rounds<W>(5));
}

// A point rounds nothing; on a fine lattice an in-range value whose 64-bit
// steps overflow is decided exactly.
TEST(ConversionsArmTest, rounds_on_a_point_and_past_the_64_bit_steps) {
    EXPECT_FALSE((conversion_rounds<inside<{5, 5}>>(5)));
    EXPECT_FALSE((conversion_rounds<inside<{5, 5}>>(rational{5})));
    EXPECT_TRUE((conversion_rounds<inside<{{0, 4}, per<3>}>>(rational{(umax{1} << 63) + 1, imax{1} << 62})));
}

// A lattice past the 64-bit rationals converts the values that fit; one
// past them is reported.
TEST(ConversionsArmTest, rational_of_a_value_past_64_bits) {
    using T = inside<{{-12000000000000000000_g, 12000000000000000000_g}, just<3, 2>}>;
    static_assert(detail::rational_readable<T>);
    const T t = T::from_index(15333333333333333333_ins); // 10999999999999999999.5
    EXPECT_THROW((void)rational{t}, inside_error);
}
} // namespace reached_arms_tests

namespace pinned_tests {
using namespace beman::inside;

// A continuous grid whose bound passes 64 bits (small limits, a fraction raw)
// takes the predicates' exact path: the 64-bit one read the bound as a 64-bit
// rational and raised inside the noexcept predicates (terminate), also under
// parse_exact.
TEST(ConversionsPinnedTest, predicates_on_a_bound_past_64_bits) {
    using F = inside<{{0, 1}, max_denominator{1180591620717411303424_g}}>; // bound 2^70
    static_assert(detail::fraction_storage<F>);
    EXPECT_FALSE(conversion_rounds<F>(test::at_run_time(0)));
    EXPECT_TRUE(conversion_overflows<F>(test::at_run_time(2)));
    EXPECT_FALSE(conversion_rounds<F>(test::at_run_time(0.5)));
    EXPECT_TRUE(parse_exact<F>("0").has_value());
    EXPECT_TRUE(parse_exact<F>("0.5").has_value());
}
} // namespace pinned_tests

namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// to<long double>() and to<float>() of exact values past the type's range
// are ±infinity, below its least subnormal 0; zero is 0.
TEST(ConversionsArmTest, floats_of_values_past_the_range) {
    using Big = inside<{{0, detail::grid_rational{detail::grid_int{1} << 20000}},
                        detail::grid_rational{detail::grid_int{1} << 19990}}>;
    EXPECT_EQ(Big::from_index(1024_ins).to<long double>(), std::numeric_limits<long double>::infinity());
    EXPECT_EQ(Big::from_index(1024_ins).to<float>(), std::numeric_limits<float>::infinity());
    using F = inside<{0, 340282366920938463463374607431768211456_g}>; // to 2^128
    EXPECT_EQ(F{just<340282366920938463463374607431768211456_g>}.to<float>(), std::numeric_limits<float>::infinity());
    using T = inside<{{0, 1}, max_denominator{detail::grid_rational{detail::grid_int{1} << 20000}}}>;
    EXPECT_EQ((*parse_exact<T>("0")).to<long double>(), 0.0L);
    EXPECT_EQ((*parse_exact<T>("0x1p-19999")).to<long double>(), 0.0L);
}

// rational{long double}: |v| ≥ 2^64 raises; finer than 2^-62 is truncated.
TEST(ConversionsArmTest, rational_of_a_long_double) {
    EXPECT_THROW((void)rational{test::at_run_time(1e30L)}, inside_error);
    EXPECT_EQ(rational{test::at_run_time(1e-30L)}, 0);
    EXPECT_EQ(rational{test::at_run_time(1.0L + 0x1p-63L)}, 1);
    EXPECT_EQ(rational{test::at_run_time(0.0)}, 0);
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// A notch-0 point (inside<grid{5}>; {5, 5} derives notch 1) rounds nothing,
// on the 64-bit path and the exact one; an integer compares on the left.
TEST(ConversionsArmTest, notch_zero_point_and_reversed_comparison) {
    using P = inside<grid{5}>;
    static_assert(grid_of<P>.Notch == 0);
    EXPECT_FALSE(conversion_rounds<P>(5));
    EXPECT_FALSE(conversion_rounds<P>(inside<{0, 1180591620717411303424_g}>{5}));
    EXPECT_TRUE((5 < rational{test::at_run_time(11), 2}));
}
} // namespace reached_arms_tests

namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// A rational against a long double finer than a double.
TEST(ConversionsArmTest, rational_against_a_long_double) {
    const long double up = test::at_run_time(1.0L + 0x1p-63L);
    EXPECT_TRUE(rational{1} < up);
    EXPECT_TRUE(rational{-1} > -up);
    EXPECT_FALSE(rational{2} < up);
}
} // namespace reached_arms_tests
