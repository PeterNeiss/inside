// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids: construction and validation (grid, interval, bounds),
// the grid operators, and the literals _r, _g and _ins.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "support.hpp"

// grid
namespace grid_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// grid structured binding
TEST(GridTest, grid_structured_binding) {

    grid g{{0, 100}, 2};
    auto [iv, notch, max_den] = g;
    ASSERT_EQ(iv.Lower, 0);
    ASSERT_EQ(iv.Upper, 100);
    ASSERT_EQ(notch, 2);
    ASSERT_EQ(max_den, 0); // a lattice bounds its denominators itself
}

// grid construction and step_count
TEST(GridTest, grid_construction_and_max_notch) {
    static_assert(grid{{0, 100}, 1}.step_count() == detail::grid_int{100});
    static_assert(grid{{1, 5}, 0.25}.step_count() == detail::grid_int{16});
    static_assert(grid{{0, std::numeric_limits<umax>::max()}, 1}.step_count() ==
                  detail::grid_int{std::numeric_limits<umax>::max()});
}

// just<>/inside values work as grid corners
TEST(GridTest, just_inside_values_work_as_grid_corners) {
    // Corners are `rational`, so an inside converts via its implicit operator rational().
    static_assert(grid{0, just<30>} == grid{0, 30});
    static_assert(grid{just<5>, just<10>} == grid{5, 10});
    static_assert(grid{just<2>} == grid{2}); // 1-arg point grid
    static_assert(grid{just<0>, just<8>, just<2>} == grid{0, 8, 2});
    // ...and as inside<> grid-spec corners (the rifle.cpp use case).
    static_assert(grid_of<inside<{0, just<30>}, wrap>> == grid_of<inside<{0, 30}, wrap>>);
}

// grid raw_for selection
TEST(GridTest, grid_storage_min_selection) {
    // Notch 0 with a bound that fits 64 bits -> rational
    static_assert(std::is_same_v<raw_for_t<grid{{0, 10}, max_denominator{1000}}>, rational>);

    // Notch 1, lower 0 -> smallest unsigned that fits Upper
    static_assert(std::is_same_v<raw_for_t<grid{0, 100, 1}>, std::uint8_t>);
    static_assert(std::is_same_v<raw_for_t<grid{0, 1000, 1}>, std::uint16_t>);
    static_assert(std::is_same_v<raw_for_t<grid{0, 100000, 1}>, std::uint32_t>);

    // Signed integer case (notch 1, negative lower)
    static_assert(std::is_same_v<raw_for_t<grid{-127, 127, 1}>, std::int8_t>);
    static_assert(std::is_same_v<raw_for_t<grid{-32000, 32000, 1}>, std::int16_t>);

    // Lower above 0: the value when it is no wider than the index, else the index
    static_assert(deduces_value<grid{5, 100, 1}> && std::is_same_v<raw_for_t<grid{5, 100, 1}>, std::uint8_t>);
    static_assert(deduces_value<grid{1024, 65535, 1}>);
    static_assert(std::is_same_v<raw_for_t<grid{1024, 65535, 1}>, std::uint16_t>);
    static_assert(!deduces_value<grid{200, 300, 1}> && std::is_same_v<raw_for_t<grid{200, 300, 1}>, std::uint8_t>);
    // a notched or offset lattice keeps the index
    static_assert(!deduces_value<grid{{0, 1}, rational{1, 256}}>);
    static_assert(!deduces_value<grid{{0.5_r, 10.5_r}, 1}>);
}

