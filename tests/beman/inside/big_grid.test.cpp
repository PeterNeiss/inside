// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids whose limits or notch pass 64 bits (C++26 static reflection): storage,
// construction and assignment, comparison, arithmetic with result grids past
// 64 bits, conversions and io. Skipped without reflection.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>

#include <gtest/gtest.h>

#include <version> // __cpp_lib_format
#ifdef __cpp_lib_format
    #include <format>
#endif

#if BEMAN_INSIDE_BIG_GRIDS
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
constexpr huge::raw_type raw_2_65_2  = static_cast<huge::raw_type>(big_int{~umax{0}} * big_int{2});
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
    EXPECT_TRUE(h < 6 && h > 4.5);
    far f{huge::from_raw(raw_2_100_3)};
    EXPECT_TRUE(f > h);
    EXPECT_TRUE(f.raw() == 3);
    fine x = 0.5;
    EXPECT_TRUE(x == 0.5);
    EXPECT_TRUE(x.raw() == (detail::wide_uint<2>{1} << 79));
    // Values past 64 bits compare exactly.
    EXPECT_TRUE(f == huge::from_raw(raw_2_100_3));
}

TEST(BigGridTest, assignment_policies) {
    huge h = 0;
    EXPECT_THROW(h = -1, inside_error);
    EXPECT_THROW(h = 1e31, inside_error); // past 2^100
    inside<{0, g(pow2(100))}, clamp> c = 0;
    c                                  = 1e31;
    EXPECT_TRUE(c == huge::from_raw(raw_2_100));
    inside<{{0, 1}, grid_rational{big_int{1}, pow2(80)}}, round_nearest> r = 0;
    r = rational{1, 3}; // rounds to the nearest 2^-80
    const bool close =
        r > (rational{1, 3} - rational{1, 1 << 30}).value() && r < (rational{1, 3} + rational{1, 1 << 30}).value();
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
    // qword + qword: its upper bound 2^65 − 2 needs big grid numbers.
    constexpr umax kUM = ~umax{0};
    auto           q   = qword{kUM} + qword{kUM};
    EXPECT_TRUE(q == huge::from_raw(raw_2_65_2));
}

TEST(BigGridTest, conversions_and_io) {
    far f{huge::from_raw(raw_2_100_3)};
    EXPECT_EQ(to_string(f), "1267650600228229401496703205379");
    EXPECT_EQ(f.to<int>().error(), errc::overflow);
    EXPECT_DOUBLE_EQ(f.to<double>().value(), 0x1p100);
    EXPECT_TRUE(from_chars<huge>("1267650600228229401496703205376").value() == huge::from_raw(raw_2_100));
    fine x = 0.25;
    EXPECT_EQ(to_string(x), to_string(rational{1, 4}));
    // Exact decimals of any length, for values and grid numbers.
    EXPECT_EQ(to_string(fine::from_raw(1)),
              "0.00000000000000000000000082718061255302767487140869206996285356581211090087890625");
    EXPECT_EQ(to_string(grid_of<inside<{{0, 1}, 1e-30_g}>>), "{[0..1], 0.000000000000000000000000000001}");
    constexpr grid_rational third_80{big_int{1}, pow2(80) * big_int{3}};
    EXPECT_EQ(to_string(third_80), "1/3626777458843887524118528");
    #ifdef __cpp_lib_format
    EXPECT_EQ(std::format("{}", f), "1267650600228229401496703205379");
    EXPECT_EQ(std::format("{}", fine::from_raw(1)), to_string(fine::from_raw(1)));
    EXPECT_EQ(std::format("{:.3e} {:.2f}", f, f), "1.268e+30 1267650600228229401496703205379.00");
    #endif
    using micro = inside<{{0, 1}, 1e-30_g}>; // a decimal notch: 30 decimals
    EXPECT_EQ(to_string(micro{0.5}), "0.500000000000000000000000000000");
}

