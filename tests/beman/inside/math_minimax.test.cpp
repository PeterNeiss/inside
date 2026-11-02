// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The generated minimax fits (math/detail/minimax.hpp, from Sollya): every
// fit's sampled relative error stays below its certified bound Err and comes
// close to it (a minimax error equioscillates, so its maximum over a fine
// sample is near the supremum). That ties each coefficient set to its own
// bound. The polynomial runs in binary128 from the double coefficients; the
// reference is the dd tier's function (within about 2^-97).

#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <utility>

#ifndef BEMAN_INSIDE_MATH_NO_FP
namespace fpk = beman::inside::math::detail::fp;
namespace mm  = fpk::minimax;
namespace ddk = beman::inside::math::detail::dd;

namespace {
using quad = __float128;

template <std::size_t N>
quad horner(quad z, const std::array<double, N>& c) {
    quad p = c[0];
    for (std::size_t k = 1; k < N; ++k)
        p = p * z + c[k];
    return p;
}
quad of(ddk::dd v) { return static_cast<quad>(v.Hi) + v.Lo; }

// The worst |p/f − 1| over [a, b], in units of the fit's Err.
template <std::size_t N, typename F>
double worst_ratio(const mm::fit<N>& fit, bool odd, double a, double b, F f) {
    constexpr int steps = 4096;
    double        worst = 0;
    for (int i = 0; i <= steps; ++i) {
        const double x = a + (b - a) * i / steps;
        if (x == 0)
            continue;
        const quad p   = odd ? x * horner(static_cast<quad>(x) * x, fit.C) : horner(static_cast<quad>(x), fit.C);
        const quad ref = of(f(ddk::dd{x, 0}));
        const quad rel = (p - ref) / ref;
        worst          = std::max(worst, std::fabs(static_cast<double>(rel)) / fit.Err);
    }
    return worst;
}

template <auto& Fit, typename F>
void expect_fit(const char* name, int n, bool odd, double a, double b, F f) {
    const double r = worst_ratio(Fit, odd, a, b, f);
    EXPECT_LE(r, 1.0) << name << " " << n;
    EXPECT_GE(r, 0.5) << name << " " << n; // the bound belongs to these coefficients
}

template <int Min, int Max, template <int> class Var, typename F>
void expect_fits(const char* name, bool odd, double a, double b, F f) {
    [&]<int... I>(std::integer_sequence<int, I...>) {
        (expect_fit<Var<Min + I>::value>(name, Min + I, odd, a, b, f), ...);
    }(std::make_integer_sequence<int, Max - Min + 1>{});
}
template <int N>
struct asin_of {
    static constexpr const auto& value = mm::asin_fit<N>;
};
template <int N>
struct exp_of {
    static constexpr const auto& value = mm::exp_fit<N>;
};
template <int N>
struct atanh_of {
    static constexpr const auto& value = mm::atanh_fit<N>;
};
template <int N>
struct atan_of {
    static constexpr const auto& value = mm::atan_fit<N>;
};
} // namespace

TEST(MathMinimaxTest, every_fit_stays_within_and_near_its_certified_bound) {
    expect_fits<mm::asin_min, mm::asin_max, asin_of>("asin", true, 0, 0.5, [](ddk::dd x) { return ddk::asin(x); });
    expect_fits<mm::exp_min, mm::exp_max, exp_of>(
        "exp", false, -fpk::r_exp, fpk::r_exp, [](ddk::dd x) { return ddk::exp(x); });
    expect_fits<mm::atanh_min, mm::atanh_max, atanh_of>(
        "atanh", true, 0, fpk::f_max, [](ddk::dd x) { return ddk::atanh(x); });
    expect_fits<mm::atan_min, mm::atan_max, atan_of>(
        "atan", true, 0, fpk::t_max, [](ddk::dd x) { return ddk::atan(x); });
}
#endif