// grid arithmetic
TEST(GridTest, grid_arithmetic) {
    {
        SCOPED_TRACE("add aligns notches via gcd");
        constexpr grid a{{0, 10}, 1};
        constexpr grid b{{0, 5}, 1};
        constexpr auto r = a + b;
        ASSERT_TRUE(r.Interval.Lower == 0);
        ASSERT_TRUE(r.Interval.Upper == 15);
        ASSERT_TRUE(r.Notch == 1);
    }

    {
        SCOPED_TRACE("multiply takes bounding box and lcm-ish notch");
        constexpr grid a{{0, 10}, 1};
        constexpr grid b{{0, 5}, 1};
        constexpr auto r = a * b;
        ASSERT_TRUE(r.Interval.Lower == 0);
        ASSERT_TRUE(r.Interval.Upper == 50);
    }

    {
        SCOPED_TRACE("divide by zero-only interval yields division_by_zero");
        constexpr grid a{{0, 10}, 1};
        constexpr grid zero{{0, 0}, 0}; // pure zero-point grid
        constexpr auto r = a / zero;
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::division_by_zero);
    }

    {
        SCOPED_TRACE("divide by positive interval excluding zero");
        constexpr grid a{{0, 10}, 1};
        constexpr grid b{{2, 5}, 1};
        constexpr auto r = a / b;
        ASSERT_TRUE(r.has_value());
    }

    {
        SCOPED_TRACE("divide by interval [0, U] excludes zero from divisor by stepping in by notch");
        constexpr grid a{{0, 10}, 1};
        constexpr grid b{{0, 5}, 1}; // includes zero, positive upper
        constexpr auto r = a / b;
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(r->Interval.Lower == 0);
        ASSERT_TRUE(r->Interval.Upper == 10); // 10 / 1 = 10
        ASSERT_TRUE(r->Notch == 0);
    }

    {
        SCOPED_TRACE("divide by interval [L, 0] (negative side only)");
        constexpr grid a{{0, 10}, 1};
        constexpr grid b{{-5, 0}, 1}; // includes zero, negative lower
        constexpr auto r = a / b;
        ASSERT_TRUE(r.has_value());
        // dividing positives by negatives gives non-positive result
        ASSERT_TRUE(r->Interval.Upper <= 0);
        ASSERT_TRUE(r->Notch == 0);
    }

    {
        SCOPED_TRACE("divide by interval that straddles zero");
        constexpr grid a{{1, 10}, 1};
        constexpr grid b{{-5, 5}, 1}; // straddles zero
        constexpr auto r = a / b;
        ASSERT_TRUE(r.has_value());
        // result must span both signs once we exclude zero
        ASSERT_TRUE(r->Interval.Lower < 0);
        ASSERT_TRUE(r->Interval.Upper > 0);
        ASSERT_TRUE(r->Notch == 0);
    }

    {
        SCOPED_TRACE("divide by zero-notch interval that straddles zero");
        constexpr grid a{{1, 10}, 1};
        constexpr grid b{{-5, 5}, 0}; // unbounded: its values come arbitrarily close to 0
        static_assert((a / b).error() == errc::overflow);
        constexpr grid c{{-5, 5}, max_denominator{4}}; // nonzero values are at least 1/4 in magnitude
        constexpr auto r = a / c;
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(r->Interval.Lower == -40);
        ASSERT_TRUE(r->Interval.Upper == 40);
    }
}

// grid validate
TEST(GridTest, grid_validate) {
    static_assert(grid::validate<grid{{0, 10}, 1}>());
    static_assert(grid::validate<grid{{0, 10}, rational{1u, 2}}>());
    static_assert(grid::validate<grid{0_r}>()); // point grid, notch=0
}