TEST(BigGridTest, grid_number_literal) {
    // _g spells grid numbers of any size, exactly — including decimal notches.
    static_assert(std::is_same_v<inside<{0, 1267650600228229401496703205376_g}>, huge>);
    using micro30 = inside<{{0, 1}, 1e-30_g}>;          // a 10^-30 notch
    static_assert(grid_of<micro30>.slot_bits() == 100); // 10^30 slots
    micro30 m = 0.5;
    EXPECT_TRUE(m == 0.5);
    static_assert(123'456'789'012'345'678'901'234'567'890_g ==
                  grid_rational{big_int{123'456'789'012'345'678ull} * big_int{1'000'000'000'000ull} +
                                big_int{901'234'567'890ull}});
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
    EXPECT_EQ(math::abs(x), 2.75);
    EXPECT_EQ(math::abs(h), 7);
    EXPECT_EQ(math::sign(x), -1);
    EXPECT_EQ(math::copysign(fine_r{1.5}, x), -1.5);
    EXPECT_EQ(math::floor(x), -3);
    EXPECT_EQ(math::ceil(x), -2);
    EXPECT_EQ(math::round(x), -3);
    EXPECT_EQ(math::trunc(x), -2);
    EXPECT_EQ(math::fmod(x, fpos{1.5}), -1.25);
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
    EXPECT_DOUBLE_EQ(detail::as_double(big), 0x1p100);
    EXPECT_DOUBLE_EQ(detail::as_double(tiny), 1.616255e-35);
    EXPECT_EQ(to_string(tiny), "0.00000000000000000000000000000000001616255");
    // Stored into a grid; added as a point delta.
    huge h = big;
    EXPECT_TRUE(h == big);
    h = 0;
    h += just<g(pow2(99))>;
    h += just<g(pow2(99))>;
    EXPECT_TRUE(h == big);
    using planck = inside<{{0, 1}, 1e-41_g}>;
    planck p     = tiny;
    EXPECT_TRUE(p == tiny);
}

TEST(BigGridTest, division) {
    // Big ÷ big: the exact quotient, reported when it passes the 64-bit rational.
    const far  a{huge::from_raw(raw_2_100_3)};
    const huge b{huge::from_raw(raw_2_100)};
    const auto q = a / b; // (2^100 + 3) / 2^100: exact in the quotient's wide fraction
    ASSERT_TRUE(q.has_value());
    EXPECT_EQ(
        to_string(*q),
        "1.0000000000000000000000000000023665827156630354162351856958483586890196193053270690143108367919921875");
    const auto r = far{huge::from_raw(raw_2_100)} / b;
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(*r == 1);
    const auto s = b / huge{4};
    ASSERT_TRUE(s.has_value());
    constexpr huge::raw_type raw_2_98 = static_cast<huge::raw_type>(pow2(98));
    EXPECT_TRUE(*s == huge::from_raw(raw_2_98));
    const auto t = huge{12} / huge{8};
    ASSERT_TRUE(t.has_value());
    EXPECT_TRUE(*t == 1.5);
    using micro  = inside<{{0, 1}, 1e-30_g}>;
    const auto u = micro{0.5} / micro{0.25};
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(*u == 2);
}

TEST(BigGridTest, continuous_past_64_bits) {
    // A quotient of big-grid values holds its exact value: a wide fraction raw.
    using length          = inside<{{1e-41_g, 1e27_g}, 1e-41_g}, round_nearest>;
    const length planck   = *from_chars<length>("1.616255e-35");
    const length universe = *from_chars<length>("8.8e26");
    const auto   ratio    = universe / planck;
    ASSERT_TRUE(ratio.has_value());
    static_assert(detail::fraction_storage<std::remove_cvref_t<decltype(*ratio)>>);
    EXPECT_EQ(to_string(*ratio), "17600000000000000000000000000000000000000000000000000000000000000000/323251");
    EXPECT_TRUE(*ratio > 5.4e61 && *ratio < 5.5e61);
    #ifdef __cpp_lib_format
    EXPECT_EQ(std::format("{:.6e}", *ratio), "5.444685e+61");
    #endif
    using decade = inside<{{-100, 100}, per<1 << 20>}, round_nearest>;
    EXPECT_EQ(math::log10_into<decade>(*ratio), (decade{rational{64'734'859, 1 << 20}})); // 61.7359724…
    const auto one = universe / universe;
    EXPECT_TRUE(*one == 1);
    EXPECT_EQ(to_string(*(planck / length{2})), "0.000000000000000000000000000000000008081275");

    // Declared directly, with every policy.
    using big_real = inside<{{0, 1e30_g}, 0}>;
    static_assert(detail::fraction_storage<big_real>);
    big_real b = rational{1, 3};
    EXPECT_EQ(to_string(b), "1/3");
    b = *from_chars<big_real>("123456789012345678901234567890/11");
    EXPECT_EQ(to_string(b), "123456789012345678901234567890/11");
    EXPECT_THROW(b = -1, inside_error);
    inside<{{0, 1e30_g}, 0}, clamp> c = 0;
    c                                 = huge::from_raw(raw_2_100);
    EXPECT_EQ(to_string(c), "1000000000000000000000000000000");
    inside<{{0, 1e30_g}, 0}, wrap> w = 0;
    w                                = far{huge::from_raw(raw_2_100_3)}; // 2^100 + 3 − 10^30
    EXPECT_EQ(to_string(w), "267650600228229401496703205379");
    // + and × hold the exact value, and report a fraction past the raw.
    const auto s = b + b;
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(to_string(*s), "246913578024691357802469135780/11");
    const auto p = b * big_real{rational{1, 2}};
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(to_string(*p), "61728394506172839450617283945/11");
    // 1/(2^250 + 1) fits big_real's raw; its square does not fit the product's.
    const big_real x =
        *from_chars<big_real>("1/1809251394333065553493296640760748560207343510400633813116524750123642650625");
    EXPECT_EQ((x * x).error(), errc::overflow);
}

#else
TEST(BigGridTest, needs_cxx26_reflection) { GTEST_SKIP() << "C++26 static reflection unavailable"; }
#endif
