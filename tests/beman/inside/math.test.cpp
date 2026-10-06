// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

// safe_abs handles INT_MIN without UB
TEST(MathTest, safe_abs_handles_int_min_without_ub) {
    static_assert(safe_abs(imax{0}) == 0u);
    static_assert(safe_abs(imax{42}) == 42u);
    static_assert(safe_abs(imax{-42}) == 42u);

    // INT_MIN: -INT_MIN as signed would overflow; safe_abs computes via umax
    constexpr imax min_v = std::numeric_limits<imax>::min();
    static_assert(safe_abs(min_v) == static_cast<umax>(std::numeric_limits<imax>::max()) + 1u);
}

// frexp handles zero
TEST(MathTest, frexp_handles_zero) {
    int    e = 99;
    double r = beman::inside::detail::frexp(0.0, &e);
    ASSERT_EQ(r, 0.0);
    ASSERT_EQ(e, 0);

    // negative zero
    e         = 99;
    double rn = beman::inside::detail::frexp(-0.0, &e);
    ASSERT_EQ(rn, 0.0);
    ASSERT_EQ(e, 0);
}

// frexp handles infinity and NaN
TEST(MathTest, frexp_handles_infinity_and_nan) {
    int    e   = 99;
    double inf = std::numeric_limits<double>::infinity();
    double r   = beman::inside::detail::frexp(inf, &e);
    ASSERT_TRUE(std::isinf(r));
    ASSERT_EQ(e, 0);

    e          = 99;
    double nan = std::numeric_limits<double>::quiet_NaN();
    double rn  = beman::inside::detail::frexp(nan, &e);
    ASSERT_TRUE(std::isnan(rn));
    ASSERT_EQ(e, 0);
}

// frexp matches std::frexp on normals
TEST(MathTest, frexp_matches_std_frexp_on_normals) {
    for (double v : {1.0, 0.5, 0.25, 1.5, 1024.0, 0.001, -7.25}) {
        int    be = 0, se = 0;
        double br = beman::inside::detail::frexp(v, &be);
        double sr = std::frexp(v, &se);
        ASSERT_EQ(br, sr);
        ASSERT_EQ(be, se);
    }
}

// frexp on subnormal scales up via recursion
TEST(MathTest, frexp_on_subnormal_scales_up_via_recursion) {
    // smallest positive subnormal — exercises the e==0 branch
    double sub = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE(sub > 0.0);
    int    be = 0, se = 0;
    double br = beman::inside::detail::frexp(sub, &be);
    double sr = std::frexp(sub, &se);
    ASSERT_EQ(br, sr);
    ASSERT_EQ(be, se);
}

// ldexp identity cases
TEST(MathTest, ldexp_identity_cases) {
    static_assert(beman::inside::detail::ldexp(0.0, 5) == 0.0);
    static_assert(beman::inside::detail::ldexp(1.5, 0) == 1.5);
}

// ldexp inf/NaN passthrough
TEST(MathTest, ldexp_inf_nan_passthrough) {
    double inf = std::numeric_limits<double>::infinity();
    ASSERT_TRUE((std::isinf(beman::inside::detail::ldexp(inf, 3))));

    double nan = std::numeric_limits<double>::quiet_NaN();
    ASSERT_TRUE((std::isnan(beman::inside::detail::ldexp(nan, 3))));
}

// ldexp overflow goes to infinity
TEST(MathTest, ldexp_overflow_goes_to_infinity) {
    // Use volatile inputs so the compiler can't fold the call at compile time.
    volatile double v_pos = 1.0;
    volatile double v_neg = -1.0;
    volatile int    e     = 2048;

    double r = beman::inside::detail::ldexp(v_pos, e);
    ASSERT_TRUE(std::isinf(r));
    ASSERT_TRUE(r > 0.0);

    double rn = beman::inside::detail::ldexp(v_neg, e);
    ASSERT_TRUE(std::isinf(rn));
    ASSERT_TRUE(rn < 0.0);
}

// ldexp underflow produces subnormal or zero
TEST(MathTest, ldexp_underflow_produces_subnormal_or_zero) {
    volatile double v   = 1.0;
    volatile int    big = -1100;
    volatile int    mid = -1050;

    double zero = beman::inside::detail::ldexp(v, big);
    ASSERT_EQ(zero, 0.0);

    double sub = beman::inside::detail::ldexp(v, mid);
    ASSERT_TRUE(sub > 0.0);
    ASSERT_TRUE(sub < std::numeric_limits<double>::min());
}

// ldexp matches std::ldexp on normal exponents
TEST(MathTest, ldexp_matches_std_ldexp_on_normal_exponents) {
    for (double v : {1.0, 0.5, 1.5, -3.25, 1024.0}) {
        for (int e : {0, 1, -1, 10, -10, 50, -50}) {
            double a = beman::inside::detail::ldexp(v, e);
            double b = std::ldexp(v, e);
            ASSERT_EQ(a, b);
        }
    }
}

// ldexp subnormal input is normalised first
TEST(MathTest, ldexp_subnormal_input_is_normalised_first) {
    // Scaling a subnormal up should match std::ldexp
    double sub = std::numeric_limits<double>::denorm_min();
    double a   = beman::inside::detail::ldexp(sub, 100);
    double b   = std::ldexp(sub, 100);
    ASSERT_EQ(a, b);
}

// abs_fraction handles values >= 2^53
TEST(MathTest, abs_fraction_handles_values_ge_2_53) {
    // exponent >= 53 path: num <<= (exponent - bits); den = 1
    // Volatile so the compiler can't fold the constexpr call at compile time.
    volatile double v53 = static_cast<double>(1ull << 53);
    volatile double v60 = static_cast<double>(1ull << 60);

    auto [n1, d1] = abs_fraction(v53);
    ASSERT_EQ(d1, 1u);
    ASSERT_EQ(n1, (1ull << 53));

    auto [n2, d2] = abs_fraction(v60);
    ASSERT_EQ(d2, 1u);
    ASSERT_EQ(n2, (1ull << 60));
}

// abs_fraction throws on non-finite
TEST(MathTest, abs_fraction_throws_on_non_finite) {
    ASSERT_THROW((void)(abs_fraction(std::numeric_limits<double>::infinity())), beman::inside::inside_error);
    ASSERT_THROW((void)(abs_fraction(-std::numeric_limits<double>::infinity())), beman::inside::inside_error);
    ASSERT_THROW((void)(abs_fraction(std::numeric_limits<double>::quiet_NaN())), beman::inside::inside_error);
}

// rational from large doubles
TEST(MathTest, rational_from_large_doubles) {
    // Through rational ctor — exercises the exponent>=53 branch in abs_fraction
    rational r1{static_cast<double>(1ull << 53)};
    ASSERT_EQ(r1.Numerator, (1ull << 53));
    ASSERT_EQ(r1.Denominator, 1);

    rational r2{static_cast<double>(1ull << 50)};
    ASSERT_EQ(r2.Numerator, (1ull << 50));
    ASSERT_EQ(r2.Denominator, 1);
}

// rational rejects non-finite double
TEST(MathTest, rational_rejects_non_finite_double) {
    ASSERT_THROW((void)(rational{std::numeric_limits<double>::infinity()}), beman::inside::inside_error);
    ASSERT_THROW((void)(rational{std::numeric_limits<double>::quiet_NaN()}), beman::inside::inside_error);
}