// grid{lo, hi} derives the notch as gcd(1, Lower, Upper): the coarsest step 1/k
// keeping every integer and both limits on the lattice.
TEST(GridTest, two_limit_ctor_derives_the_notch) {
    static_assert(per<16> == rational{1, 16}); // per<D> is the step 1/D
    static_assert(just<-6, 5> == rational{-6, 5});
    static_assert(grid{0, 100}.Notch == 1); // integers: unchanged
    static_assert(grid{-40, 85}.Notch == 1);
    static_assert(grid{0.0, 1.0}.Notch == 1);
    static_assert(grid{0.5, 10}.Notch == rational{1, 2});
    static_assert(grid{-2.5, 0.75}.Notch == rational{1, 4});
    static_assert(grid{just<-6, 5>, just<3, 5>}.Notch == rational{1, 5});
    static_assert(grid{0.1_r, 1}.Notch == rational{1, 10});
    static_assert(grid{0, 0x1p-10}.Notch == rational{1, 1024});   // at the double bound
    static_assert(grid{0, 0x1p-11_r}.Notch == rational{1, 2048}); // exact spelling: no bound
    // Big grid numbers: the exact notch 1/(2^62·(2^62−1)), past 64 bits.
    static_assert(
        grid{just<1, (1ll << 62)>, just<1, (1ll << 62) - 1>}.Notch ==
        detail::grid_rational{detail::big_int{1}, detail::big_int{1ll << 62} * detail::big_int{(1ll << 62) - 1}});
    static_assert(grid::validate<grid{just<-6, 5>, just<3, 5>}>());
    // _g: grid numbers from decimal literals, exactly, of any size.
    static_assert(grid{0, 2.5_g}.Notch == rational{1, 2});
    static_assert(1'000_g == 1000 && 25e-2_g == rational{1, 4} && 1.5e3_g == 1500);

    using half = inside<{0.5, 10}>;
    static_assert(std::is_same_v<half::raw_type, std::uint8_t>);
    static_assert(max_index_of<half> == 19);
    EXPECT_EQ(rational{half{2.5}}, (rational{5, 2}));
}

namespace {
// The derived notch 1/k must keep 1, Lower and Upper on the lattice, and be the
// coarsest such step. A coarser 1/m (m a proper divisor of k) works only if
// 1/(k/p) works for some prime p | k, so checking those suffices.
template <grid G>
constexpr bool derived_notch_is_coarsest() {
    const rational n = G.Notch;
    if (n.Num != 1)
        return false; // always 1/k
    auto on = [](rational v, rational step) { return divides_evenly(v, step); };
    if (!on(1, n) || !on(G.Interval.Lower, n) || !on(G.Interval.Upper, n))
        return false;
    const umax k    = abs_den(n.Den);
    umax       rest = k;
    for (umax p = 2; p * p <= rest || rest > 1; ++p) {
        if (p * p > rest)
            p = rest; // rest is prime
        if (rest % p != 0)
            continue;
        while (rest % p == 0)
            rest /= p;
        const rational coarser{umax{1}, static_cast<imax>(k / p)};
        if (on(G.Interval.Lower, coarser) && on(G.Interval.Upper, coarser))
            return false; // a coarser step works
    }
    return true;
}

template <grid G>
constexpr bool max_index_matches() {
    const rational span = (detail::to_rational(G.Interval.Upper) - detail::to_rational(G.Interval.Lower)).value();
    return rational{detail::max_index(G)} == (span / detail::to_rational(G.Notch)).value();
}
} // namespace

// grid{lo, hi} with fractional limits: the notch is 1/lcm of the limits'
// denominators (after reduction), the grid validates, and inside works on it.
TEST(GridTest, derived_notch_of_fraction_pairs) {
    // {13/16, 3/10} in valid order: lcm(16, 10) = 80 → slots 24/80 .. 65/80.
    constexpr grid a{just<3, 10>, just<13, 16>};
    static_assert(a.Notch == rational{1, 80});
    static_assert(a.step_count() == detail::grid_int{41});
    static_assert(grid::validate<a>() && derived_notch_is_coarsest<a>() && max_index_matches<a>());

    // Reversed order is not an interval: the notch still derives, the grid is invalid.
    static_assert(grid{just<13, 16>, just<3, 10>}.Notch == rational{1, 80});
    // (as a grid it is a compile error: tests/fail/interval_reversed.test.cpp)

    // Shared factors reduce first: 6/8 is 3/4, 10/12 is 5/6 → lcm(4, 6) = 12.
    constexpr grid b{just<6, 8>, just<10, 12>};
    static_assert(b.Notch == rational{1, 12});
    static_assert(derived_notch_is_coarsest<b>() && max_index_matches<b>());

    // Mixed signs, coprime denominators: lcm(7, 9) = 63.
    constexpr grid c{just<-5, 7>, just<4, 9>};
    static_assert(c.Notch == rational{1, 63});
    static_assert(c.step_count() == detail::grid_int{73}); // (4/9 + 5/7)·63 = 28 + 45
    static_assert(grid::validate<c>() && derived_notch_is_coarsest<c>() && max_index_matches<c>());

    // Both negative.
    constexpr grid d{just<-13, 16>, just<-3, 10>};
    static_assert(d.Notch == rational{1, 80});
    static_assert(derived_notch_is_coarsest<d>() && max_index_matches<d>());

    // Integer and fraction: the integer contributes nothing finer.
    constexpr grid e{-3, just<7, 3>};
    static_assert(e.Notch == rational{1, 3});
    static_assert(e.step_count() == detail::grid_int{16});
    static_assert(derived_notch_is_coarsest<e>() && max_index_matches<e>());

    // One denominator divides the other: lcm(4, 12) = 12.
    constexpr grid f{just<1, 4>, just<11, 12>};
    static_assert(f.Notch == rational{1, 12});
    static_assert(derived_notch_is_coarsest<f>() && max_index_matches<f>());

    // A whole number written as a fraction stays an integer grid.
    constexpr grid g{just<6, 3>, just<20, 4>};
    static_assert(g.Notch == 1);
    static_assert(g.Interval.Lower == 2 && g.Interval.Upper == 5);

    // Equal limits: a point; the two-limit form still derives a valid notch.
    constexpr grid h{just<3, 10>, just<3, 10>};
    static_assert(h.Notch == rational{1, 10});
    static_assert(grid::validate<h>() && h.step_count() == detail::grid_int{0});

    // Large coprime denominators that still fit: lcm(1009, 1013) = 1022117.
    constexpr grid i{just<1, 1013>, just<1, 1009>};
    static_assert(i.Notch == rational{1, 1022117});
    static_assert(i.step_count() == detail::grid_int{4}); // 1013 − 1009
    static_assert(grid::validate<i>() && derived_notch_is_coarsest<i>() && max_index_matches<i>());

    // inside on derived fractional grids: storage, exact values, assignment.
    using eightieths = inside<{just<3, 10>, just<13, 16>}>;
    static_assert(std::is_same_v<eightieths::raw_type, std::uint8_t>);
    EXPECT_EQ(rational{eightieths{0.5}}, (rational{1, 2})); // 40/80
    EXPECT_EQ((rational{eightieths{just<13, 16>}}), (rational{13, 16}));
    EXPECT_EQ(eightieths::try_make(rational{just<1, 3>}).error(), errc::rounding_error); // 1/3 is not k/80

    using signed_63 = inside<{just<-5, 7>, just<4, 9>}, round_nearest>;
    EXPECT_EQ((rational{signed_63{just<-5, 7>}}), (rational{-5, 7}));
    EXPECT_EQ(rational{signed_63{0.0}}, rational{0});
    EXPECT_EQ((rational{signed_63{just<1, 4>}}), (rational{16, 63})); // 1/4 = 15.75/63 → nearest 16/63
}

// bounds: an open limit is a spelling, moved in by one notch; the grid is
// stored closed, so both spellings are the same type.
static_assert(grid{0, 10, bounds::closed} == grid{0, 10});
static_assert(std::is_same_v<inside<{0, 360, bounds::half_open}>, inside<{0, 359}>>);
static_assert(std::is_same_v<inside<{{0, 64}, per<16>, bounds::half_open}>, inside<{{0, 63.9375}, per<16>}>>);
static_assert(grid{0, 10, bounds::open} == grid{1, 9});
static_assert(grid{0.5, 10, bounds::half_open} == grid{{0.5, 9.5}, 0.5}); // derived notch 1/2
static_assert(grid{-8, -2, 2, bounds::open} == grid{{-6, -4}, 2});
static_assert(grid{{0.5, 10.5}, 1, bounds::half_open} == grid{{0.5, 9.5}, 1}); // unanchored lattice
static_assert(grid{0, 2, bounds::open} == grid{{1, 1}, 1});                    // one point left

TEST(GridTest, half_open_wrap_has_the_written_period) {
    inside<{0, 360, bounds::half_open}, wrap> deg{0};
    deg = 370;
    EXPECT_EQ(deg, 10);
    deg = 360;
    EXPECT_EQ(deg, 0);
    inside<{{0, 64}, per<16>, bounds::half_open}, wrap> x{0};
    x = 66;
    EXPECT_EQ(x, 2);
}

//---------------------------------------------------------------------------
// Denominator bounds — a continuous grid that an exact op produces knows how
// large its values' denominators can get.
//---------------------------------------------------------------------------
namespace {
constexpr grid hundred{0, 100}, one_to_seven{1, 7};
constexpr grid thousandths{interval{0, 1}, max_denominator{1000}};
constexpr grid unbounded{{0, 10}, 0};
} // namespace

TEST(GridTest, quotients_and_their_results_bound_denominators) {
    // x/y = p·b/(a·r): den ≤ den_bound(x)·num_bound(y) = 1·7
    constexpr grid q = *(hundred / one_to_seven);
    static_assert(q.Notch == 0 && q.MaxDenominator == 7);
    static_assert((q + grid{32}).MaxDenominator == 7);             // a point adds den 1
    static_assert((q * q).MaxDenominator == 49);                   // products multiply
    static_assert((q - q).MaxDenominator == 49);                   // sums too
    static_assert((-q).MaxDenominator == 7);                       // negation keeps it
    static_assert((*(hundred / grid{-7, 7})).MaxDenominator == 7); // a divisor spanning 0 too
    static_assert((q + unbounded).MaxDenominator == 0);            // unbounded stays unbounded
    static_assert((*(hundred / grid{{1, 10}, 0})).MaxDenominator == 0);
    // an unbounded divisor reaching 0 comes arbitrarily close to it: no bounded quotient
    static_assert((hundred / unbounded).error() == errc::overflow);
    // hull: the larger bound; a lattice bounds by its value unit's denominator
    static_assert(hull(thousandths, grid{{0, 2}, rational{1, 4}}).MaxDenominator == 1000);
    static_assert(hull(thousandths, unbounded).MaxDenominator == 0);
    // a quotient that is a point, or lands on a lattice, carries no bound
    static_assert((*(grid{3} / grid{4})).MaxDenominator == 0);
    static_assert((*(hundred / grid{4})).MaxDenominator == 0);
    static_assert(grid::validate<q>());
}

TEST(GridTest, max_denominator_spelling_and_representable) {
    static_assert(thousandths.MaxDenominator == 1000 && thousandths.Notch == 0);
    static_assert(thousandths.representable(rational{1, 1000}));
    static_assert(!thousandths.representable(rational{1, 1001}));
    static_assert(unbounded.representable(rational{1, 1001}));
    static_assert(thousandths != grid{{0, 1}, 0}); // a bound is part of the type
    static_assert(den_bound(thousandths) == grid_int{1000});
    static_assert(den_bound(grid{{0.5_r, 10.5_r}, 1}) == grid_int{2});
    static_assert(num_bound(thousandths) == grid_int{1000});
}

TEST(GridTest, bounds_past_their_storage_are_dropped) {
    constexpr grid billionths{interval{0, 1}, max_denominator{1'000'000'000}};
    constexpr grid b2 = billionths * billionths;
    static_assert(b2.MaxDenominator == 1'000'000'000'000'000'000);
    // Kept until num + den need more than detail::max_fraction_bits
    constexpr grid b4 = b2 * b2, b8 = b4 * b4, b16 = b8 * b8, b32 = b16 * b16;
    static_assert(b16.MaxDenominator != 0 && b32.MaxDenominator == 0); // 10^144 kept, 10^288 dropped
}

TEST(GridTest, bounded_storage_holds_every_value) {
    // Within the 64-bit rational a bounded grid stores a rational, as before.
    static_assert(std::same_as<raw_for_t<thousandths>, rational>);
    static_assert(std::same_as<raw_for_t<*(hundred / one_to_seven)>, rational>);
    // Past it, a fraction sized from the bounds — here 10^36 needs 120 bits.
    constexpr grid billionths{interval{0, 1}, max_denominator{1'000'000'000}};
    constexpr grid b2 = billionths * billionths, b4 = b2 * b2;
    static_assert(std::same_as<raw_for_t<b4>, wide_rational<2>>);
}
} // namespace grid_tests

// grid construction (the rejected grids are compile errors: tests/fail)
namespace grid_construction_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

TEST(GridConstructionTest, well_formed_grids) {
    static_assert(grid{interval{0, 100}, 1_r}.Interval == interval{0, 100} && grid{interval{0, 100}, 1_r}.Notch == 1);
    static_assert(grid{interval{0, 0}, 0_r}.Notch == 0); // a point
    static_assert(grid{interval{0, 1}, max_denominator{1000}}.Notch == 0 &&
                  grid{interval{0, 1}, max_denominator{1000}}.MaxDenominator == 1000);
    static_assert(grid::validate<grid{interval{0, 255}, 0x1p-8_r}>()); // Q8.8
}

// A Lower off the notch's multiples is fine: the lattice is 1, 3, …, 11.
TEST(GridConstructionTest, an_unanchored_lattice) {
    constexpr grid g{interval{1, 11}, 2_r};
    static_assert(grid::validate<g>() && !g.anchored());
    static_assert(g.step_count() == detail::grid_int{5});
}

// An open limit moves in by one notch.
TEST(GridConstructionTest, bounds_move_open_limits_in) {
    static_assert(grid{interval{0, 360}, 1_r, bounds::half_open} == grid{0, 359});
    static_assert(grid{interval{0, 10}, 1_r, bounds::open} == grid{1, 9});
    static_assert(grid{interval{0, 10}, 1_r, bounds::closed} == grid{0, 10});
}
} // namespace grid_construction_tests

// interval
namespace interval_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// interval structured binding
TEST(IntervalTest, interval_structured_binding) {

    interval iv{-3, 5};
    auto [lo, hi] = iv;
    ASSERT_EQ(lo, -3);
    ASSERT_EQ(hi, 5);

    // Mutating through structured binding writes back to source.
    auto& [mlo, mhi] = iv;
    mlo              = -7;
    ASSERT_EQ(iv.Lower, -7);
}

// interval construction and predicates
TEST(IntervalTest, interval_construction_and_predicates) {
    {
        SCOPED_TRACE("point interval");
        constexpr interval p{*(3_r / 4), *(3_r / 4)};
        static_assert(p.Lower == p.Upper);
        static_assert(includes(p, *(3_r / 4)));
        static_assert(!(includes(p, 0)));
    }

    {
        SCOPED_TRACE("includes / excludes / overlaps");
        constexpr interval a{0, 10};
        constexpr interval b{5, 15};
        constexpr interval c{20, 30};

        static_assert(includes(a, 5));
        static_assert(includes(a, 0));
        static_assert(includes(a, 10));
        static_assert(!(includes(a, 11)));

        static_assert(overlaps(a, b));
        static_assert(!(excludes(a, b)));
        static_assert(excludes(a, c));
        static_assert(!(overlaps(a, c)));
    }

    {
        SCOPED_TRACE("includes whole sub-interval");
        constexpr interval outer{0, 100};
        constexpr interval inner{10, 90};
        static_assert(includes(outer, inner));
        static_assert(!(includes(inner, outer)));
    }
}

// interval arithmetic via lift
TEST(IntervalTest, interval_arithmetic_via_lift) {
    constexpr interval a{0, 10};
    constexpr interval b{2, 5};

    {
        SCOPED_TRACE("addition");
        constexpr auto r = a + b;
        static_assert(r.Lower == 2);
        static_assert(r.Upper == 15);
    }

    {
        SCOPED_TRACE("subtraction propagates via -rhs");
        constexpr auto r = a - b;
        static_assert(r.Lower == -5);
        static_assert(r.Upper == 8);
    }

    {
        SCOPED_TRACE("multiplication takes the bounding box");
        constexpr interval x{-2, 3};
        constexpr interval y{-4, 5};
        constexpr auto     r = x * y;
        static_assert(r.Lower == -12); // -2 * 6 ... actually min(-2*-4, -2*5, 3*-4, 3*5) = min(8,-10,-12,15) = -12
        static_assert(r.Upper == 15);
    }

    {
        SCOPED_TRACE("division excludes zero in divisor");
        constexpr interval c{-1, 1}; // contains 0
        constexpr auto     r = a / c;
        static_assert(!(r.has_value()));
    }

    {
        SCOPED_TRACE("division by positive interval");
        constexpr interval pos{2, 5}; // strictly positive
        constexpr auto     r = a / pos;
        static_assert(r.has_value());
    }
}

// interval ordering
TEST(IntervalTest, interval_ordering) {
    constexpr interval a{0, 10};
    constexpr interval b{20, 30};
    constexpr interval c{5, 15};
    constexpr interval d{0, 10};

    static_assert(a < b);
    static_assert(b > a);
    static_assert(a == d);
    // Overlapping but non-equal → unordered
    static_assert((a <=> c) == std::partial_ordering::unordered);
}

// interval / rational notch
TEST(IntervalTest, interval_rational_notch) {
    constexpr interval a{0, 10};

    {
        SCOPED_TRACE("divides_evenly");
        static_assert(a.divides_evenly(1_r));
        static_assert(a.divides_evenly(0.5_r));
        static_assert(!(a.divides_evenly(3_r)));
    }

    {
        SCOPED_TRACE("operator/ rational gives notch count");
        constexpr auto r = a / 1_r;
        static_assert(r.has_value());
        static_assert(*r == 10);

        constexpr auto r2 = a / 0.5_r;
        static_assert(r2.has_value());
        static_assert(*r2 == 20);
    }
}
} // namespace interval_tests

// literals
namespace literals_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// _r literal: integer forms
TEST(LiteralsTest, r_literal_integer_forms) {
    static_assert(5_r == rational{5});
    static_assert(1'000_r == rational{1000});
    static_assert(0xff_r == rational{255});
    static_assert(0xFF_r == rational{255});
    static_assert(0b1010_r == rational{10});
    static_assert(0b1'010_r == rational{10});
}

// _r literal: decimal forms
TEST(LiteralsTest, r_literal_decimal_forms) {
    static_assert(1.25_r == rational{5, 4});
    static_assert(.5_r == rational{1, 2});
    static_assert(0.1_r == rational{1, 10}); // exact, no double round-trip
    static_assert(0.01_r == rational{1, 100});
    static_assert(3.14_r == rational{157, 50});
}

// _r literal: decimal scientific (e)
TEST(LiteralsTest, r_literal_decimal_scientific_e) {
    static_assert(1.5e2_r == rational{150});
    static_assert(2.5e-1_r == rational{1, 4});
    static_assert(1e3_r == rational{1000});
    static_assert(1e-3_r == rational{1, 1000});
}

// _r literal: hex float / binary exponent (p)
TEST(LiteralsTest, r_literal_hex_float_binary_exponent_p) {
    static_assert(0x1p15_r == rational{32768});
    static_assert(0x1p-15_r == rational{1, 32768});
    static_assert(0x3p-4_r == rational{3, 16});
    static_assert(0x1.8p3_r == rational{12});
    static_assert(0x1.8p0_r == rational{3, 2});
    static_assert(0x1p-14_r == rational{1, 16384}); // Q14 notch
}

// _ins literal: produces point inside (just<value>)
TEST(LiteralsTest, ins_literal_produces_point_inside_just_value) {
    constexpr auto five = 5_ins;
    static_assert(lower_of<decltype(five)> == 5);
    static_assert(upper_of<decltype(five)> == 5);

    constexpr auto quarter = 0.25_ins;
    static_assert(lower_of<decltype(quarter)> == rational{1, 4});
    static_assert(upper_of<decltype(quarter)> == rational{1, 4});

    constexpr auto q14_notch = 0x1p-14_ins;
    static_assert(lower_of<decltype(q14_notch)> == rational{1, 16384});
}

// a_b / b_b ~= rational{a,b} - value-equivalent (expected-wrapped)
TEST(LiteralsTest, a_b_b_b_rational_a_b_value_equivalent_expected_wrapped) {
    // Point bounds: the divisor is a nonzero point and the quotient provably
    // fits, so `inside / inside` is a plain point inside with grid {a/b, a/b}.
    constexpr auto three_quarters = 3_ins / 4_ins;
    static_assert(three_quarters == rational{3, 4});
    static_assert(!is_expected_v<decltype(three_quarters)>);

    using point_t = std::remove_cv_t<decltype(three_quarters)>;
    static_assert(lower_of<point_t> == rational{3, 4});
    static_assert(upper_of<point_t> == rational{3, 4});
}

// _r and _ins agree
TEST(LiteralsTest, r_and_ins_agree) {
    static_assert(1.25_ins == 1.25_r);
    static_assert(0x1p-14_ins == 0x1p-14_r);
    static_assert(1.5e2_ins == 1.5e2_r);
}
} // namespace literals_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

#define WIDE_LONG_DOUBLE (LDBL_MANT_DIG > DBL_MANT_DIG)

// A bound of 2^63 or more: the range prefilter's step 1/MaxDenominator
// narrowed to imax and did not compile.
TEST(GridTest, max_denominator_past_imax) {
    using B = inside<{{0, 1}, max_denominator{rational{umax{1} << 63}}}, round_toward_zero>;
    B x{};
    x = 0.3;
    EXPECT_EQ(x, rational(0.3));
    x = 0x1p-70;
    EXPECT_EQ(x, 0);
    errc ec{};
    x.policy(ec) = 1.0 + 0x1p-52;
    EXPECT_EQ(ec, errc::overflow);
}

// The _g literal multiplied in grid_int without a limit: a long literal
// wrapped; trailing zeros counted as digits.
TEST(GridTest, grid_literal_takes_long_exact_literals) {
    static_assert(1.50000000000000000000000000000000000000000000000000000000000000000000000000000000000000_g ==
                  rational{3, 2});
    static_assert(100000000000000000000000000000000000000000000000000000000000000000000000000000000000e-83_g ==
                  rational{1});
    static_assert(0e500_g == rational{0});
    static_assert(18446744073709551615_g == rational{~umax{0}});
}

// The _r literal rejected 0x1p63 (which fits) and fine forms whose factors
// cancel; a decimal denominator past imax was checked before reducing.
TEST(GridTest, rational_literal_cancels_before_the_limits) {
    static_assert(0x1p63_r == rational{umax{1} << 63});
    static_assert(0x0.1p4_r == rational{1});
    static_assert(0x10p-66_r == rational{1, 1ll << 62});
    static_assert(0.5e-18_r == rational{1, 2'000'000'000'000'000'000ll});
}

// includes(interval, double) went through rational{d}: bits past 2^-62 were
// dropped and |d| ≥ 2^64 terminated.
TEST(GridTest, includes_a_double_exactly) {
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
TEST(GridTest, literal_forms_with_a_leading_zero) {
    static_assert(0_ins == 0);
    static_assert(01.5_r == rational{3, 2});
    static_assert(0x10_r == 16);
    static_assert(0.5_r == rational{1, 2});
}

// includes(interval, long double) and grid::representable(long double)
// dropped the bits past the double's.
TEST(GridTest, interval_and_grid_take_long_double_exactly) {
    using I64 = std::int64_t;
#if WIDE_LONG_DOUBLE
    constexpr grid G{{-100, 100}, per<4>};
    static_assert(!G.representable(1.0L + 0x1p-60L) && G.representable(1.0L));
    static_assert(!includes(interval{-100, 1}, 1.0L + 0x1p-60L) && !includes(interval{-1, 0}, 0x1p-70L));
    constexpr grid C{{-1, 2}, max_denominator{I64{1} << 62}};
    static_assert(!C.representable(1.0L + 0x1p-63L)); // finer than the bound
    static_assert(C.representable(1.0L + 0x1p-60L));
    EXPECT_FALSE(G.representable(test::at_run_time(1.0L + 0x1p-60L)));
    EXPECT_FALSE(includes(interval{-1, 0}, test::at_run_time(0x1p-70L)));
#endif
}

// A fine double on a grid whose v − Lower passes one limb: the
// lattice test formed that difference as a big grid number, which exists
// only at compile time (std::terminate from the noexcept representable).
TEST(GridTest, representable_fine_double_at_runtime) {
    constexpr grid G{{-100, 100}, per<4>};
    EXPECT_FALSE(G.representable(test::at_run_time(0x1.0000000000001p-10)));
    EXPECT_TRUE(G.representable(test::at_run_time(0.25)));
}

//---------------------------------------------------------------------------
// gcd(rational, rational) (rational.hpp)
//
// `gcd(rational, rational)` calls `std::lcm` on |Den|s without an
// overflow check and casts the result to imax via static_cast. Large
// denominators silently wrap. The fix is to make `gcd` return
// `std::expected<rational, errc>` and detect the overflow.
//
// Trigger: lcm(2^62, 3) = 3 * 2^62. That fits in umax (≈1.38e19) but
// exceeds imax_max (≈9.22e18). After the cast to imax it goes negative,
// producing a bogus rational.
//---------------------------------------------------------------------------
TEST(GridTest, grid_plus_reports_gcd_lcm_overflow) {
    // Use grid arithmetic since gcd's return type changes — the error
    // surfaces at grid::operator+ which already returns expected<grid, errc>.
    constexpr auto big   = rational{1u, imax{1} << 62};
    constexpr auto third = rational{1u, 3};

    constexpr grid g1{interval{0_r, 1_r}, big};
    constexpr grid g2{interval{0_r, 1_r}, third};

    // Big grid numbers: the notch gcd 1/(3·2^62) is exact.
    static_assert((g1 + g2).Notch ==
                  detail::grid_rational{detail::big_int{1}, detail::big_int{3} * detail::big_int{imax{1} << 62}});
}

} // namespace pinned_tests
