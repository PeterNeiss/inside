// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bounded continuous grids: a continuous result whose denominators the grids
// bound (grid::MaxDenominator) holds every exact +, −, ×, ÷ result, so those
// ops return plain values instead of std::expected.

#include <beman/inside/math.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using num   = inside<{0, 100}>;
using den   = inside<{1, 7}>;
using quot  = decltype(num{} / den{});
using half  = inside<{{-6, 6}, per<2>}>;
using small = inside<{1, 5}>;
} // namespace

TEST(BoundedRationalTest, quotient_of_notched_grids_is_bounded_and_plain) {
    static_assert(grid_of<quot>.MaxDenominator == 7);
    static_assert(rational_storage<quot>);
    static_assert(!is_expected_v<quot>);
    static_assert(!is_expected_v<decltype(quot{} * 9_ins / 5_ins + 32_ins)>);
    static_assert(!is_expected_v<decltype(quot{} + quot{} - quot{} * quot{})>);
    static_assert(!is_expected_v<decltype(quot{} / den{})>);

    const auto q = num{50} / den{3};
    EXPECT_EQ(rational{q}, (rational{50, 3}));
    EXPECT_EQ(rational{q * 9_ins / 5_ins + 32_ins}, 62);
    EXPECT_EQ(rational{q + q - q * q}, (rational{-2200, 9}));
    EXPECT_EQ(rational{q / den{7}}, (rational{50, 21}));
}

TEST(BoundedRationalTest, declared_bound_makes_arithmetic_plain) {
    using thousandths = inside<{interval{0, 1}, max_denominator{1000}}>;
    static_assert(!is_expected_v<decltype(thousandths{} + thousandths{})>);
    static_assert(!is_expected_v<decltype(thousandths{} * thousandths{})>);
    const thousandths a{rational{1, 1000}}, b{rational{999, 1000}};
    EXPECT_EQ(rational{a * b}, (rational{999, 1'000'000}));
    EXPECT_EQ(rational{a + b}, 1);
}

