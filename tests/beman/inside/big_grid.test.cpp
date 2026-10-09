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
using detail::rational;

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
    static_assert(detail::big_valued<far> && detail::exact_valued<far>);
    static_assert(!detail::big_valued<inside<{0, 100}>>);
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
    #endif
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

#else
TEST(BigGridTest, needs_cxx26_reflection) { GTEST_SKIP() << "C++26 static reflection unavailable"; }
#endif
