// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bounded continuous grids: a continuous result whose denominators the grids
// bound (grid::MaxDenominator) holds every exact +, −, ×, ÷ result, so those
// ops return plain values instead of std::expected.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using num   = inside<{0, 100}>;
using den   = inside<{1, 7}>;
using quot  = decltype(num{} / den{});
using C     = inside<{{0, 10}, 0}>; // declared continuous: unbounded
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

TEST(BoundedRationalTest, unbounded_continuous_grids_still_report) {
    static_assert(is_expected_v<decltype(C{} + C{})>);
    static_assert(is_expected_v<decltype(C{} * C{})>);
    static_assert(is_expected_v<decltype(quot{} + C{})>); // one unbounded operand
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
#if BEMAN_INSIDE_BIG_GRIDS
    static_assert(!is_expected_v<b3>); // a fraction sized for 10^27
    static_assert(fraction_storage<b3>);
#else
    static_assert(is_expected_v<b3>); // past 64 bits: unbounded, checked
#endif
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
