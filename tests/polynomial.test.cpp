// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polynomials (units/polynomial.hpp): substitute, derivative, antiderivative and divmod
// on sums of powers of polynomial variables, exact, with every coefficient's range proved.

#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/polynomial.hpp>

#include <gtest/gtest.h>

#include <concepts>
#include <expected>
#include <type_traits>

namespace polynomial_tests {
using namespace ins;

namespace {
inline constexpr auto x = basis_vector<"x", polynomial>;
inline constexpr auto y = basis_vector<"y", polynomial>;
inline constexpr auto e1 = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>;
inline constexpr auto e2 = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>;

using digit = id<"[0..10]">;
using small = id<"[0..3]">;

// 3 + 2 x + x²
inline constexpr auto p = 3_q + 2_q * x + x * x;
} // namespace

TEST(PolynomialTest, substitute_evaluates_by_horner) {
    EXPECT_TRUE(substitute(p, x, 2_q) == 11);
    EXPECT_TRUE(substitute(p, x, -1_q) == 2);
    // The value's range follows from the coefficients' and the argument's: c₀ + c₁ a in [0, 40].
    const auto line = digit{4} + digit{5} * x;
    const auto v    = substitute(line, x, small{2});
    static_assert(lower_of<decltype(v)> == grid_number{0} && upper_of<decltype(v)> == grid_number{40});
    EXPECT_TRUE(v == 14);
    // Coefficients with units give a quantity.
    const auto s = 2_q * si::metre + 3_q * si::metre * x;
    EXPECT_TRUE(substitute(s, x, 4_q) == 14_q * si::metre);
    // A constant has no x: itself.
    EXPECT_TRUE(substitute(7_q, x, 2_q) == 7);
}

TEST(PolynomialTest, substitute_composes) {
    // p(y + 1) = 6 + 4 y + y².
    EXPECT_TRUE(substitute(p, x, y + 1_q) == 6_q + 4_q * y + y * y);
    // In two variables, the other stays: (x + y)² at x = 2 is 4 + 4 y + y².
    const auto b = x + y;
    EXPECT_TRUE(substitute(b * b, x, 2_q) == 4_q + 4_q * y + y * y);
    // Vector coefficients: the curve e₁ + t e₂ at t = 3.
    const auto c = e1 + x * e2;
    EXPECT_TRUE(substitute(c, x, 3_q) == e1 + 3_q * e2);
}

TEST(PolynomialTest, derivative_and_antiderivative) {
    EXPECT_TRUE(derivative(p, x) == 2_q + 2_q * x);
    static_assert(std::same_as<decltype(derivative(5_q, x)), multivector<>>);
    // The product rule: (p q)' = p' q + p q'.
    const auto q = 1_q - 3_q * x * x * x;
    EXPECT_TRUE(derivative(p * q, x) == derivative(p, x) * q + p * derivative(q, x));
    // The antiderivative has constant term 0 and its derivative is p.
    const auto F = antiderivative(p, x);
    EXPECT_EQ(to_string(F), "3 x + 1 x² + 1/3 x³");
    EXPECT_TRUE(derivative(F, x) == p);
    // ∫₀² p dx = F(2) - F(0) = 6 + 4 + 8/3 = 38/3.
    EXPECT_TRUE((substitute(F, x, 2_q) - substitute(F, x, 0_q) == "38/3"_q));
    // In a variable p does not have: 0, and p y.
    static_assert(std::same_as<decltype(derivative(p, y)), multivector<>>);
    EXPECT_TRUE(antiderivative(p, y) == p * y);
    // With units: the derivative of a position in m is in m (x is a number).
    const auto s = 2_q * si::metre + 3_q * si::metre * x;
    EXPECT_TRUE(derivative(s, x) == 3_q * si::metre);
    // Negative powers: (1/x)' = -1/x².
    EXPECT_TRUE(derivative(1_q / x, x) == -1_q / (x * x));
}

TEST(PolynomialTest, divmod_divides_with_a_remainder) {
    // x² + 2 x + 3 = (x + 1)(x + 1) + 2.
    const auto d       = 1_q + x;
    const auto [q, r]  = divmod(p, d);
    EXPECT_TRUE(q == 1_q + x);
    EXPECT_TRUE(r == 2);
    // A leading coefficient of 2: exact fractions.
    const auto d2      = 1_q + 2_q * x;
    const auto [q2, r2] = divmod(p, d2);
    EXPECT_TRUE((q2 == 0.75_q + 0.5_q * x));
    EXPECT_TRUE((r2 == 2.25_q));
    EXPECT_TRUE(q2 * d2 + r2 == p);
    // A lower degree: q = 0, r = p.
    const auto [q3, r3] = divmod(d, p);
    static_assert(std::same_as<std::remove_const_t<decltype(q3)>, multivector<>>);
    EXPECT_TRUE(r3 == d);
    // Single terms are polynomials too: 3 x² / x = 3 x.
    const auto [q4, r4] = divmod(3_q * x * x, x);
    EXPECT_TRUE(q4 == 3_q * x);
    static_assert(std::same_as<std::remove_const_t<decltype(r4)>, multivector<>>);
    // Coefficients with units: (6 m + 4 m x) / (2 + 2 x) = 2 m, remainder 2 m.
    const auto [q5, r5] = divmod(6_q * si::metre + 4_q * si::metre * x, 2_q + 2_q * x);
    EXPECT_TRUE(q5 == 2_q * si::metre && r5 == 2_q * si::metre);
}

TEST(PolynomialTest, divmod_checks_a_leading_coefficient_that_can_be_0) {
    // d's leading coefficient is in [-5, 5]: an expected.
    using coefficient = id<"[-5..5]">;
    const auto d      = coefficient{3} * x + 1_q;
    const auto e      = divmod(p, d);
    static_assert(std::same_as<std::remove_const_t<decltype(e)>,
                               std::expected<typename std::remove_const_t<decltype(e)>::value_type, error>>);
    ASSERT_TRUE(e.has_value());
    EXPECT_TRUE(e->Quotient * d + e->Remainder == p);
    EXPECT_TRUE((e->Remainder == "22/9"_q));
    const auto zero = divmod(p, coefficient{0} * x + 1_q);
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error(), error::division_by_zero);
}
} // namespace polynomial_tests
