// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The rational results of the math functions, placed on a rounding boundary.
//
// A result can sit on a rounding boundary (a grid point, or a midpoint
// between two) only if it is rational: every boundary is. Each core's exact()
// stores the rational results; every other result is irrational, so the Ziv
// driver decides it at some precision. These tests put each rational result
// on a tie or a grid point and check every rounding mode, at compile time
// (the integer path) and at run time (the double and dd tiers first). A
// missed rational result shows up here as a wrong tie or a wrong side.
// Tables are off: an input with few slots would otherwise be a compile-time
// table at run time too, and never reach the double and dd tiers.
//
// The lists, with the theorem that makes them complete (rational x):
//   exp, sinh, cosh, tanh                 x = 0             Lindemann–Weierstrass
//   exp2                                  x an integer      2^(p/q) is rational iff q | p
//   log, acosh                            x = 1             Lindemann–Weierstrass
//   log2, log10                           x = 2^k, 10^k     2 and 10 are not perfect powers
//   asinh, atanh, sin, tan, atan, asin    x = 0             Lindemann–Weierstrass
//   cos                                   x = 0             Lindemann–Weierstrass
//   acos                                  x = 1             Lindemann–Weierstrass
//   atan2(y, x)                           y = 0, x > 0      Lindemann–Weierstrass
//   sqrt, cbrt, hypot                     a rational square or cube   rational root theorem
//   pow(b, p/q)                           b^p a q-th power  rational root theorem
//   sin, cos of a degree or a turn        sin, cos ∈ {0, ±1/2, ±1}    Niven

#define BEMAN_INSIDE_MATH_TABLE_SLOTS 0
#include <beman/inside/units_math.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
namespace am = beman::inside::math;
namespace un = beman::inside::units;

