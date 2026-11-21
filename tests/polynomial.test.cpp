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

using digit = inside<{0, 10}>;
using small = inside<{0, 3}>;

// 3 + 2 x + x²
inline constexpr auto p = just<3> + just<2> * x + just<1> * x * x;
} // namespace

TEST(PolynomialTest, substitute_evaluates_by_horner) {
    EXPECT_TRUE(substitute(p, x, just<2>) == 11);
    EXPECT_TRUE(substitute(p, x, just<-1>) == 2);
    // The value's range follows from the coefficients' and the argument's: c₀ + c₁ a in [0, 40].
    const auto line = digit{4} + digit{5} * x;
    const auto v    = substitute(line, x, small{2});
    static_assert(lower_of<decltype(v)> == grid_number{0} && upper_of<decltype(v)> == grid_number{40});
    EXPECT_TRUE(v == 14);
    // Coefficients with units give a quantity.
    const auto s = just<2> * si::metre + just<3> * si::metre * x;
    EXPECT_TRUE(substitute(s, x, just<4>) == just<14> * si::metre);
    // A constant has no x: itself.
    EXPECT_TRUE(substitute(just<7>, x, just<2>) == 7);
}

TEST(PolynomialTest, substitute_composes) {
    // p(y + 1) = 6 + 4 y + y².
    EXPECT_TRUE(substitute(p, x, just<1> * y + just<1>) == just<6> + just<4> * y + just<1> * y * y);
    // In two variables, the other stays: (x + y)² at x = 2 is 4 + 4 y + y².
    const auto b = just<1> * x + just<1> * y;
    EXPECT_TRUE(substitute(b * b, x, just<2>) == just<4> + just<4> * y + just<1> * y * y);
    // Vector coefficients: the curve e₁ + t e₂ at t = 3.
    const auto c = just<1> * e1 + just<1> * x * e2;
    EXPECT_TRUE(substitute(c, x, just<3>) == just<1> * e1 + just<3> * e2);
}

TEST(PolynomialTest, derivative_and_antiderivative) {
    EXPECT_TRUE(derivative(p, x) == just<2> + just<2> * x);
    static_assert(std::same_as<decltype(derivative(just<5>, x)), multivector<>>);
    // The product rule: (p q)' = p' q + p q'.
    const auto q = just<1> - just<3> * x * x * x;
    EXPECT_TRUE(derivative(p * q, x) == derivative(p, x) * q + p * derivative(q, x));
    // The antiderivative has constant term 0 and its derivative is p.
    const auto F = antiderivative(p, x);
    EXPECT_EQ(to_string(F), "3 x + 1 x² + 1/3 x³");
    EXPECT_TRUE(derivative(F, x) == p);
    // ∫₀² p dx = F(2) - F(0) = 6 + 4 + 8/3 = 38/3.
    EXPECT_TRUE((substitute(F, x, just<2>) - substitute(F, x, just<0>) == just<38, 3>));
    // In a variable p does not have: 0, and p y.
    static_assert(std::same_as<decltype(derivative(p, y)), multivector<>>);
    EXPECT_TRUE(antiderivative(p, y) == p * (just<1> * y));
    // With units: the derivative of a position in m is in m (x is a number).
    const auto s = just<2> * si::metre + just<3> * si::metre * x;
    EXPECT_TRUE(derivative(s, x) == just<3> * si::metre);
    // Negative powers: (1/x)' = -1/x².
    EXPECT_TRUE(derivative(just<1> / (just<1> * x), x) == just<-1> / (just<1> * x * x));
}

TEST(PolynomialTest, divmod_divides_with_a_remainder) {
    // x² + 2 x + 3 = (x + 1)(x + 1) + 2.
    const auto d       = just<1> + just<1> * x;
    const auto [q, r]  = divmod(p, d);
    EXPECT_TRUE(q == just<1> + just<1> * x);
    EXPECT_TRUE(r == 2);
    // A leading coefficient of 2: exact fractions.
    const auto d2      = just<1> + just<2> * x;
    const auto [q2, r2] = divmod(p, d2);
    EXPECT_TRUE((q2 == just<3, 4> + just<1, 2> * x));
    EXPECT_TRUE((r2 == just<9, 4>));
    EXPECT_TRUE(q2 * d2 + r2 == p);
    // A lower degree: q = 0, r = p.
    const auto [q3, r3] = divmod(d, p);
    static_assert(std::same_as<std::remove_const_t<decltype(q3)>, multivector<>>);
    EXPECT_TRUE(r3 == d);
    // Single terms are polynomials too: 3 x² / x = 3 x.
    const auto [q4, r4] = divmod(just<3> * x * x, just<1> * x);
    EXPECT_TRUE(q4 == just<3> * x);
    static_assert(std::same_as<std::remove_const_t<decltype(r4)>, multivector<>>);
    // Coefficients with units: (6 m + 4 m x) / (2 + 2 x) = 2 m, remainder 2 m.
    const auto [q5, r5] = divmod(just<6> * si::metre + just<4> * si::metre * x, just<2> + just<2> * x);
    EXPECT_TRUE(q5 == just<2> * si::metre && r5 == just<2> * si::metre);
}

TEST(PolynomialTest, divmod_checks_a_leading_coefficient_that_can_be_0) {
    // d's leading coefficient is in [-5, 5]: an expected.
    using coefficient = inside<{-5, 5}>;
    const auto d      = coefficient{3} * x + just<1>;
    const auto e      = divmod(p, d);
    static_assert(std::same_as<std::remove_const_t<decltype(e)>,
                               std::expected<typename std::remove_const_t<decltype(e)>::value_type, error>>);
    ASSERT_TRUE(e.has_value());
    EXPECT_TRUE(e->Quotient * d + e->Remainder == p);
    EXPECT_TRUE((e->Remainder == just<22, 9>));
    const auto zero = divmod(p, coefficient{0} * x + just<1>);
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error(), error::division_by_zero);
}
} // namespace polynomial_tests