TEST(BoundedRationalTest, bounds_past_storage_report_in_cxx23_and_fit_in_cxx26) {
    using billionths = inside<{interval{0, 1}, max_denominator{1'000'000'000}}>;
    using b2         = decltype(billionths{} * billionths{});
    static_assert(!is_expected_v<b2>); // 10^18 fits the 64-bit rational
    using b3 = decltype(b2{} * billionths{});
    static_assert(!is_expected_v<b3>); // a fraction sized for 10^27
    static_assert(fraction_storage<b3>);
    // (A value whose cube still fits 64 bits, so both modes have one.)
    const billionths x{rational{1, 1000}};
    const auto       x3 = lift_unwrap(x * x * x);
    EXPECT_TRUE(x3 == rational(1, 1'000'000'000));
    EXPECT_TRUE(x3 == lift_unwrap(x * (x * x)));
}

// Every value of small grids against the checked 64-bit rational: the
// unchecked kernels the bound selects give the exact result.
TEST(BoundedRationalTest, sweep_matches_checked_rational) {
    using q_t = decltype(half{} / small{});
    static_assert(grid_of<q_t>.MaxDenominator == 10 && !is_expected_v<q_t>);
    for (auto x : values<half>)
        for (auto y : values<small>) {
            const q_t      q  = x / y;
            const rational qr = (rational{x} / rational{y}).value();
            ASSERT_EQ(rational{q}, qr);
            ASSERT_EQ(rational{q + q}, (qr + qr).value());
            ASSERT_EQ(rational{q - x}, (qr - rational{x}).value());
            ASSERT_EQ(rational{q * q}, (qr * qr).value());
            ASSERT_EQ(rational{q / y}, (qr / rational{y}).value());
            ASSERT_EQ(rational{(q + q) * q - q / y},
                      (((qr + qr).value() * qr).value() - (qr / rational{y}).value()).value());
        }
}

// A bounded grid holds only values within its bound: a denominator past it
// is off the grid, like a value off a notched grid's lattice.
TEST(BoundedRationalTest, store_past_the_bound_reports_rounding_error) {
    using thousandths = inside<{interval{0, 1}, max_denominator{1000}}>;
    EXPECT_TRUE(thousandths::try_make(rational{7, 1000}).has_value());
    EXPECT_EQ(thousandths::try_make(rational{1, 1001}).error(), errc::rounding_error);
    EXPECT_EQ(parse<thousandths>("0.0001").error(), errc::rounding_error);
    EXPECT_THROW((thousandths{rational{1, 1001}}), inside_error);
    static_assert(!thousandths::try_make(rational{1, 1001}).has_value()); // also at compile time

    // Compound assignment stores back through the same check.
    thousandths x{rational{1, 10}};
    x *= thousandths{rational{1, 10}}; // 1/100: within the bound
    EXPECT_TRUE(x == rational(1, 100));
    EXPECT_THROW((x *= thousandths{rational{1, 100}}), inside_error); // 1/10000

    // A quotient type's bound is what the grids allow, so its own results fit.
    auto q = num{1} / den{7};
    q      = num{3} / den{7};
    EXPECT_TRUE(q == rational(3, 7));

    // unsafe: nothing reported; its snap truncates onto the bound, as it
    // truncates onto a notch.
    inside<{interval{0, 1}, max_denominator{1000}}, unsafe> u{rational{1, 1001}};
    EXPECT_TRUE(u == 0);
}

// The same check on a bounded grid stored as a wide fraction.
TEST(BoundedRationalTest, wide_bounded_store_checks_the_bound) {
    constexpr grid e18{interval{0, 1}, max_denominator{1'000'000'000'000'000'000}};
    using wide = inside<e18 * e18>; // denominators ≤ 10^36
    static_assert(fraction_storage<wide>);
    const auto ok = parse<wide>("0.000000000000000000000000000000000001"); // 10^-36
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(parse<wide>("0.0000000000000000000000000000000000001").error(), errc::rounding_error); // 10^-37
    // A rounding policy rounds onto the bound instead: 10^-37 floors to 0, ceils to 10^-36.
    EXPECT_TRUE((parse<wide, round_floor>("0.0000000000000000000000000000000000001").value() == 0));
    EXPECT_TRUE((parse<wide, round_ceil>("0.0000000000000000000000000000000000001").value() == *ok));
}

// Exact math outputs on continuous inputs keep the bound.
TEST(BoundedRationalTest, abs_copysign_fmod_keep_the_bound) {
    using signed_quot = decltype(inside<{-100, 100}>{} / den{});
    const signed_quot q{rational{-50, 3}};
    const auto        a = math::abs(q);
    static_assert(grid_of<std::remove_cvref_t<decltype(a)>>.MaxDenominator == 7);
    EXPECT_TRUE(a == rational(50, 3));
    const auto c = math::copysign(q, 1_ins);
    static_assert(grid_of<std::remove_cvref_t<decltype(c)>>.MaxDenominator == 7);
    EXPECT_TRUE(c == rational(50, 3));
    const auto r = math::fmod(q, inside<{1, 4}>{3}); // −50/3 + 5·3 = −5/3
    static_assert(grid_of<std::remove_cvref_t<decltype(r)>>.MaxDenominator == 7);
    EXPECT_TRUE(lift_unwrap(r) == rational(-5, 3));
}

// A bounded continuous result holds every exact sum and product: where the
// 64-bit kernels' intermediates pass 64 bits (each operand over D_L·D_R
// here), the op computes exactly in wide integers. No check, no expected.
TEST(BoundedRationalTest, bounded_results_are_total_past_64_bit_intermediates) {
    using L = inside<{{1ull << 40, (1ull << 40) + 1}, max_denominator{4096}}>;
    using R = inside<{{-((1ll << 40) + 1), -(1ll << 40)}, max_denominator{4096}}>;
    constexpr L    a{(rational{1ull << 40} + rational{1, 3}).value()};
    constexpr R    b{-(1ll << 40)};
    constexpr auto s = a + b;
    static_assert(insidable<decltype(s)>); // a plain value
    static_assert(s == rational{1, 3});
    EXPECT_EQ(static_cast<rational>(a - (-b)), (rational{1, 3}));
}

// Fractions past BEMAN_INSIDE_MAX_FRACTION_BITS have no type: the operator
// is deleted, so a probe answers, and mul_into rounds the product instead.
template <class A, class B>
constexpr bool can_mul = requires(A a, B b) { a * b; };
template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };

TEST(BoundedRationalTest, results_past_the_fraction_cap_have_no_type) {
    using billionths = inside<{interval{0, 1}, max_denominator{1'000'000'000}}>;
    using b2         = decltype(billionths{} * billionths{});
    using b4         = decltype(b2{} * b2{});
    using b8         = decltype(b4{} * b4{});
    using b16        = decltype(b8{} * b8{}); // denominators to 10^144
    static_assert(can_mul<b8, b8> && can_add<b16, billionths>);
    static_assert(!can_mul<b16, b16> && !can_add<b16, b16>); // 10^288: past 1024 bits
    using cent      = inside<{{0, 1}, per<100>}, round_nearest>;
    const b16 tenth = rational{1, 10};
    EXPECT_EQ(mul_into<cent>(tenth, tenth), rational(1, 100));
}
