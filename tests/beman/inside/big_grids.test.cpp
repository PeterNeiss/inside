// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids past 64 bits: big grid numbers and wide raws, construction,
// every policy, comparison, arithmetic, conversions, text, hashing and
// sampling.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/numeric_limits.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/inside/range.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/math/integer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cfloat>
#include <cstdint>
#include <format>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "support.hpp"

// Grids whose limits or notch pass 64 bits: storage, construction and
// assignment, comparison, arithmetic with result grids past 64 bits,
// conversions and io.
namespace big_grid_tests {
using namespace beman::inside;

using detail::big_int;
using detail::grid_rational;

namespace {
consteval big_int       pow2(int k) { return big_int{1} << k; }
consteval grid_rational g(big_int v) { return grid_rational{v}; }

// {0, 2^100} in steps of 1: a 101-bit index.
using huge = inside<{0, g(pow2(100))}>;
// Big values, few slots: {2^100, 2^100 + 10} has 11 slots (a uint8 index).
using far = inside<{g(pow2(100)), g(pow2(100) + big_int{10})}>;
// A notch past 64 bits: {0, 1} in steps of 2^-80.
using fine = inside<{{0, 1}, grid_rational{big_int{1}, pow2(80)}}>;

// Raws past 64 bits are compile-time constants: a big value can only be
// formed during constant evaluation.
constexpr huge::raw_type raw_2_100   = static_cast<huge::raw_type>(pow2(100));
constexpr huge::raw_type raw_2_100_3 = static_cast<huge::raw_type>(pow2(100) + big_int{3});
constexpr huge::raw_type raw_2_65_2  = static_cast<huge::raw_type>(big_int{~std::uint64_t{0}} * big_int{2});
} // namespace

TEST(BigGridTest, storage) {
    static_assert(std::is_same_v<huge::raw_type, detail::wide_uint<2>>);
    static_assert(std::is_same_v<far::raw_type, std::uint8_t>);          // 11 slots
    static_assert(std::is_same_v<fine::raw_type, detail::wide_uint<2>>); // 2^80 + 1 slots
    static_assert(detail::wide_grid_numbers<far> && detail::wide_valued<far>);
    static_assert(!detail::wide_grid_numbers<inside<{0, 100}>>);
    // Equal grids are the same type, however they were spelled.
    static_assert(std::is_same_v<inside<{0, g(pow2(50) * pow2(50))}>, huge>);
}

TEST(BigGridTest, construction_and_comparison) {
    huge h = 5;
    EXPECT_TRUE(h == 5);
    EXPECT_TRUE(h < 6 && h > 4.5_r);
    const far f = test::store<far>(huge::from_raw(raw_2_100_3));
    EXPECT_TRUE(f > h);
    EXPECT_TRUE(f.raw() == 3);
    fine x = 0.5;
    EXPECT_TRUE(x == 0.5_r);
    EXPECT_TRUE(x.raw() == (detail::wide_uint<2>{1} << 79));
    // Values past 64 bits compare exactly.
    EXPECT_TRUE(f == huge::from_raw(raw_2_100_3));
}

TEST(BigGridTest, assignment_policies) {
    huge h = 0;
    EXPECT_EQ(test::store_error<huge>(test::at_run_time(-1)), errc::overflow);
    EXPECT_EQ(test::store_error<huge>(test::at_run_time(1e31)), errc::overflow); // past 2^100
    inside<{0, g(pow2(100))}, clamp> c = 0;
    c                                  = 1e31;
    EXPECT_TRUE(c == huge::from_raw(raw_2_100));
    inside<{{0, 1}, grid_rational{big_int{1}, pow2(80)}}, round_nearest> r = 0;
    r                = rational{1, 3}; // rounds to the nearest 2^-80
    const bool close = r > (rational{1, 3} - rational{1, 1 << 30}) && r < (rational{1, 3} + rational{1, 1 << 30});
    EXPECT_TRUE(close);
}

TEST(BigGridTest, arithmetic_past_64_bits) {
    huge a = 7, b = 5;
    auto s = a + b; // {0, 2^101}
    EXPECT_TRUE(s == 12);
    auto d = a - b; // {−2^100, 2^100}
    EXPECT_TRUE(d == 2);
    auto p = a * b; // {0, 2^200}
    static_assert(grid_of<decltype(p)>.slot_bits() == 201);
    EXPECT_TRUE(p == 35);
    auto n = -a;
    EXPECT_TRUE(n == -7);
    // uint64 + uint64: its upper bound 2^65 − 2 needs big grid numbers.
    constexpr std::uint64_t u64_max = ~std::uint64_t{0};
    auto                    q       = uint64{u64_max} + uint64{u64_max};
    EXPECT_TRUE(q == huge::from_raw(raw_2_65_2));
}

TEST(BigGridTest, conversions_and_io) {
    const far f = test::store<far>(huge::from_raw(raw_2_100_3));
    EXPECT_EQ(to_string(f), "1267650600228229401496703205379");
    EXPECT_EQ(f.to<int>().error(), errc::overflow);
    EXPECT_DOUBLE_EQ(f.to<double>(), 0x1p100);
    EXPECT_TRUE(parse<huge>("1267650600228229401496703205376").value() == huge::from_raw(raw_2_100));
    fine x = 0.25;
    EXPECT_EQ(to_string(x), to_string(rational{1, 4}));
    // Exact decimals of any length, for values and grid numbers.
    EXPECT_EQ(to_string(fine::from_raw(1)),
              "0.00000000000000000000000082718061255302767487140869206996285356581211090087890625");
    EXPECT_EQ(to_string(grid_of<inside<{{0, 1}, 1e-30_g}>>), "{[0..1], 0.000000000000000000000000000001}");
    constexpr grid_rational third_80{big_int{1}, pow2(80) * big_int{3}};
    EXPECT_EQ(to_string(third_80), "1/3626777458843887524118528");
    EXPECT_EQ(std::format("{}", f), "1267650600228229401496703205379");
    EXPECT_EQ(std::format("{}", fine::from_raw(1)), to_string(fine::from_raw(1)));
    EXPECT_EQ(std::format("{:.3e} {:.2f}", f, f), "1.268e+30 1267650600228229401496703205379.00");
    using micro = inside<{{0, 1}, 1e-30_g}>; // a decimal notch: 30 decimals
    EXPECT_EQ(to_string(micro{0.5}), "0.500000000000000000000000000000");
}

TEST(BigGridTest, grid_number_literal) {
    // _g spells grid numbers of any size, exactly — including decimal notches.
    static_assert(std::is_same_v<inside<{0, 1267650600228229401496703205376_g}>, huge>);
    using micro30 = inside<{{0, 1}, 1e-30_g}>;          // a 10^-30 notch
    static_assert(grid_of<micro30>.slot_bits() == 100); // 10^30 slots
    micro30 m = 0.5;
    EXPECT_TRUE(m == 0.5_r);
    static_assert(123'456'789'012'345'678'901'234'567'890_g ==
                  grid_rational{big_int{123'456'789'012'345'678ull} * big_int{1'000'000'000'000ull} +
                                big_int{901'234'567'890ull}});
    // Grid numbers take built-in integers: {0, 2^100 − 1} without spelling 1_g.
    static_assert(std::is_same_v<inside<{0, 1267650600228229401496703205376_g - 1}>,
                                 inside<{0, 1267650600228229401496703205375_g}>>);
}

// The exact paths are constexpr.
namespace {
static_assert(huge{7} + huge{5} == 12);
static_assert(huge{7} * huge{5} == 35);
} // namespace
TEST(BigGridTest, math_grid_ops) {
    // abs, sign, copysign, floor/ceil/round/trunc and fmod run on exact values.
    using fine_r = inside<{{-4, 4}, grid_rational{big_int{1}, pow2(80)}}, round_nearest>;
    using fpos   = inside<{{1, 4}, grid_rational{big_int{1}, pow2(80)}}, round_nearest>;
    using hsym   = inside<{-g(pow2(100)), g(pow2(100))}>;
    using hpos   = inside<{1, g(pow2(100))}>;
    const fine_r x{-2.75};
    const hsym   h{-7};
    EXPECT_EQ(math::abs(x), 2.75_r);
    EXPECT_EQ(math::abs(h), 7);
    EXPECT_EQ(math::sign(x), -1);
    EXPECT_EQ(math::copysign(fine_r{1.5_r}, x), -1.5_r);
    EXPECT_EQ(math::floor(x), -3);
    EXPECT_EQ(math::ceil(x), -2);
    EXPECT_EQ(math::round(x), -3);
    EXPECT_EQ(math::trunc(x), -2);
    EXPECT_EQ(math::fmod(x, fpos{1.5}), -1.25_r);
    EXPECT_EQ(math::fmod(h, hpos{3}), -1);
    static_assert(upper_of<decltype(math::abs(h))> == g(pow2(100)));
    static_assert(lower_of<decltype(math::floor(x))> == -4);
}

TEST(BigGridTest, just_big_and_small_grid_numbers) {
    // Points past 64 bits, in either direction.
    constexpr auto big  = just<g(pow2(100))>;
    constexpr auto tiny = just<1.616255e-35_g>;
    constexpr auto five = just<5_g>;
    static_assert(five == 5);
    static_assert(big > five && tiny < five && tiny > 0);
    EXPECT_TRUE(big == huge::from_raw(raw_2_100));
    EXPECT_DOUBLE_EQ(big.to<double>(), 0x1p100);
    EXPECT_DOUBLE_EQ(tiny.to<double>(), 1.616255e-35);
    EXPECT_EQ(to_string(tiny), "0.00000000000000000000000000000000001616255");
    // Stored into a grid; added as a point delta.
    huge h = big;
    EXPECT_TRUE(h == big);
    h = 0;
    errc ec{}; // h + 2^99 may pass 2^100: the store reports
    h.policy(ec) += just<g(pow2(99))>;
    h.policy(ec) += just<g(pow2(99))>;
    EXPECT_TRUE(h == big && ec == errc{});
    using planck = inside<{{0, 1}, 1e-41_g}>;
    planck p     = tiny;
    EXPECT_TRUE(p == tiny);
}

TEST(BigGridTest, division) {
    // Big ÷ big: the exact quotient, reported when it passes the 64-bit rational.
    const far  a = test::store<far>(huge::from_raw(raw_2_100_3));
    const huge b{huge::from_raw(raw_2_100)};
    const auto q = a / b; // (2^100 + 3) / 2^100: exact in the quotient's wide fraction
    ASSERT_TRUE(q.has_value());
    EXPECT_EQ(
        to_string(*q),
        "1.0000000000000000000000000000023665827156630354162351856958483586890196193053270690143108367919921875");
    const auto r = test::store<far>(huge::from_raw(raw_2_100)) / b;
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(*r == 1);
    const auto s = b / huge{4};
    ASSERT_TRUE(s.has_value());
    constexpr huge::raw_type raw_2_98 = static_cast<huge::raw_type>(pow2(98));
    EXPECT_TRUE(*s == huge::from_raw(raw_2_98));
    const auto t = huge{12} / huge{8};
    ASSERT_TRUE(t.has_value());
    EXPECT_TRUE(*t == 1.5_r);
    // Under a rounding policy the integer quotient rounds, as on 64-bit grids
    // (it was the exact 3.5).
    const auto f = div(huge{7}, huge{2}, round_floor);
    ASSERT_TRUE(f.has_value());
    EXPECT_TRUE(*f == 3);
    static_assert(grid_of<decltype(f)::value_type>.Notch == 1);
    EXPECT_TRUE(div(huge{7}, inside<{2, 2}>{}, round_ceil) == 4);
    using micro  = inside<{{0, 1}, 1e-30_g}>;
    const auto u = micro{0.5} / micro{0.25};
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(*u == 2);
}

TEST(BigGridTest, divmod_past_64_bits) {
    // Quotient and remainder of big integers in one exact division, each on
    // the grid of `/` and `%`, agreeing with them.
    using seven  = inside<{7, 7}>;
    using three  = inside<{3, 3}>;
    using digits = inside<{-9, 9}>;
    const huge a{huge::from_raw(raw_2_100_3)};
    const auto [q, r] = divmod(a, seven{}); // a point divisor: no zero, a plain result
    EXPECT_TRUE(q * seven{} + r == a);
    EXPECT_TRUE(r == a % seven{});
    EXPECT_TRUE(q == div(a, seven{}, round_toward_zero));
    EXPECT_TRUE(r == 5); // (2^100 + 3) mod 7
    using signed_huge    = inside<{-g(pow2(100)), g(pow2(100))}>;
    const signed_huge n  = -signed_huge{huge::from_raw(raw_2_100)}; // −2^100
    const auto        fl = divmod(n, digits{3}, round_floor);
    ASSERT_TRUE(fl.has_value());
    EXPECT_TRUE(fl->Remainder == 2); // floored: −2^100 mod 3 = 2
    EXPECT_TRUE(fl->Quotient * three{} + fl->Remainder == n);
    EXPECT_FALSE(divmod(n, digits{0}).has_value());
}

namespace {
constexpr grid_rational two_100   = g(pow2(100));
constexpr grid_rational below     = two_100 - 1;
constexpr grid_rational above     = two_100 + 1;
constexpr grid_rational far_out   = g(pow2(5000)); // wider than any store's exact type
constexpr grid_rational far_frac  = grid_rational{big_int{1}, pow2(5000)};
constexpr grid_rational half_past = grid_rational{pow2(101) - big_int{1}, big_int{2}}; // 2^100 − 1/2
static_assert(huge{below} + huge{1} == huge{two_100});                                 // the store is constexpr too
} // namespace

TEST(BigGridTest, grid_number_at_run_time) {
    // A grid number past 64 bits is formed at compile time only; a store
    // reads one at run time through try_make (a constant: construct, assign).
    volatile int             pick      = 1;
    const grid_rational&     v         = pick ? below : two_100; // chosen at run time
    huge                     a         = *huge::try_make(v);
    constexpr huge::raw_type raw_below = static_cast<huge::raw_type>(pow2(100) - big_int{1});
    EXPECT_TRUE(a.raw() == raw_below);
    a = two_100;
    EXPECT_TRUE(a == huge::from_raw(raw_2_100));
    using small = inside<{-5, 5}>;
    EXPECT_TRUE(small{grid_rational{-3}} == -3); // a value within 64 bits
    // Out of range: try_make reports, clamp saturates (and a checked constant
    // store, small{two_100}, does not compile).
    EXPECT_EQ(small::try_make(two_100).error(), errc::overflow);
    EXPECT_TRUE((small{two_100, clamp}) == 5);
    EXPECT_TRUE((small{-far_out, clamp}) == -5);
    EXPECT_EQ(huge::try_make(above).error(), errc::overflow);
    EXPECT_EQ(huge::try_make(far_out).error(), errc::overflow);
    EXPECT_EQ(huge::try_make(-two_100).error(), errc::overflow);
    // Off the lattice: rounds under a rounding policy, else rejected.
    EXPECT_FALSE(huge::try_make(half_past).has_value());
    EXPECT_TRUE((huge{half_past, round_floor}) == huge{below});
    EXPECT_TRUE((huge{half_past, round_ceil}) == huge{two_100});
    EXPECT_EQ(huge::try_make(far_frac).error(), errc::overflow);
    // A fine big grid takes its own notch.
    constexpr grid_rational three_notches = grid_rational{big_int{3}, pow2(80)};
    fine                    f             = three_notches;
    EXPECT_TRUE(f.raw() == 3);
}

TEST(BigGridTest, continuous_past_64_bits) {
    // A quotient of big-grid values holds its exact value: a wide fraction raw,
    // sized from the quotient's denominator bound — so a plain value.
    using length          = inside<{{1e-41_g, 1e27_g}, 1e-41_g}, round_nearest>;
    const length planck   = *parse<length>("1.616255e-35");
    const length universe = *parse<length>("8.8e26");
    const auto   ratio    = universe / planck;
    static_assert(detail::fraction_storage<std::remove_cvref_t<decltype(ratio)>>);
    EXPECT_EQ(to_string(ratio), "17600000000000000000000000000000000000000000000000000000000000000000/323251");
    EXPECT_TRUE(ratio > just<5.4e61_g> && ratio < just<5.5e61_g>);
    EXPECT_EQ(std::format("{:.6e}", ratio), "5.444685e+61");
    using decade = inside<{{-100, 100}, per<1 << 20>}, round_nearest>;
    EXPECT_EQ(math::log10<decade>(ratio), (decade{rational{64'734'859, 1 << 20}})); // 61.7359724…
    const auto one = universe / universe;
    EXPECT_TRUE(one == 1);
    EXPECT_EQ(to_string(planck / length{2}), "0.000000000000000000000000000000000008081275");

    // Declared directly, with every policy.
    using big_real = inside<{{0, 1e30_g}, max_denominator{1000}}>;
    static_assert(detail::fraction_storage<big_real>);
    big_real b = rational{1, 3};
    EXPECT_EQ(to_string(b), "1/3");
    b = *parse<big_real>("123456789012345678901234567890/11");
    EXPECT_EQ(to_string(b), "123456789012345678901234567890/11");
    EXPECT_EQ(test::store_error<big_real>(test::at_run_time(-1)), errc::overflow);
    inside<{{0, 1e30_g}, max_denominator{1000}}, clamp> c = 0;
    c                                                     = huge::from_raw(raw_2_100);
    EXPECT_EQ(to_string(c), "1000000000000000000000000000000");
    inside<{{0, 1e30_g}, max_denominator{1000}}, wrap> w = 0;
    w = test::store<far>(huge::from_raw(raw_2_100_3)); // 2^100 + 3 − 10^30
    EXPECT_EQ(to_string(w), "267650600228229401496703205379");
    // + and × hold the exact value, in fractions sized from the bounds.
    const auto s = b + b;
    static_assert(!detail::is_expected_v<decltype(s)>);
    EXPECT_EQ(to_string(s), "246913578024691357802469135780/11");
    const auto p = b * big_real{rational{1, 2}};
    static_assert(!detail::is_expected_v<decltype(p)>);
    EXPECT_EQ(to_string(p), "61728394506172839450617283945/11");
    // A denominator past the bound is off the grid.
    EXPECT_EQ(parse<big_real>("1/1001").error(), errc::rounding_error);
}

// A continuous grid past 64 bits (an exact-fraction raw) negates and subtracts.
TEST(BigGridTest, big_continuous_negates) {
    using F = inside<{{-1e30_g, 1e30_g}, max_denominator{1000}}>;
    static_assert(detail::fraction_storage<F>);
    const F a{3}, b{rational{1, 2}};
    EXPECT_EQ(to_string(-a), "-3");
    EXPECT_EQ(to_string(b - a), "-2.5");
}

// A same-notch store between grids whose lattices sit off 0 shifts by the
// exact offset between their Lowers (here one notch: 1/8 − (−1/8) = 1/4).
TEST(BigGridTest, same_notch_store_between_unanchored_wide_grids) {
    using L = inside<{{-0.125_g, 1180591620717411303423.875_g}, 0.25_g}>;
    using R = inside<{{0.125_g, 1180591620717411303424.125_g}, 0.25_g}>;
    const R r{rational{11, 8}};
    L       l{rational{-1, 8}};
    l = test::store<L>(r); // R's top is past L's: a checked store
    EXPECT_EQ(to_string(l), "1.375");
}

// A point quotient is its type's one value: plain, even past 64 bits, and
// expected only for a divisor grid that holds 0.
TEST(BigGridTest, point_quotients_are_plain) {
    const auto q = just<1e30_g> / just<3>;
    static_assert(!detail::is_expected_v<decltype(q)>);
    EXPECT_EQ(to_string(q), "1000000000000000000000000000000/3");
    const auto z = just<0> / inside<{-1, 1}>{1};
    static_assert(detail::is_expected_v<decltype(z)>);
    EXPECT_TRUE(z.has_value() && *z == 0);
    EXPECT_EQ((just<0> / inside<{-1, 1}>{0}).error(), errc::division_by_zero);
}
} // namespace big_grid_tests

// Grids with more than 2^64 slots store a wide_int index instead of falling
// back to a rational: construction, assignment under every policy, comparison,
// arithmetic, conversions, io, hashing and sampling.
namespace wide_grid_tests {
using namespace beman::inside;

namespace {
constexpr std::uint64_t p32 = std::uint64_t{1} << 32;
constexpr std::uint64_t p34 = std::uint64_t{1} << 34;

// {0, 2^34} in steps of 2^-32: 2^66 + 1 slots, a 67-bit index.
using fine         = inside<{{0, p34}, per<p32>}>;
using fine_nearest = inside<{{0, p34}, per<p32>}, round_nearest>;
using fine_clamp   = inside<{{0, p34}, per<p32>}, clamp>;
using fine_wrap    = inside<{{0, p34}, per<p32>}, wrap>;
// Signed and offset: {−2^34, 2^34} in steps of 2^-32, Lower ≠ 0.
using fine_signed = inside<{{-rational{p34}, rational{p34}}, per<p32>}>;

constexpr rational tick{std::uint64_t{1}, static_cast<std::int64_t>(p32)}; // 2^-32
} // namespace

TEST(WideGridTest, deduces_wide_index) {
    static_assert(std::is_same_v<fine::raw_type, detail::wide_uint<2>>);
    static_assert(std::is_same_v<fine_signed::raw_type, detail::wide_uint<2>>);
    static_assert(sizeof(fine) == 16);
    static_assert(detail::wide_index_storage<fine> && detail::index_storage<fine>);
    static_assert(grid_of<fine>.slot_bits() == 67);
    // A 64-bit rational cannot hold every value: no implicit conversion.
    static_assert(!std::is_convertible_v<fine, rational>);
    static_assert(!std::is_convertible_v<fine, std::int64_t>);
}

TEST(WideGridTest, construction_from_scalars) {
    fine a = 5;
    EXPECT_TRUE(a == 5);
    EXPECT_TRUE(a.raw() == detail::wide_uint<2>{5 * p32});
    fine b = 0.5;
    EXPECT_TRUE(b == 0.5_r);
    fine c = tick; // the first notch
    EXPECT_TRUE(c == tick);
    EXPECT_TRUE(c.raw() == detail::wide_uint<2>{1});
    fine top = p34;
    EXPECT_TRUE(top == p34);
    EXPECT_TRUE(top.raw() == (detail::wide_uint<2>{1} << 66));

    fine_signed s = -3.25;
    EXPECT_TRUE(s == -3.25_r);
    EXPECT_TRUE(s < 0);
}

TEST(WideGridTest, assignment_policies) {
    // checked: off-lattice and out-of-range report.
    fine a = 1;
    errc ae{};
    a.policy(ae) = test::at_run_time(rational{1, std::int64_t{1} << 33}); // half a notch
    EXPECT_EQ(ae, errc::rounding_error);
    EXPECT_EQ(test::store_error<fine>(test::at_run_time(p34 + 1)), errc::overflow);
    EXPECT_EQ(test::store_error<fine>(test::at_run_time(-1)), errc::overflow);
    EXPECT_TRUE(a == 1); // unchanged

    // rounding: half a notch rounds away from zero.
    fine_nearest n = 0;
    n              = rational{1, std::int64_t{1} << 33};
    EXPECT_TRUE(n == tick);

    // clamp and wrap.
    fine_clamp c = 0;
    c            = p34 + 7;
    EXPECT_TRUE(c == p34);
    c = -2.0;
    EXPECT_TRUE(c == 0);
    fine_wrap w = 0;
    w           = p34 + 1; // one past the top: 2^32 notches past it
    EXPECT_TRUE(w == (rational{1} - tick));

    // errc-reporting policy.
    errc e{};
    fine x      = 3;
    x.policy(e) = p34 * 2;
    EXPECT_EQ(e, errc::overflow);
    EXPECT_TRUE(x == 3);

    // non-finite doubles.
    EXPECT_EQ(test::store_error<fine>(test::at_run_time(std::numeric_limits<double>::infinity())), errc::not_finite);
    c = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(c == p34);
}

TEST(WideGridTest, assignment_between_insides) {
    fine a = inside<{0, 100}>{42};
    EXPECT_TRUE(a == 42);
    // wide → narrow: the narrow grid's notch is coarser, so it rounds toward zero.
    fine                            b = 41.75;
    inside<{0, 100}, round_nearest> n = 0;
    n                                 = test::store<inside<{0, 100}, round_nearest>>(b);
    EXPECT_EQ(n.as<std::int64_t>(), 42);
    // wide → wide of another lattice.
    fine_signed s = b;
    EXPECT_TRUE(s == 41.75_r);
}

TEST(WideGridTest, comparisons) {
    fine a = 1.5, b = 2;
    EXPECT_TRUE(a < b && b > a && a != b);
    EXPECT_TRUE((a == 1.5_r && a == rational{3, 2}));
    EXPECT_TRUE(a > 1 && a < 2u && a < just<1e30_g> && a > -just<1e30_g>);
    EXPECT_TRUE((a == inside<{{0, 10}, per<2>}>{1.5}));
    EXPECT_TRUE((a < inside<{0, 10}>{2}));
    fine_signed s = -1.5;
    EXPECT_TRUE(s < a);
    EXPECT_TRUE(-a == s);
    EXPECT_EQ(a <=> b, std::strong_ordering::less);
}

TEST(WideGridTest, arithmetic) {
    fine a = 1.5, b = (rational{1} + tick);
    auto sum = a + b; // {0, 2^35}, notch 2^-32
    static_assert(detail::wide_index_storage<decltype(sum)>);
    EXPECT_TRUE(sum == (rational{5, 2} + tick));
    auto diff = a - b; // {−2^34, 2^34}
    EXPECT_TRUE(diff == (rational{1, 2} - tick));
    auto neg = -a;
    EXPECT_TRUE(neg == -1.5_r);
    auto prod = a * inside<{0, 3}>{3}; // {0, 3·2^34}, notch 2^-32
    static_assert(detail::wide_index_storage<decltype(prod)>);
    EXPECT_TRUE(prod == 4.5_r);
    // narrow + wide and back.
    auto mixed = inside<{0, 100}>{7} + a;
    EXPECT_TRUE(mixed == 8.5_r);
}

TEST(WideGridTest, uint64_difference_widens) {
    constexpr std::uint64_t u64_max = ~std::uint64_t{0};
    uint64                  x{u64_max}, y{3};
    auto                    d = x - y; // spans 2^65 values
    static_assert(detail::wide_index_storage<decltype(d)>);
    EXPECT_TRUE(d == u64_max - 3);
    EXPECT_TRUE(y - x == -rational{u64_max - 3});
    EXPECT_EQ(x.to<std::uint64_t>(), u64_max);
}

TEST(WideGridTest, conversions) {
    fine a = 41.75;
    EXPECT_EQ(a.to<int>().error(), errc::rounding_error); // 41.75: no rounding named
    EXPECT_EQ((a.to<int, round_toward_zero>().value()), 41);
    EXPECT_EQ(a.to<double>(), 41.75);
    EXPECT_EQ((a.to<std::uint8_t, round_nearest>().value()), 42);
    fine big = p34;
    EXPECT_EQ(big.to<std::int32_t>().error(), errc::overflow);
    EXPECT_EQ(fine_signed{-1}.to<unsigned>().error(), errc::domain_error);
}

TEST(WideGridTest, exact_read_out) {
    // 2^33 + 2^-32 = (2^65 + 1) / 2^32, in lowest terms: insides on the
    // numerator and denominator grids, wide where they must be.
    using detail::big_int, detail::grid_rational;
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    using N        = decltype(odd.numerator());
    static_assert(detail::wide_index_storage<N>);
    static_assert(grid_of<N> == grid{{0, grid_rational{big_int{1} << 66}}, 1}); // 2^34 · 2^32
    static_assert(grid_of<decltype(odd.denominator())> == grid{{1, std::int64_t{1} << 32}, 1});
    EXPECT_TRUE(odd.numerator() == just<grid_rational{(big_int{1} << 65) + big_int{1}}>);
    EXPECT_TRUE(odd.denominator() == std::int64_t{1} << 32);
    const fine half{0.5};
    EXPECT_TRUE(half.numerator() == 1 && half.denominator() == 2);
    const fine_signed neg{-0.75};
    EXPECT_TRUE(neg.numerator() == -3 && neg.denominator() == 4);
    // A 64-bit grid whose values pass int64 reads out exactly too.
    using big64     = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;
    const big64 top = std::numeric_limits<std::uint64_t>::max();
    EXPECT_TRUE(top.numerator() == std::numeric_limits<std::uint64_t>::max());
    // On an integer grid the numerator is the value, on the same grid.
    static_assert(std::is_same_v<decltype(std::declval<inside<{0, 100}>>().numerator()), inside<{0, 100}>>);
    static_assert(std::is_same_v<decltype(std::declval<inside<{0, 100}>>().denominator()),
                                 std::remove_cvref_t<decltype(just<1>)>>);
}

TEST(WideGridTest, same_notch_assignment) {
    // A wider sum maps back by a raw shift; out of range runs the policy.
    // (A sum's grid is wider than fine's: checked stores, reported.)
    fine f = test::store<fine>(fine{1.5} + fine{2.25});
    EXPECT_TRUE(f == 3.75_r);
    fine_signed g = test::store<fine_signed>(fine{1.5} + fine{2.25}); // offset raw (Lower ≠ 0)
    EXPECT_TRUE(g == 3.75_r);
    g = test::store<fine_signed>(fine_signed{-1.5} - fine{2.25});
    EXPECT_TRUE(g == -3.75_r);
    const auto over = fine{rational{p34}} + fine{tick};
    EXPECT_EQ(test::store_error<fine>(over), errc::overflow);
    fine_clamp c = 0;
    c            = over;
    EXPECT_TRUE(c == rational{p34});
    fine_wrap w = 0;
    w           = over; // 2^34 + 2^-32 wraps to 0
    EXPECT_TRUE(w == 0);
}

TEST(WideGridTest, sum) {
    // 2^33 + 2^-32 per element: exact, however many 64-bit words the
    // index total needs.
    const fine        e = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    std::vector<fine> v(1000, e);
    using total = inside<{{0, p34 * 4096}, per<p32>}>;
    EXPECT_TRUE(beman::inside::sum<total>(v) == total::from_raw(detail::wide_uint<2>{1000} * e.raw()));
    // Offset (index raw, Lower ≠ 0) wide elements.
    std::vector<fine_signed> s{fine_signed{-1.5}, fine_signed{0.25}, fine_signed{tick}};
    EXPECT_TRUE(beman::inside::sum<fine_signed>(s) == fine_signed{(rational{-5, 4} + tick)});
    // Into a 64-bit target, by its policy: the total is checked once.
    using coarse = inside<{0, 100}, round_nearest>;
    EXPECT_TRUE(beman::inside::sum<coarse>(std::vector<fine>(3, fine{2.25})) == 7);
    using six = inside<{0, 6}>;
    EXPECT_EQ(beman::inside::sum<six>(std::vector<fine>(3, fine{2.25})).error(), errc::overflow);
}

TEST(WideGridTest, to_string) {
    EXPECT_EQ(to_string(fine{1.5}), to_string(rational{3, 2}));
    // 2^33 + 2^-32 = (2^65 + 1) / 2^32: the numerator passes 64 bits.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_EQ(to_string(odd), "8589934592.00000000023283064365386962890625");
    EXPECT_EQ(parse<fine>(to_string(odd)), odd);
    EXPECT_EQ(to_string(detail::wide_uint<2>{1} << 100), "1267650600228229401496703205376");
    EXPECT_NE(to_string_debug(odd).find("wide_uint<2>"), std::string::npos);
}

TEST(WideGridTest, parse) {
    EXPECT_TRUE(parse<fine>("1.5").value() == 1.5_r);
    // 2^33 + 2^-32 needs a 66-bit numerator: the exact fallback parses it.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_TRUE(parse<fine>("36893488147419103233/4294967296").value() == odd);
    EXPECT_TRUE(parse<fine>("8589934592.00000000023283064365386962890625").value() == odd);
    EXPECT_EQ(parse<fine>("1e40").error(), errc::overflow); // out of range
    EXPECT_EQ(parse<fine>("12x").error(), errc::invalid_format);
    // Round trip through to_string.
    EXPECT_TRUE(parse<fine>(to_string(odd)).value() == odd);
}

TEST(WideGridTest, hashing) {
    std::unordered_set<fine> set{fine{1}, fine{1.5}, fine{1}};
    EXPECT_EQ(set.size(), 2u);
    EXPECT_EQ(std::hash<fine>{}(fine{2}), std::hash<fine>{}(fine{2}));
}

TEST(WideGridTest, uniform_sampling) {
    std::mt19937_64 rng{1};
    bool            above_2_33 = false;
    for (int i = 0; i < 1000; ++i) {
        const fine v = uniform<fine>(rng);
        ASSERT_TRUE(v >= 0 && v <= p34);
        above_2_33 = above_2_33 || v > (std::uint64_t{1} << 33);
    }
    EXPECT_TRUE(above_2_33);
}

// Pinned like the 64-bit draws (sign_uniform.test.cpp): the same limbs on every
// standard library.
TEST(WideGridTest, uniform_draws_are_pinned_across_standard_libraries) {
    const auto expect = [](auto g, const std::array<std::array<std::uint64_t, 2>, 4>& want) {
        for (const auto& [hi, lo] : want) {
            const auto r = uniform<fine>(g).raw();
            EXPECT_EQ(r.Word[1], hi);
            EXPECT_EQ(r.Word[0], lo);
        }
    };
    expect(std::mt19937_64{42},
           {{{0x0u, 0xc151df7d6ee5e2d6u},
             {0x0u, 0x93170a1965d42420u},
             {0x1u, 0x461c9cf62eb9fcb6u},
             {0x0u, 0xd398c3c9b02b56cdu}}});
    expect(std::minstd_rand{42},
           {{{0x3u, 0x3fbdde24bd38d973u},
             {0x2u, 0x6f951d3b3ad29545u},
             {0x3u, 0x51a288b798c54832u},
             {0x0u, 0x3ad020608824cbe3u}}});
}

TEST(WideGridTest, division) {
    using divisor = inside<{{1, 4}, per<4>}>;
    fine a        = 3;
    auto q        = a / divisor{1.5}; // exact
    EXPECT_TRUE(detail::lift_unwrap(q) == 2);
    // (2^33 + 2^-32) / 1 has no 64-bit rational form: the quotient's
    // denominator bound sizes a fraction that holds it.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_TRUE(odd / divisor{1} == odd);
    // narrow ÷ wide, including a zero divisor.
    auto r = inside<{0, 10}>{3} / fine{0.5};
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(*r == 6);
    EXPECT_EQ((inside<{0, 10}>{3} / fine{0}).error(), errc::division_by_zero);
}

TEST(WideGridTest, modulo) {
    // uint64 differences span 2^65 values: integers past int64.
    constexpr std::uint64_t u64_max = ~std::uint64_t{0};
    auto                    d       = uint64{u64_max} - uint64{0};
    static_assert(detail::wide_index_storage<decltype(d)>);
    auto m = mod(d, inside<{1, 1000}>{7}, make_policy<round_toward_zero>());
    EXPECT_TRUE(m == (u64_max % 7));
    auto n = mod(-d, inside<{1, 1000}>{7}, make_policy<round_toward_zero>()); // takes the dividend's sign
    EXPECT_TRUE(n == -static_cast<std::int64_t>(u64_max % 7));
    // uint64 itself (a uint64 value raw past int64) now has a modulo too.
    EXPECT_TRUE((mod(uint64{u64_max}, inside<{1, 1000}>{10}, make_policy<round_toward_zero>()) == 5));
}

TEST(WideGridTest, clamp_and_wrap_actions) {
    // on_clamp: the overshoot is an inside for every source, as on the builtin
    // paths.
    fine         c                                                               = 0;
    std::int64_t over                                                            = 0;
    c.on_clamp([&](auto&, auto o) { over = test::unwrap(to<std::int64_t>(o)); }) = p34 + 5;
    EXPECT_TRUE(c == p34);
    EXPECT_EQ(over, 5);
    bool frac_ok                                                         = false;
    c.on_clamp([&](auto&, auto o) { frac_ok = (o == rational{-1, 2}); }) = rational{-1, 2};
    EXPECT_TRUE(c == 0);
    EXPECT_TRUE(frac_ok);
    bool got_inside = false;
    c.on_clamp([&](auto&, auto o) { got_inside = insidable<decltype(o)> && o == 6; }) =
        inside<{0, std::uint64_t{1} << 40}>{p34 + 6};
    EXPECT_TRUE(got_inside);

    // on_wrap: the carry is the number of full turns.
    fine         w     = 0;
    std::int64_t carry = 0;
    // One turn is 2^34 + 2^-32, so 3·2^34 + 2 = 3 turns + (2 − 3·2^-32).
    w.on_wrap([&](auto&, auto q) { carry = as<std::int64_t>(q); }) = 3 * p34 + 2;
    EXPECT_EQ(carry, 3);
    EXPECT_TRUE(w == rational{2} - rational{3} * tick);
}

// on_clamp and on_wrap on a continuous grid past 64 bits (fraction storage):
// clamp stores the limit and reports the overshoot, wrap folds by the span
// and reports the carry.
TEST(WideGridTest, hooks_on_a_wide_continuous_grid) {
    // (round_nearest: a sum of two B values may have a denominator past 3,
    // which a store through a hook alone cannot report.)
    using C = inside<{{-0x1p70, 0x1p70}, max_denominator{3}}, round_nearest>;
    using B = inside<{{-0x1p80, 0x1p80}, max_denominator{3}}>;
    static_assert(detail::fraction_storage<C>);

    // From an inside the overshoot is exact.
    C    x{0};
    bool exact = false;
    x.on_clamp([&](auto&, auto o) {
        static_assert(insidable<decltype(o)>);
        exact = (o == B{0x1p70} + B{rational{1, 3}});
    })         = B{0x1p71} + B{rational{1, 3}};
    EXPECT_TRUE(exact);
    EXPECT_TRUE(x == B{0x1p70});

    // From a double it is exact too, past the int64 range.
    bool from_double                                                  = false;
    x.on_clamp([&](auto&, auto o) { from_double = o == B{-0x1p70}; }) = -0x1p71;
    EXPECT_TRUE(x == B{-0x1p70});
    EXPECT_TRUE(from_double);

    // The period is the span, 2^71.
    std::int64_t carry = 7;
    x.on_wrap([&](auto&, auto k) {
        static_assert(insidable<decltype(k)>);
        carry = test::unwrap(to<std::int64_t>(k));
    })                 = B{0x1p71} + B{rational{1, 3}};
    EXPECT_EQ(carry, 1);
    EXPECT_TRUE((x == B{rational{1, 3}}));
    x.on_wrap([&](auto&, auto k) { carry = test::unwrap(to<std::int64_t>(k)); }) = -0x1p72;
    EXPECT_EQ(carry, -2);
    EXPECT_TRUE(x == B{0});
    x.on_wrap([&](auto&, auto k) { carry = test::unwrap(to<std::int64_t>(k)); }) = 0x1p72 + 0x1p70;
    EXPECT_EQ(carry, 3);
    EXPECT_TRUE(x == B{-0x1p70});
}

// A source past the 64-bit rational into a rational inside: both hooks.
TEST(WideGridTest, hooks_on_a_rational_inside_from_a_wide_source) {
    using R = inside<{{-10, 10}, max_denominator{4}}, round_nearest>; // (as above)
    using B = inside<{{-0x1p80, 0x1p80}, max_denominator{3}}>;
    static_assert(detail::rational_storage<R>);
    R    r{0};
    bool exact = false;
    r.on_clamp([&](auto&, auto o) {
        static_assert(insidable<decltype(o)>);
        exact = (o == B{0x1p70} - B{10});
    })         = B{0x1p70};
    EXPECT_TRUE(exact);
    EXPECT_TRUE(r == 10);

    // (2^70 + 1/3 + 10) / 20 turns: the carry past int64, exactly.
    bool carry                                                                   = false;
    r.on_wrap([&](auto&, auto k) { carry = k == just<59029581035870565171_g>; }) = B{0x1p70} + B{rational{1, 3}};
    EXPECT_TRUE(carry);
    EXPECT_TRUE((r == rational(13, 3)));
}

// The wrap carry grid is computed on grid numbers, so a grid past 64 bits has
// one: an int never leaves {-2^70, 2^70}, so its carry is always 0.
TEST(WideGridTest, wrap_carry_grid_past_64_bits) {
    using W = inside<{-0x1p70, 0x1p70}, wrap>;
    W            w{0};
    std::int64_t c = 7;
    w.on_wrap([&](auto&, auto k) {
        static_assert(grid_of<decltype(k)> == grid{0, 0});
        c = as<std::int64_t>(k);
    })             = 3;
    EXPECT_EQ(c, 7); // in range: no wrap
    EXPECT_TRUE(w == 3);
}

// A double past a wide grid's magnitude folds by its exact value, not a
// stand-in just past the grid (which gave 4 for -2^72 below).
TEST(WideGridTest, wrap_of_a_huge_double_folds_its_exact_value) {
    using N = inside<{-0x1p70, 0x1p70}, wrap>; // period 2^71 + 1
    EXPECT_TRUE(N{-0x1p72} == 2);
    EXPECT_TRUE(N{-0x1p71} == 1);
    EXPECT_TRUE(N{0x1p71} == -1);
    EXPECT_TRUE(N{0x1p72} == -2);
    using C = inside<{{-0x1p70, 0x1p70}, max_denominator{3}}, wrap>; // period 2^71
    using B = inside<{{-0x1p80, 0x1p80}, max_denominator{3}}>;
    EXPECT_TRUE(C{0x1p72 + 0x1p70} == B{-0x1p70});
}

// An operand past 64 bits whose sum with a continuous operand fits a 64-bit
// rational: the wide kernels add, and the sum stores as a rational.
TEST(WideGridTest, wide_operand_into_a_rational_sum) {
    using L = inside<{18446744073709551616_g, 18446744073709551620_g}>; // 2^64 … 2^64 + 4
    using R = inside<{{-18446744073709551616_g, -18446744073709551612_g}, max_denominator{3}}>;
    static_assert(detail::wide_valued<L>);
    static_assert(detail::rational_storage<decltype(L{} + R{})>);
    const L l{18446744073709551617_g};
    const R r{-18446744073709551616_g};
    EXPECT_TRUE(l + r == 1);
    EXPECT_TRUE(r + l == 1);
    EXPECT_TRUE(l - (-r) == 1);
}

// Limb boundaries, where the removed 128-bit helpers kept their edge cases:
// slot counts 2^64 − 1 (the last uint64 index) and 2^64 (the first two-limb
// one), a 128-bit count, and arithmetic that crosses from one to the other.
TEST(WideGridTest, limb_edges) {
    constexpr std::uint64_t u64_max = ~std::uint64_t{0};
    constexpr rational      q1{1, 4};                                                 // one notch
    using top64  = inside<{{rational{0}, rational{u64_max, 4}}, per<4>}>;             // 2^64 − 1 slots
    using over64 = inside<{{rational{0}, rational{std::uint64_t{1} << 62}}, per<4>}>; // 2^64 slots
    static_assert(std::is_same_v<top64::raw_type, std::uint64_t>);
    static_assert(std::is_same_v<over64::raw_type, detail::wide_uint<2>>);
    static_assert(grid_of<over64>.slot_bits() == 65);

    // The top slot of each round-trips.
    top64 t = rational{u64_max, 4};
    EXPECT_EQ(t.raw(), u64_max);
    over64 o = rational{std::uint64_t{1} << 62};
    EXPECT_TRUE(o.raw() == (detail::wide_uint<2>{1} << 64));
    EXPECT_TRUE((-t == -rational{u64_max, 4}));
    EXPECT_TRUE(-o == -rational{std::uint64_t{1} << 62});

    // += across the 64-bit index boundary, by a point delta and by a raw.
    over64 c = rational{u64_max, 4}; // index 2^64 − 1
    errc   ec{};                     // (a checked += names its channel)
    c.policy(ec) += just<1, 4>;      // index 2^64
    EXPECT_EQ(ec, errc{});
    EXPECT_TRUE(c.raw() == (detail::wide_uint<2>{1} << 64));
    c.policy(ec) += just<1, 4>; // past the top
    EXPECT_EQ(ec, errc::overflow);
    inside<{{rational{0}, rational{std::uint64_t{1} << 62}}, per<4>}, wrap> w = rational{std::uint64_t{1} << 62};
    w += just<1, 4>; // wraps to 0
    EXPECT_TRUE(w == 0);

    // Sums and differences across the boundary, both ends.
    auto s = o + inside<{{0, 1}, per<4>}>{1}; // 2^64 + 4 slots
    static_assert(detail::wide_index_storage<decltype(s)>);
    EXPECT_TRUE((s == rational{(std::uint64_t{1} << 62) + 1}));
    // (2^64 − 1)/4 + (2^64 − 1)/4: a big grid number.
    auto tt = t + t; // 2^65 − 1 slots
    EXPECT_TRUE((tt == rational{u64_max, 2}));
    auto d = top64{0} - t;
    EXPECT_TRUE((d == -rational{u64_max, 4}));
    EXPECT_TRUE(t - top64{q1} == (rational{u64_max, 4} - q1));

    // A 128-bit slot count: {−(2^64 − 1), 2^64 − 1} in steps of 1/(2^63 − 1).
    using full = inside<{{rational{u64_max, std::int64_t{-1}}, rational{u64_max}}, per<(std::uint64_t{1} << 63) - 1>}>;
    static_assert(std::is_same_v<full::raw_type, detail::wide_uint<2>>);
    static_assert(grid_of<full>.slot_bits() == 128);
    full lo = rational{u64_max, std::int64_t{-1}}, hi = rational{u64_max};
    EXPECT_TRUE(lo.raw() == detail::wide_uint<2>{0});
    constexpr auto full_top = static_cast<detail::wide_uint<2>>(grid_of<full>.step_count());
    EXPECT_TRUE(hi.raw() == full_top);
    EXPECT_TRUE(-lo == hi && lo < hi);
}

// Q-format division whose scaled dividend raw·N passes 64 bits (here 95):
// the native Q-format path runs in a wide work type instead of falling back
// to an exact fraction, so the result keeps the Q-format notch.
TEST(WideGridTest, qformat_division_past_64_bits) {
    using q32 = inside<{{0, (1ull << 31)}, per<(1ull << 32)>}, round_nearest>; // 2^63 slots
    q32  a = 1000.5, b = 0.25;
    auto q = (a / b).value();                              // b's grid includes 0
    static_assert(notch_of<decltype(q)> == notch_of<q32>); // Q-format result
    static_assert(!detail::rational_storage<decltype(q)>);
    EXPECT_TRUE(q == 4002);
    auto r = (q32{1} / q32{3}).value(); // rounds to the nearest 2^-32
    EXPECT_TRUE((r == rational{1431655765, std::int64_t{1} << 32}));
}

// The wide paths are constexpr.
namespace {
static_assert(fine{1.5_r} + fine{2} == 3.5_r);
static_assert(fine{1.5} < fine_signed{2});
static_assert(-fine{1} == -1);
static_assert([] {
    fine_clamp c = 0;
    c            = p34 + 1;
    return c == p34;
}());
} // namespace
} // namespace wide_grid_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;

template <class T>
concept has_epsilon = requires { std::numeric_limits<T>::epsilon(); };

// The same-notch fast path of a wide store took any equal notch, so an
// offset lattice (0.5 + k) shifted by a truncated 0.5: every mode floored.
TEST(BigGridTest, wide_store_from_an_offset_lattice) {
    using halves = inside<{{0.5, 10.5}, 1}>;  // Lower off the notch's multiples
    using neg    = inside<{{-3.5, -0.5}, 1}>; // ditto, below 0
    constexpr grid wide{{-0x1p63, 0x1p63}, 1};
    using ceil_t    = inside<wide, round_ceil>;
    using nearest_t = inside<wide, round_nearest>;
    using trunc_t   = inside<wide, round_toward_zero>;
    EXPECT_EQ(*ceil_t::try_make(halves{rational{3, 2}}), 2_ins);
    EXPECT_EQ(*nearest_t::try_make(halves{rational{3, 2}}), 2_ins);
    EXPECT_EQ(*trunc_t::try_make(neg{rational{-5, 2}}), -2_ins);
}

// numeric_limits built min/max/lowest/epsilon from the 64-bit rational: on
// a grid past 64 bits they did not compile.
TEST(BigGridTest, numeric_limits_of_a_big_grid) {
    using big = inside<{-1237940039285380274899124224_g, 1237940039285380274899124224_g}>; // ±2^90
    using far = inside<{1267650600228229401496703205376_g, 1267650600228229401496703205386_g}>;
    static_assert(std::numeric_limits<big>::lowest() < 0 && std::numeric_limits<big>::max() > 0);
    static_assert(std::numeric_limits<big>::epsilon() == 0);
    static_assert(!has_epsilon<far>); // 0 is no value of far
    EXPECT_EQ(to_string(std::numeric_limits<big>::lowest()), "-1237940039285380274899124224");
    EXPECT_EQ(to_string(std::numeric_limits<far>::max()), "1267650600228229401496703205386");
    EXPECT_EQ(to_string(std::numeric_limits<big>::max()), "1237940039285380274899124224");
}

// The predicates did not compile for a wide-valued inside argument.
TEST(BigGridTest, predicates_take_a_wide_inside) {
    using W = inside<{{-rational{1ll << 62}, rational{1ll << 62}}, rational{1, 1ll << 62}}>;
    using T = inside<{{0, 2}, rational{1, 1ll << 62}}>;
    const W w{rational{1, 4}};
    EXPECT_FALSE(conversion_overflows<T>(w));
    EXPECT_FALSE(conversion_rounds<T>(w));
    EXPECT_TRUE(conversion_overflows<T>(W{rational{-1, 4}}));
}

// The predicates of a grid past 64 bits at runtime: comparisons and the
// lattice test formed big values (std::terminate).
TEST(BigGridTest, predicates_at_run_time) {
    constexpr grid G{{0, 1267650600228229401496703205376_g}, 1}; // 2^100
    constexpr grid H{{1267650600228229401496703205376_g, 1267650600228229401496703205386_g}, 2};
    EXPECT_TRUE(G.representable(test::at_run_time(5.0)));
    EXPECT_FALSE(G.representable(test::at_run_time(2.5)));
    EXPECT_TRUE(G.representable(test::at_run_time(0x1p100)));
    EXPECT_FALSE(G.representable(test::at_run_time(0x1p101)));
    EXPECT_TRUE(H.representable(test::at_run_time(0x1p100)));
    EXPECT_FALSE(H.representable(test::at_run_time(5.0)));
    EXPECT_TRUE(includes(G.Interval, test::at_run_time(1e30)));
    EXPECT_FALSE(includes(G.Interval, test::at_run_time(-1.0)));
    EXPECT_TRUE(includes(G.Interval, test::at_run_time(7)));
    EXPECT_FALSE(includes(H.Interval, test::at_run_time(7)));
}

// sum's block loop over narrow raws held Lower and the notch in 64 bits: a
// grid near 2^70 with a uint8 raw lost cnt·2^70 (base truncated to 0), an
// unanchored one with notch 2^70 lost every raw·2^70 (scale 0).
TEST(BigGridTest, sum_of_narrow_raws_far_from_zero) {
    using X  = inside<{1180591620717411303424_g, 1180591620717411303524_g}>; // 2^70 + 0..100
    using Xu = inside<{{1, 1_g + 100_g * 1180591620717411303424_g}, 1180591620717411303424_g}>;
    using T  = inside<{0, 1208925819614629174706176_g}>; // 2^80
    static_assert(sizeof(X::raw_type) == 1 && sizeof(Xu::raw_type) == 1);
    constexpr std::array<X, 3> a{
        X{1180591620717411303424_g}, X{1180591620717411303425_g}, X{1180591620717411303524_g}};
    constexpr std::array<Xu, 2> b{Xu{1_g + 1180591620717411303424_g}, Xu{1_g + 2_g * 1180591620717411303424_g}};
    static_assert(sum<T>(a) == a[0] + a[1] + a[2]);
    static_assert(sum<T>(b) == b[0] + b[1]);
    const std::vector<X> v(1000, a[2]);
    EXPECT_EQ(sum<T>(v), T{1000_g * 1180591620717411303524_g});
}

// The out-of-range rounding of the predicates compared the slot with
// max_index_of, 0 past 2^64 slots: a value just past Upper that rounds onto
// it counted as overflow, not rounding.
TEST(BigGridTest, predicates_round_onto_a_grid_past_two_to_the_64_slots) {
    using T = inside<{{-18446744073709551615_g, 1}, per<4>}, round_nearest>; // 2^66 slots
    static_assert(!conversion_overflows<T>(1.0625) && conversion_rounds<T>(1.0625));
    static_assert(conversion_overflows<T>(1.25) && !conversion_rounds<T>(1.25));
    EXPECT_FALSE(conversion_overflows<T>(test::at_run_time(1.0625)));
    EXPECT_TRUE(conversion_rounds<T>(test::at_run_time(1.0625)));
}

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;

// A fraction raw past 64 bits hashes and prints its reduced parts.
TEST(BigGridsArmTest, hash_and_debug_string_of_a_fraction_raw) {
    using F = inside<{{0, 2}, max_denominator{1180591620717411303424_g}}>; // bound 2^70
    static_assert(detail::fraction_storage<F>);
    const F third = *parse_exact<F>("1/3");
    EXPECT_EQ(std::hash<F>{}(third), std::hash<F>{}(*parse_exact<F>("2/6")));
    EXPECT_EQ(to_string_debug(third).substr(0, 9), "1/3 {1/3[");
}

// uniform over 2^128 slots: the top limb is a whole word.
TEST(BigGridsArmTest, uniform_over_two_full_limbs) {
    using U = inside<{0, 340282366920938463463374607431768211455_g}>; // 2^128 − 1
    std::mt19937_64 g{1};
    const U         u = uniform<U>(g);
    EXPECT_TRUE(u >= 0);
}

// {-2^63, 2^63} needs a wide index (2^64 + 1 values), yet its values fit the
// 64-bit rational: times a continuous grid, the product's raw is the
// rational, read off the exact product.
TEST(BigGridsArmTest, wide_index_times_continuous_into_a_rational) {
    using L = inside<{-9223372036854775808_g, 9223372036854775808_g}>;
    using C = inside<{{0, 1}, max_denominator{1}}>;
    static_assert(detail::wide_index_storage<L> && detail::rational_storage<decltype(L{} * C{})>);
    EXPECT_EQ(to_string(L{std::numeric_limits<std::int64_t>::min()} * C{1}), "-9223372036854775808");
    EXPECT_EQ(to_string(*parse_exact<L>("9223372036854775808") * C{1}), "9223372036854775808");
}
} // namespace reached_arms_tests

// The whole public surface over six kinds of grid past 64 bits: a wide index,
// grid numbers past 64 bits, a notch of 2^-70, a fraction raw, uint64's
// range, and a signed wide grid. Every operation instantiates and runs, so
// a path that assumes 64 bits fails this build (the audit that found the
// last ones, 2026-11-07).
namespace sweep_tests {
using namespace beman::inside;
using detail::rational;

template <class X>
std::string text_of(const X& x) {
    if constexpr (insidable<X>)
        return to_string(x);
    else if constexpr (std::integral<X>)
        return std::to_string(static_cast<long long>(x));
    else
        return x ? text_of(*x) : std::string{"error"};
}

template <class T>
std::string sweep(const T a, const T b, std::mt19937_64& g) {
    using S  = inside<{0, 100}, round_nearest>;
    using O  = inside<{{0, 4}, per<1024>}, round_nearest | clamp>;
    using Ow = inside<{{0, 4}, per<1180591620717411303424_g>}, round_nearest | clamp>;
    const S     s{3};
    std::string r;
    T           c{a};
    c                          = b;
    c.template policy<clamp>() = s;
    r += text_of(a + b) + text_of(a - b) + text_of(a * b) + text_of(a / b) + text_of(-a) + text_of(a + s) +
         text_of(s * a) + text_of(a / s) + text_of(s / a);
    if constexpr (requires { a % b; })
        r += text_of(a % b) + text_of(a % s);
    r += (a == b || a < b || a == s || a < s || a == 5 || a < 7 || a == rational{1, 2}) ? "y" : "n";
    r += text_of(a.template to<int>()) + text_of(a.template to<std::uint64_t>());
    r += std::to_string(a.template to<double>()) + std::to_string(static_cast<double>(a.template to<long double>()));
    r += to_string(a.numerator()) + to_string(a.denominator());
    if constexpr (requires { a.index(); })
        r += to_string(T::from_index(a.index())) + to_string(step_count_of<T>);
    r += std::format("{} {:.3f} {:e} {:>40g}", a, a, a, a) + to_string_debug(a);
    if constexpr (detail::integer_lattice<T>)
        r += std::format("{:x} {:#b}", a, a);
    r += text_of(parse<T>("1")) + text_of(parse_exact<T>("1/2")) + std::to_string(std::hash<T>{}(a));
    r += to_string(std::numeric_limits<T>::max()) + to_string(std::numeric_limits<T>::lowest());
    if constexpr (detail::notched<T>) {
        r += to_string(uniform<T>(g)) + to_string(*values<T>.begin()) + to_string(*std::ranges::prev(values<T>.end()));
        auto t = cursor<T>{};
        r += to_string(*++t);
    }
    r += text_of(math::abs(a)) + text_of(math::floor(a)) + text_of(math::round(a)) + text_of(math::fmod(a, b));
    if constexpr (lower_of<T> >= 0)
        r += text_of(math::sqrt<O>(a)) + text_of(math::sqrt<Ow>(a));
    r += text_of(math::sin<O>(a)) + text_of(math::atan<O>(a)) + text_of(math::cos<Ow>(a)) + text_of(math::pown<3>(a));
    r += text_of(a * math::pi) + (a < math::pi ? "<" : ">");
    T z                                                          = a;
    z.template policy<clamp>()                                   = 1e300;
    z.template policy<wrap>()                                    = -5;
    z.on_wrap([&](auto&, auto carry) { r += to_string(carry); }) = a * a * a;
    z.on_clamp([&](auto&, auto over) { r += to_string(over); })  = a * a * a;
    z.template policy<wrap>() += b;
    z.template policy<wrap>() -= b;
    if constexpr (std::integral<__int128>) {
        z.template policy<clamp>() = (static_cast<__int128>(1) << 100) + 7;
        r += text_of(z.template to<__int128>());
    }
    return r + to_string(z);
}

TEST(BigGridsSweepTest, every_operation_past_64_bits) {
    std::mt19937_64 g{7};
    const auto      run = [&]<class T>(std::type_identity<T>, auto lo, auto hi) {
        EXPECT_FALSE(sweep<T>(test::store<T>(lo), test::store<T>(hi), g).empty());
    };
    using WI = inside<{0, 1180591620717411303424_g}, round_nearest>;
    using WN = inside<{1267650600228229401496703205376_g, 1267650600228229401496703205386_g}, round_nearest>;
    using WL = inside<{{0, 1}, 1_g / 1180591620717411303424_g}, round_nearest>;
    using WF = inside<{{0, 2}, max_denominator{1180591620717411303424_g}}, round_nearest>;
    using WU = inside<{0, 18446744073709551615_g}, round_nearest>;
    using WS = inside<{-1180591620717411303424_g, 1180591620717411303424_g}, round_nearest>;
    run(std::type_identity<WI>{}, just<1180591620717411303423_g>, just<7>);
    run(std::type_identity<WN>{}, just<1267650600228229401496703205380_g>, just<1267650600228229401496703205377_g>);
    run(std::type_identity<WL>{}, rational{1, 3}, rational{3, 4});
    run(std::type_identity<WF>{}, rational{1, 3}, rational{3, 4});
    run(std::type_identity<WU>{}, std::uint64_t{18446744073709551615u}, std::uint64_t{3});
    run(std::type_identity<WS>{}, just<-1180591620717411303423_g>, just<5>);
}
} // namespace sweep_tests