namespace {
template <policy_flag M>
using ints = inside<{-64, 64}, M>;
template <policy_flag M>
using halves = inside<{{-64, 64}, per<2>}, M>;
template <policy_flag M>
using quarters = inside<{{-64, 64}, per<4>}, M>;

// A result as an exact value: an inside, an expected one, or a quantity's number.
template <class R>
constexpr rational exact_value(const R& r) {
    if constexpr (requires { r.has_value(); })
        return r.has_value() ? exact_value(*r) : rational{-999'999};
    else if constexpr (requires { r.numerical_value(); })
        return rational{r.numerical_value()};
    else
        return rational{r};
}

// The result under each rounding mode: nearest (ties away from zero), half
// to even, floor, ceil, toward zero.
struct expected_results {
    rational Nearest, Even, Floor, Ceil, Zero;
};

// F is a captureless lambda []<class O>() { return fn_into<O>(x); }: a
// function argument, not a template argument, which MSVC cannot parse.
template <policy_flag M, template <policy_flag> class G, class F>
void expect_mode(rational want, const char* mode) {
    using Out                       = G<M>;
    static constexpr rational ctime = exact_value(F{}.template operator()<Out>());
    const rational            rtime = exact_value(F{}.template operator()<Out>());
    EXPECT_EQ(ctime, want) << mode << " at compile time";
    EXPECT_EQ(rtime, want) << mode << " at run time";
}

template <template <policy_flag> class G, class F>
void expect_modes(F, expected_results w) {
    expect_mode<round_nearest, G, F>(w.Nearest, "round_nearest");
    expect_mode<round_half_even, G, F>(w.Even, "round_half_even");
    expect_mode<round_floor, G, F>(w.Floor, "round_floor");
    expect_mode<round_ceil, G, F>(w.Ceil, "round_ceil");
    expect_mode<round_toward_zero, G, F>(w.Zero, "round_toward_zero");
}

// A result on a grid point: every mode gives it.
template <template <policy_flag> class G, class F>
void expect_point(F f, rational v) {
    expect_modes<G>(f, {v, v, v, v, v});
}

using q4  = inside<{{-4, 4}, per<4>}>;
using q8  = inside<{{-4, 4}, per<8>}>;
using i8  = inside<{-8, 8}>;
using pos = inside<{{rational{1, 100}, 1024}, per<100>}>;
using pq4 = inside<{{rational{1, 4}, 4}, per<4>}>;
using u4  = inside<{{-1, 1}, per<4>}>;
using deg = inside<{-360, 360}>;
} // namespace

TEST(MathExactTest, roots_of_squares_and_cubes_are_ties) {
    // √(9/4) = 3/2 and ∛(27/8) = 3/2, between 1 and 2.
    expect_modes<ints>([]<class O>() { return am::sqrt_into<O>(q4{2.25}); }, {2, 2, 1, 2, 1});
    expect_modes<ints>([]<class O>() { return am::cbrt_into<O>(q8{3.375}); }, {2, 2, 1, 2, 1});
    expect_modes<ints>([]<class O>() { return am::cbrt_into<O>(q8{-3.375}); }, {-2, -2, -2, -1, -1});
    // √(25/4) = 5/2: the even neighbour is 2.
    expect_modes<ints>([]<class O>() { return am::sqrt_into<O>(inside<{{0, 8}, per<4>}>{6.25}); }, {3, 2, 2, 3, 2});
    // hypot(3/2, 2) = 5/2, hypot(−3/2, −2) too.
    expect_modes<ints>([]<class O>() { return am::hypot_into<O>(q4{1.5}, q4{2}); }, {3, 2, 2, 3, 2});
    expect_modes<ints>([]<class O>() { return am::hypot_into<O>(q4{-1.5}, q4{-2}); }, {3, 2, 2, 3, 2});
    // √4 = 2 and ∛(−8) = −2: grid points.
    expect_point<ints>([]<class O>() { return am::sqrt_into<O>(q4{4}); }, 2);
    expect_point<ints>([]<class O>() { return am::cbrt_into<O>(i8{-8}); }, -2);
}

TEST(MathExactTest, exp2_of_an_integer) {
    // 2^−1 = 1/2, between 0 and 1; 2^−2 = 1/4 between 0 and 1/2.
    expect_modes<ints>([]<class O>() { return am::exp2_into<O>(i8{-1}); }, {1, 0, 0, 1, 0});
    expect_modes<halves>([]<class O>() { return am::exp2_into<O>(i8{-2}); },
                         {rational{1, 2}, 0, 0, rational{1, 2}, 0});
    expect_point<ints>([]<class O>() { return am::exp2_into<O>(i8{3}); }, 8);
    expect_point<ints>([]<class O>() { return am::exp2_into<O>(i8{0}); }, 1);
}

TEST(MathExactTest, logs_of_exact_powers) {
    expect_point<ints>([]<class O>() { return am::log2_into<O>(pos{8}); }, 3);
    expect_point<ints>([]<class O>() { return am::log2_into<O>(pq4{0.25}); }, -2);
    expect_point<ints>([]<class O>() { return am::log10_into<O>(pos{1000}); }, 3);
    expect_point<ints>([]<class O>() { return am::log10_into<O>(pos{rational{1, 100}}); }, -2);
    expect_point<ints>([]<class O>() { return am::log_into<O>(pos{1}); }, 0);
}

TEST(MathExactTest, pow_with_a_rational_exponent) {
    using thirds     = inside<{{-3, 3}, per<3>}>;
    using half_steps = inside<{{-3, 3}, per<2>}>;
    // (1/4)^(1/2) = 1/2: a tie between 0 and 1.
    expect_modes<ints>([]<class O>() { return am::pow_into<O>(q4{0.25}, half_steps{0.5}); }, {1, 0, 0, 1, 0});
    // (9/4)^(3/2) = 27/8: a tie between 13/4 and 14/4.
    expect_modes<quarters>([]<class O>() { return am::pow_into<O>(q4{2.25}, half_steps{1.5}); },
                           {rational{7, 2}, rational{7, 2}, rational{13, 4}, rational{7, 2}, rational{13, 4}});
    // 8^(2/3) = 4 and 8^(−2/3) = 1/4.
    expect_point<ints>([]<class O>() { return am::pow_into<O>(i8{8}, thirds{rational{2, 3}}); }, 4);
    expect_modes<halves>([]<class O>() { return am::pow_into<O>(i8{8}, thirds{rational{-2, 3}}); },
                         {rational{1, 2}, 0, 0, rational{1, 2}, 0});
}

TEST(MathExactTest, zero_and_one_are_exact) {
    // Grid points: a result a hair off 0 or 1 would floor or ceil to a neighbour.
    expect_point<ints>([]<class O>() { return am::exp_into<O>(i8{0}); }, 1);
    expect_point<ints>([]<class O>() { return am::sin_into<O>(i8{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::cos_into<O>(i8{0}); }, 1);
    expect_point<ints>([]<class O>() { return am::atan_into<O>(i8{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::asin_into<O>(u4{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::acos_into<O>(u4{1}); }, 0);
    expect_point<ints>([]<class O>() { return am::atan2_into<O>(i8{0}, i8{1}); }, 0);
    expect_point<ints>([]<class O>() { return am::sinh_into<O>(i8{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::cosh_into<O>(i8{0}); }, 1);
    expect_point<ints>([]<class O>() { return am::tanh_into<O>(i8{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::asinh_into<O>(i8{0}); }, 0);
    expect_point<ints>([]<class O>() { return am::acosh_into<O>(inside<{1, 8}>{1}); }, 0);
    expect_point<ints>(
        []<class O>() { return am::atanh_into<O>(inside<{{rational{-3, 4}, rational{3, 4}}, per<4>}>{0}); }, 0);
}

TEST(MathExactTest, niven_angles_in_degrees) {
    using un::si::degree;
    // sin 30° = cos 60° = 1/2 and sin −30° = −1/2: ties.
    expect_modes<ints>([]<class O>() { return un::sin_into<O>(un::quantity<degree, deg>{30}); }, {1, 0, 0, 1, 0});
    expect_modes<ints>([]<class O>() { return un::cos_into<O>(un::quantity<degree, deg>{60}); }, {1, 0, 0, 1, 0});
    expect_modes<ints>([]<class O>() { return un::sin_into<O>(un::quantity<degree, deg>{-30}); }, {-1, 0, -1, 0, 0});
    expect_modes<ints>([]<class O>() { return un::cos_into<O>(un::quantity<degree, deg>{240}); }, {-1, 0, -1, 0, 0});
    // 0 and ±1: grid points.
    expect_point<ints>([]<class O>() { return un::cos_into<O>(un::quantity<degree, deg>{90}); }, 0);
    expect_point<ints>([]<class O>() { return un::sin_into<O>(un::quantity<degree, deg>{180}); }, 0);
    expect_point<ints>([]<class O>() { return un::sin_into<O>(un::quantity<degree, deg>{-90}); }, -1);
}
