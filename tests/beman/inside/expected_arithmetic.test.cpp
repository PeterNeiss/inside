// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/detail/rational.hpp>

#include <gtest/gtest.h>

#include <expected>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using exp_r = std::expected<rational, errc>;

// Fallible-result stand-ins: std::expected travels only as a return value or
// parameter, so the tests build operands with these instead of storing them.
constexpr exp_r good(rational r) { return r; }
constexpr exp_r bad(errc e = errc::overflow) { return std::unexpected{e}; }
} // namespace

// expected<rational> * expected<rational>
TEST(ExpectedArithmeticTest, expected_rational_times_expected_rational) {
    auto r = good(0.75_r) * good(rational{2, 3});
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 0.5_r);

    ASSERT_FALSE((bad() * good(1_r)).has_value());
    ASSERT_FALSE((good(1_r) * bad()).has_value());
    ASSERT_FALSE((bad() * bad()).has_value());
}

// expected<rational> + expected<rational>
TEST(ExpectedArithmeticTest, expected_rational_plus_expected_rational) {
    auto r = good(0.25_r) + good(0.5_r);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 0.75_r);

    ASSERT_FALSE((bad() + good(0.25_r)).has_value());
    ASSERT_FALSE((good(0.25_r) + bad()).has_value());
}

// expected<rational> - expected<rational>
TEST(ExpectedArithmeticTest, expected_rational_minus_expected_rational) {
    auto r = good(0.75_r) - good(0.5_r);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 0.25_r);
}

// expected<rational> / expected<rational>, with the division_by_zero cause
TEST(ExpectedArithmeticTest, expected_rational_div_expected_rational) {
    auto r = good(0.75_r) / good(0.5_r);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 1.5_r);

    auto z = good(0.75_r) / good(rational{0});
    ASSERT_FALSE(z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}

// The first error short-circuits and keeps its cause.
TEST(ExpectedArithmeticTest, first_error_keeps_its_cause) {
    auto r = bad(errc::division_by_zero) + bad(errc::overflow);
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::division_by_zero);

    auto chained = (good(1_r) / good(rational{0})) * good(2_r) + good(3_r);
    ASSERT_FALSE(chained.has_value());
    ASSERT_EQ(chained.error(), errc::division_by_zero);
}

// expected<rational> op arithmetic and symmetric
TEST(ExpectedArithmeticTest, expected_rational_op_arithmetic_and_symmetric) {
    ASSERT_EQ(*(good(0.5_r) + 1), 1.5_r);
    ASSERT_EQ(*(1 + good(0.5_r)), 1.5_r);
    ASSERT_EQ(*(good(0.5_r) - 1), -0.5_r);
    ASSERT_EQ(*(1 - good(0.5_r)), 0.5_r);
    ASSERT_EQ(*(good(0.5_r) * 2), (rational{1, 1}));
    ASSERT_EQ(*(2 * good(0.5_r)), (rational{1, 1}));
    ASSERT_EQ(*(good(0.5_r) / 2), 0.25_r);
    ASSERT_EQ(*(2 / good(0.5_r)), (rational{4, 1}));

    ASSERT_FALSE((bad() + 1).has_value());
    ASSERT_FALSE((1 + bad()).has_value());
}

// unary -expected<rational>
TEST(ExpectedArithmeticTest, unary_expected_rational) {
    auto r = -good(0.75_r);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, -0.75_r);

    auto e = -bad(errc::division_by_zero);
    ASSERT_FALSE(e.has_value());
    ASSERT_EQ(e.error(), errc::division_by_zero);
}

// rational overflow reports errc::overflow
TEST(ExpectedArithmeticTest, rational_overflow_reports_overflow) {
    const rational huge{std::numeric_limits<umax>::max()};
    auto           r = huge * huge;
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::overflow);
}

// inside construction from expected<rational> - sink unwrap
TEST(ExpectedArithmeticTest, inside_construction_from_expected_rational_sink_unwrap) {
    using b_t = inside<{{0, 1}, per<16>}, round_nearest>;

    b_t v{good(0.5_r)};
    ASSERT_EQ(rational{v}, 0.5_r);

    // An error goes through the policy, as a failed store does: inside_error
    // with the errc, never std::bad_expected_access.
    try {
        (void)b_t{bad(errc::division_by_zero)};
        FAIL() << "no error reported";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::division_by_zero);
    }
}

// inside operator= from expected<rational>
TEST(ExpectedArithmeticTest, inside_operator_from_expected_rational) {
    using b_t = inside<{{0, 1}, per<16>}, round_nearest>;

    b_t v{0};
    v = good(0.25_r);
    ASSERT_EQ(rational{v}, 0.25_r);

    // An error goes through the policy and leaves v unchanged.
    try {
        v = bad();
        FAIL() << "no error reported";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::overflow);
    }
    ASSERT_EQ(rational{v}, 0.25_r);
}

// A division whose divisor grid holds 0 stored into a clamp type: the zero
// divisor is reported (clamp handles the range, not the error), whether the
// expected is constructed from or assigned.
TEST(ExpectedArithmeticTest, zero_divisor_into_a_clamp_type_is_reported) {
    inside<{0, 10}> a{4};
    inside<{0, 2}>  b{0};
    using c_t = inside<{0, 100}, clamp | round_nearest>;
    try {
        c_t c = a / b;
        (void)c;
        FAIL() << "no error reported";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::division_by_zero);
    }
    c_t d{7};
    EXPECT_THROW(d = a / b, inside_error);
    EXPECT_EQ(rational{d}, 7);
    // a nonzero divisor stores (rounded and clamped by c_t)
    b = 2;
    d = a / b;
    EXPECT_EQ(rational{d}, 2);
}

// rational's sinks raise too: the expected-converting ctor, the compound
// assignments and a comparison against an expected.
TEST(ExpectedArithmeticTest, rational_sinks_raise_inside_error) {
    try {
        const rational r = bad(errc::division_by_zero);
        (void)r;
        FAIL() << "no error reported";
    } catch (const inside_error& e) {
        EXPECT_EQ(e.Code, errc::division_by_zero);
    }
    rational a{1};
    EXPECT_THROW(a += bad(), inside_error);
    EXPECT_THROW((void)(a < bad()), inside_error);
    EXPECT_EQ(a, 1);
}

// expected<inside> + rational propagates the error
TEST(ExpectedArithmeticTest, expected_inside_plus_rational_propagates_error) {
    using b_t    = inside<{{0, 1}, per<16>}, round_nearest>;
    auto some    = []() -> std::expected<b_t, errc> { return b_t{0.5_r}; };
    auto failing = []() -> std::expected<b_t, errc> { return std::unexpected{errc::division_by_zero}; };

    auto r = some() + 0.25_r;
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 0.75_r);

    ASSERT_FALSE((failing() + 0.25_r).has_value());
    ASSERT_FALSE((0.25_r + failing()).has_value());
    ASSERT_FALSE((failing() - 0.25_r).has_value());
    ASSERT_FALSE((failing() * 0.25_r).has_value());
    ASSERT_FALSE((failing() / 0.25_r).has_value());
    ASSERT_EQ((failing() * 0.25_r).error(), errc::division_by_zero);
}

// expected<rational> value_or
TEST(ExpectedArithmeticTest, expected_rational_value_or) {
    ASSERT_EQ(bad().value_or(7_r), 7_r);
    ASSERT_EQ(good(0.5_r).value_or(7_r), 0.5_r);
}
