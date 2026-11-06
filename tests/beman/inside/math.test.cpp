// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The math functions: exact results on rounding boundaries, the
// inverse hyperbolics, grids rounding toward zero, angles over many turns,
// sign/copysign, and the counting functions.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units/math.hpp>

#include <gtest/gtest.h>

#include "support.hpp"

using beman::inside::test::exact;

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// The rational results of the math functions, placed on a rounding boundary.
//
// A result can sit on a rounding boundary (a grid point, or a midpoint
// between two) only if it is rational: every boundary is. Each core's exact()
// stores the rational results; every other result is irrational, so the Ziv
// driver decides it at some precision. These tests put each rational result
// on a tie or a grid point and check every rounding mode, at compile time
// (the integer path) and at run time (the double and dd tiers first). A
// missed rational result shows up here as a wrong tie or a wrong side.
// Every input has at least 256 slots: a smaller one would be a compile-time
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
namespace math_exact_tests {
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

// F is a captureless lambda []<class O>() { return fn_into<O>(x); }.
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

// The inputs, named by the values the tests store, on notches fine enough for
// at least 256 slots (no table).
using q4  = inside<{{-4, 4}, per<64>}>;
using q8  = inside<{{-4, 4}, per<64>}>;
using i8  = inside<{{-8, 8}, per<16>}>;
using pos = inside<{{rational{1, 100}, 1024}, per<100>}>;
using pq4 = inside<{{rational{1, 4}, 4}, per<128>}>;
using u4  = inside<{{-1, 1}, per<128>}>;
using deg = inside<{-360, 360}>;
using nq4 = inside<{{0, 4}, per<64>}>; // sqrt's and pow's base domain: no negative values
using n8  = inside<{{0, 8}, per<32>}>;
template <class... In>
inline constexpr bool no_table = (!math::detail::engine::table_input<In> && ...);
static_assert(no_table<q4, q8, i8, pos, pq4, u4, deg, nq4, n8>);
} // namespace

TEST(MathExactTest, roots_of_squares_and_cubes_are_ties) {
    // √(9/4) = 3/2 and ∛(27/8) = 3/2, between 1 and 2.
    expect_modes<ints>([]<class O>() { return am::sqrt_into<O>(nq4{2.25}); }, {2, 2, 1, 2, 1});
    expect_modes<ints>([]<class O>() { return am::cbrt_into<O>(q8{3.375}); }, {2, 2, 1, 2, 1});
    expect_modes<ints>([]<class O>() { return am::cbrt_into<O>(q8{-3.375}); }, {-2, -2, -2, -1, -1});
    // √(25/4) = 5/2: the even neighbour is 2.
    expect_modes<ints>([]<class O>() { return am::sqrt_into<O>(inside<{{0, 8}, per<32>}>{6.25}); }, {3, 2, 2, 3, 2});
    // hypot(3/2, 2) = 5/2, hypot(−3/2, −2) too.
    expect_modes<ints>([]<class O>() { return am::hypot_into<O>(q4{1.5}, q4{2}); }, {3, 2, 2, 3, 2});
    expect_modes<ints>([]<class O>() { return am::hypot_into<O>(q4{-1.5}, q4{-2}); }, {3, 2, 2, 3, 2});
    // √4 = 2 and ∛(−8) = −2: grid points.
    expect_point<ints>([]<class O>() { return am::sqrt_into<O>(nq4{4}); }, 2);
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
    expect_modes<ints>([]<class O>() { return am::pow_into<O>(nq4{0.25}, half_steps{0.5}); }, {1, 0, 0, 1, 0});
    // (9/4)^(3/2) = 27/8: a tie between 13/4 and 14/4.
    expect_modes<quarters>([]<class O>() { return am::pow_into<O>(nq4{2.25}, half_steps{1.5}); },
                           {rational{7, 2}, rational{7, 2}, rational{13, 4}, rational{7, 2}, rational{13, 4}});
    // 8^(2/3) = 4 and 8^(−2/3) = 1/4.
    expect_point<ints>([]<class O>() { return am::pow_into<O>(n8{8}, thirds{rational{2, 3}}); }, 4);
    expect_modes<halves>([]<class O>() { return am::pow_into<O>(n8{8}, thirds{rational{-2, 3}}); },
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
    expect_point<ints>([]<class O>() { return am::acosh_into<O>(inside<{{1, 8}, per<64>}>{1}); }, 0);
    expect_point<ints>(
        []<class O>() { return am::atanh_into<O>(inside<{{rational{-3, 4}, rational{3, 4}}, per<256>}>{0}); }, 0);
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

// fn<Mode>(x): the rounding mode on the call, for an input whose type has
// none. The output rounds by exactly Mode.
TEST(MathExactTest, mode_on_the_deduced_call) {
    using x_t = inside<{{1, 16}, per<64>}>; // no rounding policy
    static_assert(math::sqrt<round_floor>(x_t{2}) == 1.40625_ins && math::sqrt<round_ceil>(x_t{2}) == 1.421875_ins);
    static_assert(math::exp<round_floor>(x_t{1.5}) < math::exp<round_ceil>(x_t{1.5}));
    static_assert(detail::rounding_of(flags_of<decltype(math::sqrt<round_toward_zero>(x_t{2}))>) ==
                  detail::round_mode::trunc);
    static_assert(math::pow_base<2, round_nearest>(x_t{3}) == 8);
    static_assert(
        math::atan2<round_nearest>(x_t{1}, x_t{1}) ==
        math::atan2(inside<{{1, 16}, per<64>}, round_nearest>{1}, inside<{{1, 16}, per<64>}, round_nearest>{1}));
    EXPECT_EQ(math::log2<round_floor>(x_t{8}), 3);
}
} // namespace math_exact_tests

// math::asinh / acosh / atanh: shared domains, auto output grids, exact anchors,
// and agreement with <cmath> within one output notch on every engine.
namespace inverse_hyperbolic_tests {
using namespace beman::inside;

namespace {
using A                     = inside<{{-64, 64}, per<1024>}, round_nearest>;
using C                     = inside<{{1, 64}, per<1024>}, round_nearest>;
using T                     = inside<{{-0.9990234375, 0.9990234375}, per<1024>}, round_nearest>;
constexpr double notch_size = 1.0 / 1024;

template <class R>
double value(const R& r) {
    return r.template to<double>();
}
} // namespace

TEST(InverseHyperbolicTest, anchors_are_exact_and_constexpr_on_the_integer_engine) {
    static_assert(rational{math::asinh(A{0})} == 0);
    static_assert(rational{math::acosh(C{1})} == 0);
    static_assert(rational{math::atanh(T{0})} == 0);
    EXPECT_EQ(rational{math::asinh(A{0})}, 0);
    EXPECT_EQ(rational{math::acosh(C{1})}, 0);
    EXPECT_EQ(rational{math::atanh(T{0})}, 0);
}

TEST(InverseHyperbolicTest, auto_output_grids_hold_the_endpoint_images) {
    using AR = decltype(math::asinh(A{0}));
    static_assert(lower_of<AR> <= rational{-48523, 10000} &&
                  upper_of<AR> >= rational{48523, 10000}); // asinh(64) ≈ 4.8523
    using CR = decltype(math::acosh(C{1}));
    static_assert(lower_of<CR> == 0 && upper_of<CR> >= rational{48520, 10000}); // acosh(64) ≈ 4.8520
    static_assert(notch_of<CR> == notch_of<C>);
}

TEST(InverseHyperbolicTest, every_engine_matches_cmath_within_one_notch) {
    for (double x = -64; x <= 64; x += 0.5) {
        EXPECT_NEAR(value(math::asinh(A{x})), std::asinh(x), notch_size) << x;
    }
    for (double x = 1; x <= 64; x += 0.25) {
        EXPECT_NEAR(value(math::acosh(C{x})), std::acosh(x), notch_size) << x;
    }
    for (double x = -0.9990234375; x <= 0.9990234375; x += 0.0625) {
        EXPECT_NEAR(value(math::atanh(T{x})), std::atanh(x), notch_size) << x;
    }
    // Near the poles of atanh the ratio form keeps full precision.
    EXPECT_NEAR(value(math::atanh(T{0.9990234375})), std::atanh(0.9990234375), notch_size);
}

TEST(InverseHyperbolicTest, explicit_output_and_decimal_grids) {
    using I = inside<{-20, 20}, round_nearest>; // integer grid
    EXPECT_EQ(rational{math::asinh(I{3})}, 2);  // asinh(3) ≈ 1.818 → 2
    using Out = inside<{{0, 8}, per<256>}, round_nearest>;
    EXPECT_NEAR(value(math::acosh_into<Out>(C{2})), std::acosh(2.0), 1.0 / 256);
}
} // namespace inverse_hyperbolic_tests

// Transcendentals need rounding permission. This exercises
// `beman::inside::math` on rounding grids: integer-index storage and non-dyadic
// (1/100) grids — using exact
// special values that are bit-exact on both engines and any grid containing them.
namespace math_snap_grids_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// transcendentals on integer and 1/100 grids
TEST(MathRoundingTest, transcendentals_on_integer_and_hundredth_grids) {
    // round_nearest is a rounding mode.
    using Ang = inside<{{-8, 8}, per<16384>}, round_nearest>; // integer-index storage
    ASSERT_EQ(rational{math::sin(Ang{0})}, 0);
    ASSERT_EQ(rational{math::cos(Ang{0})}, 1);
    ASSERT_EQ(rational{math::atan(Ang{0})}, 0);

    using Sq = inside<{{0, 16}, per<100>}, round_nearest>; // non-dyadic 1/100 grid
    ASSERT_EQ(rational{math::sqrt(Sq{0})}, 0);
    ASSERT_EQ(rational{math::sqrt(Sq{4})}, 2);

    using Lg = inside<{{1, 1000}, per<100>}, round_nearest>;
    ASSERT_EQ(rational{math::log10(Lg{1})}, 0);
    ASSERT_EQ(rational{math::log10(Lg{100})}, 2);

    using Cb = inside<{{-8, 8}, per<100>}, round_nearest>;
    ASSERT_EQ(rational{math::cbrt(Cb{0})}, 0);
    ASSERT_EQ(rational{math::cbrt(Cb{8})}, 2);
    ASSERT_EQ(rational{math::cbrt(Cb{-8})}, -2);

    using Hy = inside<{{-10, 10}, per<100>}, round_nearest>;
    ASSERT_EQ(rational{math::sinh(Hy{0})}, 0);
    ASSERT_EQ(rational{math::cosh(Hy{0})}, 1);
    ASSERT_EQ(rational{math::tanh(Hy{0})}, 0);

    using Ex = inside<{{-4, 4}, per<100>}, round_nearest>;
    ASSERT_EQ(rational{math::exp(Ex{0})}, 1);

    // pow of a positive base: plain; 2^4 snaps exactly onto the 1/100 grid.
    using B = inside<{{1, 16}, per<100>}, round_nearest>;
    using E = inside<{{-4, 8}, per<100>}, round_nearest>;
    auto p  = math::pow(B{2}, E{4});
    ASSERT_EQ(rational{p}, 16);
}
} // namespace math_snap_grids_tests

// docs/advanced/math-engine.md "Angles over many turns": a wrapping radians grid folds modulo
// span + notch (not 2π); an unwrapped radians angle and a turn phase converted
// at the call do not drift.
namespace math_turns_tests {
using namespace beman::inside;

TEST(MathTurnsTest, a_wrapping_radians_grid_folds_modulo_span_plus_notch) {
    using wrapped = inside<{{0, 6.28125}, per<64>}, wrap | round_nearest>;
    using step_t  = inside<{{0, 8}, per<64>}, round_nearest>;
    wrapped w{0};
    w += step_t{6.296875}; // span + notch: back to 0
    EXPECT_EQ(rational{w}, 0);
    wrapped v{0};
    v += step_t{6.28125}; // ≈ 2π is not a full fold
    EXPECT_EQ(rational{v}, (rational{201, 32}));
}

TEST(MathTurnsTest, a_turn_phase_wraps_exactly_and_converts_at_the_call) {
    using turn_t  = inside<{{0, 1 - 1.0 / 4096}, per<4096>}, wrap | round_nearest>;
    using angle_t = inside<{{0, 8}, per<16384>}, round_nearest>;
    turn_t       phase{0};
    const turn_t step{rational{777, 4096}};
    for (int k = 0; k < 4096 * 3; ++k)
        phase += step;             // 777·3 whole turns
    EXPECT_EQ(rational{phase}, 0); // no drift

    turn_t     q{0.25};
    const auto s = math::sin_into<math::unit_amplitude<16384>>(angle_t{q * math::two_pi});
    EXPECT_EQ(rational{s}, 1); // sin(π/2), on the 1/16384 grid
}

TEST(MathTurnsTest, an_unwrapped_radians_angle_stays_exact) {
    using big    = inside<{{0, 1 << 20}, per<64>}, round_nearest>;
    using step_t = inside<{{0, 1}, per<64>}, round_nearest>;
    big x{0};
    for (int n = 0; n < 100000; ++n)
        x += step_t{0.25};
    EXPECT_EQ(rational{x}, 25000); // exact after ~4000 turns
}
} // namespace math_turns_tests

// Counting and number theory (math/integer.hpp): factorial, binomial,
// falling_factorial, rising_factorial, multinomial, gcd and lcm. Their result
// grids, every value on small ranges against plain loops and the identities,
// values past 64 bits, the _into forms and compile-time evaluation. Inputs
// pass through a volatile, so the run-time paths run.
namespace math_integer_tests {
using namespace beman::inside;

using detail::big_int;
using detail::grid_rational;

namespace {
int at_run_time(int v) {
    volatile int x = v;
    return x;
}

umax factorial_ref(umax n) {
    umax f = 1;
    for (umax i = 2; i <= n; ++i)
        f *= i;
    return f;
}
umax binomial_ref(umax n, umax k) {
    if (k > n)
        return 0;
    umax c = 1;
    for (umax i = 1; i <= k; ++i)
        c = c * (n - k + i) / i;
    return c;
}
umax falling_ref(umax n, umax k) {
    if (k > n)
        return 0;
    umax p = 1;
    for (umax i = 0; i < k; ++i)
        p *= n - i;
    return p;
}

consteval grid_rational g(big_int v) { return grid_rational{v}; }
} // namespace

TEST(MathIntegerTest, result_grids) {
    using n20 = inside<{0, 20}>;
    static_assert(grid_of<decltype(math::factorial(n20{}))> == grid{1, 2432902008176640000});
    static_assert(grid_of<decltype(math::factorial(inside<{3, 5}>{}))> == grid{6, 120});
    static_assert(grid_of<decltype(math::factorial(just<5>))> == grid{120});
    static_assert(sizeof(math::factorial(n20{})) == 8);

    // falling: 0 where k can pass n; P(10, 5) at the top.
    static_assert(grid_of<decltype(math::falling_factorial(inside<{0, 10}>{}, inside<{0, 5}>{}))> == grid{0, 30240});
    static_assert(grid_of<decltype(math::falling_factorial(inside<{5, 10}>{}, inside<{0, 3}>{}))> == grid{1, 720});
    // rising: 0 when n can be 0 with k ≥ 1.
    static_assert(grid_of<decltype(math::rising_factorial(inside<{0, 10}>{}, inside<{0, 5}>{}))> == grid{0, 240240});
    static_assert(grid_of<decltype(math::rising_factorial(inside<{2, 4}>{}, inside<{1, 3}>{}))> == grid{2, 120});
    // binomial: the peak at the k nearest n/2, the least at an end of k's range.
    static_assert(grid_of<decltype(math::binomial(inside<{0, 60}>{}, inside<{0, 60}>{}))> ==
                  grid{0, 118264581564861424});
    static_assert(grid_of<decltype(math::binomial(inside<{10, 20}>{}, inside<{0, 5}>{}))> == grid{1, 15504});
    static_assert(grid_of<decltype(math::binomial(inside<{10, 20}>{}, inside<{3, 4}>{}))> == grid{120, 4845});
    static_assert(grid_of<decltype(math::multinomial(inside<{0, 2}>{}, inside<{0, 2}>{}))> == grid{1, 6});

    // gcd and lcm keep the lattice their operands share.
    static_assert(grid_of<decltype(math::gcd(inside<{-30, 30}>{}, inside<{-30, 30}>{}))> == grid{0, 30});
    static_assert(grid_of<decltype(math::gcd(inside<{{0, 100}, 2}>{}, inside<{{0, 100}, 4}>{}))> == grid{{0, 100}, 2});
    static_assert(grid_of<decltype(math::gcd(inside<{1, 30}>{}, inside<{1, 30}>{}))> == grid{1, 30});
    static_assert(grid_of<decltype(math::gcd(just<12>, just<18>))> == grid{6});
    static_assert(grid_of<decltype(math::lcm(inside<{1, 30}>{}, inside<{1, 30}>{}))> == grid{1, 900});
    static_assert(grid_of<decltype(math::lcm(inside<{0, 5}>{}, inside<{1, 5}>{}))> == grid{0, 25});
    static_assert(grid_of<decltype(math::lcm(inside<{{2, 30}, 2}>{}, inside<{{3, 30}, 3}>{}))> == grid{{6, 900}, 6});
    static_assert(grid_of<decltype(math::lcm(just<4>, just<6>))> == grid{12});
}

TEST(MathIntegerTest, compile_time) {
    static_assert(math::factorial(just<10>) == 3628800);
    static_assert(math::binomial(just<49>, just<6>) == 13983816);
    static_assert(math::falling_factorial(just<8>, just<3>) == 336);
    static_assert(math::rising_factorial(just<5>, just<3>) == 210);
    static_assert(math::multinomial(1_ins, 4_ins, 4_ins, 2_ins) == 34650); // MISSISSIPPI
    static_assert(math::gcd(just<-12>, just<18>) == 6 && math::lcm(just<-4>, just<6>) == 12);
    static_assert(math::binomial(inside<{0, 60}>{60}, inside<{0, 60}>{30}) == 118264581564861424);
}

TEST(MathIntegerTest, small_ranges_against_loops) {
    using n60 = inside<{0, 60}>;
    for (int n = 0; n <= 20; ++n)
        EXPECT_TRUE(math::factorial(inside<{0, 20}>{at_run_time(n)}) == factorial_ref(static_cast<umax>(n))) << n;
    for (int n = 0; n <= 60; ++n)
        for (int k = 0; k <= 60; ++k) {
            const auto c = math::binomial(n60{at_run_time(n)}, n60{at_run_time(k)});
            EXPECT_TRUE(c == binomial_ref(static_cast<umax>(n), static_cast<umax>(k))) << n << " " << k;
            if (k <= n) { // symmetry
                EXPECT_TRUE(c == math::binomial(n60{n}, n60{n - k}));
            }
        }
    using n20 = inside<{0, 20}>;
    for (int n = 0; n <= 20; ++n)
        for (int k = 0; k <= 20; ++k) {
            const auto p = math::falling_factorial(n20{at_run_time(n)}, n20{at_run_time(k)});
            EXPECT_TRUE(p == falling_ref(static_cast<umax>(n), static_cast<umax>(k))) << n << " " << k;
            // C(n, k)·k! = P(n, k)
            EXPECT_TRUE(math::binomial(n20{n}, n20{k}) * math::factorial(n20{k}) == p);
        }
    using n12 = inside<{0, 12}>;
    for (int n = 0; n <= 12; ++n)
        for (int k = 0; k <= 12; ++k) {
            const auto r    = math::rising_factorial(n12{at_run_time(n)}, n12{at_run_time(k)});
            const umax want = k == 0   ? 1
                              : n == 0 ? 0
                                       : falling_ref(static_cast<umax>(n + k - 1), static_cast<umax>(k));
            EXPECT_TRUE(r == want) << n << " " << k;
        }
    using n8 = inside<{0, 8}>;
    for (int a = 0; a <= 8; ++a)
        for (int b = 0; b <= 8; ++b)
            for (int c = 0; c <= 8; ++c) {
                const auto m    = math::multinomial(n8{at_run_time(a)}, n8{at_run_time(b)}, n8{at_run_time(c)});
                const umax want = binomial_ref(static_cast<umax>(a + b), static_cast<umax>(b)) *
                                  binomial_ref(static_cast<umax>(a + b + c), static_cast<umax>(c));
                EXPECT_TRUE(m == want) << a << " " << b << " " << c;
            }
    using z = inside<{-30, 30}>;
    for (int a = -30; a <= 30; ++a)
        for (int b = -30; b <= 30; ++b) {
            EXPECT_TRUE(math::gcd(z{at_run_time(a)}, z{at_run_time(b)}) == std::gcd(a, b)) << a << " " << b;
            EXPECT_TRUE(math::lcm(z{at_run_time(a)}, z{at_run_time(b)}) == std::lcm(a, b)) << a << " " << b;
        }
}

TEST(MathIntegerTest, past_64_bits) {
    const auto f25 = math::factorial(inside<{0, 25}>{at_run_time(25)});
    EXPECT_EQ(to_string(f25), "15511210043330985984000000");
    const auto f100 = math::factorial(inside<{0, 100}>{at_run_time(100)});
    static_assert(detail::wide_index_storage<std::remove_const_t<decltype(f100)>>);
    EXPECT_EQ(to_string(f100),
              "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941"
              "463976156518286253697920827223758251185210916864000000000000000000000000");
    using n100 = inside<{0, 100}>;
    EXPECT_EQ(to_string(math::binomial(n100{at_run_time(100)}, n100{at_run_time(50)})),
              "100891344545564193334812497256");
    // Pascal at C(1000, 500), 300 digits.
    using n1000   = inside<{0, 1000}>;
    using n999    = inside<{0, 999}>;
    const auto c  = math::binomial(n1000{at_run_time(1000)}, n1000{at_run_time(500)});
    const auto c1 = math::binomial(n999{at_run_time(999)}, n999{at_run_time(499)});
    const auto c2 = math::binomial(n999{at_run_time(999)}, n999{at_run_time(500)});
    EXPECT_TRUE(c == c1 + c2);
    EXPECT_EQ(to_string(c).size(), 300u);
    // P(30, 25) = 30!/5!
    EXPECT_TRUE(math::falling_factorial(inside<{0, 30}>{at_run_time(30)}, inside<{0, 30}>{at_run_time(25)}) ==
                math::factorial(inside<{0, 30}>{30}) / just<120>);

    // gcd and lcm of wide values: 2^150·3 and 2^120·9 share 2^120·3.
    using wide         = inside<{0, g(big_int{1} << 160)}>;
    constexpr auto a   = g((big_int{1} << 150) * big_int{3});
    constexpr auto b   = g((big_int{1} << 120) * big_int{9});
    const wide     wa  = a;
    const wide     wb  = b;
    const auto     gab = math::gcd(wa, wb);
    EXPECT_TRUE(gab == just<g((big_int{1} << 120) * big_int{3})>);
    EXPECT_TRUE(math::lcm(wa, wb) == just<g((big_int{1} << 150) * big_int{9})>);
    // An odd exponent and a power of two are coprime (RSA's e and φ, smaller).
    EXPECT_TRUE(math::gcd(inside<{0, 65537}>{65537}, wide{just<g(big_int{1} << 130)>}) == 1);
}

TEST(MathIntegerTest, into_forms) {
    using n10 = inside<{0, 10}>;
    EXPECT_TRUE((math::factorial_into<inside<{0, 1000}, clamp>>(n10{at_run_time(7)})) == 1000); // 5040 saturates
    using checked_out = inside<{0, 1000}>;
    EXPECT_THROW((void)math::factorial_into<checked_out>(n10{at_run_time(7)}), inside_error);
    EXPECT_TRUE((math::binomial_into<inside<{0, 255}>>(n10{at_run_time(10)}, n10{at_run_time(5)})) == 252);
    EXPECT_TRUE((math::gcd_into<inside<{0, 100}>>(n10{at_run_time(4)}, n10{at_run_time(6)})) == 2);
    EXPECT_TRUE((math::lcm_into<inside<{0, 100}>>(n10{at_run_time(4)}, n10{at_run_time(6)})) == 12);
    EXPECT_TRUE((math::multinomial_into<inside<{0, 100}>>(n10{at_run_time(2)}, n10{at_run_time(2)})) == 6);
    EXPECT_TRUE((math::falling_factorial_into<inside<{0, 100}>>(n10{at_run_time(5)}, n10{at_run_time(2)})) == 20);
    EXPECT_TRUE((math::rising_factorial_into<inside<{0, 100}>>(n10{at_run_time(5)}, n10{at_run_time(2)})) == 30);
}

} // namespace math_integer_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <typename X, typename Y>
void expect_copysign_fmod_match_rational() {
    for (const X x : inside_range<grid_of<X>, flags_of<X>>{})
        for (const Y y : inside_range<grid_of<Y>, flags_of<Y>>{}) {
            const rational rx{x}, ry{y};
            EXPECT_EQ(rational{math::copysign(x, y)}, ry < 0 ? -abs(rx) : abs(rx));
            if (ry != 0) {
                const auto  r    = math::fmod(x, y);
                const exact want = exact{rx} - trunc(exact{rx} / exact{ry}) * exact{ry};
                if constexpr (is_expected_v<decltype(r)>)
                    EXPECT_EQ(exact{*r}, want);
                else
                    EXPECT_EQ(exact{r}, want);
            }
        }
}
template <typename X>
void expect_algebraic_matches_rational() {
    for (const X x : inside_range<grid_of<X>, flags_of<X>>{}) {
        const rational r{x};
        EXPECT_EQ(rational{math::abs(x)}, abs(r));
        EXPECT_EQ(rational{math::floor(x)}, rational{floor(r)});
        EXPECT_EQ(rational{math::ceil(x)}, rational{ceil(r)});
        EXPECT_EQ(rational{math::round(x)}, rational{round(r)});
        EXPECT_EQ(rational{math::trunc(x)}, rational{trunc(r)});
    }
}

// tan at a pole on a one-limb output: the "unresolved" error ~0 read as −1,
// inverted the interval, and decided 0.
// (The input's branch now ends just below π/2: a grid holding the pole is a
// compile error, fail/tan_pole.)
TEST(MathTest, tan_near_a_pole_overflows) {
    constexpr imax top = 1727108826178; // ⌊2^40·π/2⌋: the last point below the pole
    using In           = inside<{{0, rational{top, imax{1} << 40}}, per<(std::uint64_t{1} << 40)>}, round_nearest>;
    using Out          = inside<{{-4, 4}, per<8>}, round_nearest>;
    using Clamped      = inside<{{-4, 4}, per<8>}, round_nearest | clamp>;
    for (int d = 0; d <= 2; ++d) {
        const auto x = In::from_raw(static_cast<In::raw_type>(top - d));
        try {
            (void)math::tan_into<Out>(x);
            ADD_FAILURE() << "no error reported";
        } catch (const inside_error& e) {
            EXPECT_EQ(e.Code, errc::overflow);
        }
        EXPECT_EQ(math::tan_into<Clamped>(x), 4);
    }
}

// A deduced result needs no rounding mode on its input: it rounds by the
// input's mode, else to nearest, and its type carries that mode.
TEST(MathTest, deduced_output_rounds_to_nearest_by_default) {
    const inside<{0, 3}> plain{1}; // checked, no rounding mode
    const auto           s = math::sin(plain);
    static_assert(has_flag(flags_of<decltype(s)>, round_nearest));
    EXPECT_EQ(s, 1);                                         // sin 1 ≈ 0.84: the nearest integer
    EXPECT_EQ(math::sin(inside<{0, 3}, round_floor>{1}), 0); // the input's own mode
    EXPECT_EQ(math::sqrt(inside<{0, 9}>{2}), 1);             // √2 ≈ 1.41
}

// The range gates of the math kernels read a grid's largest magnitude: one
// reaching 2^63 wrapped to −1 and passed every gate.
TEST(MathTest, gates_fail_closed_on_huge_grids) { static_assert(math::detail::engine::in_max<uint64> > 1e18); }

// pow of an exact rational result with a non-integer exponent sat on a
// rounding boundary and was guessed at the precision cap.
TEST(MathTest, pow_with_a_rational_exponent_is_exact) {
    using half   = inside<{{0, 2}, per<2>}>;
    using qtr    = inside<{{0, 4}, per<4>}>;
    using up4    = inside<{{0, 4}, per<4>}, round_ceil>;
    using down4  = inside<{{0, 4}, per<4>}, round_floor>;
    using up8    = inside<{{0, 4}, per<8>}, round_ceil>;
    using down16 = inside<{0, 16}, round_floor>;
    EXPECT_EQ(math::pow_into<up4>(qtr{2.25_r}, half{0.5_r}), 1.5_r);
    EXPECT_EQ(math::pow_into<down4>(qtr{2.25_r}, half{0.5_r}), 1.5_r);
    EXPECT_EQ(math::pow_into<up8>(qtr{2.25_r}, half{1.5_r}), 3.375_r);
    EXPECT_EQ(math::pow_into<down16>(inside<{0, 16}>{4}, half{0.5}), 2);
}

// A second exponent sign was accepted ("1e-+5" read as 1e5).
TEST(MathTest, pow_exponent_one_keeps_the_sign) {
    using B = inside<{0, 1'000'000}>;
    EXPECT_EQ(parse<B>(std::string_view{"1e-+5"}).error(), errc::invalid_format);
    EXPECT_EQ(parse<B>(std::string_view{"1e+-5"}).error(), errc::invalid_format);
    EXPECT_EQ(*parse<B>(std::string_view{"1e+5"}), 100000);
}

// pow with a zero base: 0^y = 0 for y > 0, 0^0 = 1, a pole for y < 0.
TEST(MathTest, pow_of_a_zero_base) {
    using B   = inside<{{0, 4}, per<4>}>;
    using E   = inside<{{-2, 2}, per<2>}>;
    using Out = inside<{{0, 16}, per<16>}, round_nearest>;
    EXPECT_EQ(*math::pow_into<Out>(B{0}, E{1.5}), 0);
    EXPECT_EQ(*math::pow_into<Out>(B{0}, E{0}), 1);
    EXPECT_EQ(math::pow_into<Out>(B{0}, E{-1}).error(), errc::division_by_zero);
    EXPECT_EQ(*math::pow_into<Out>(B{2}, E{2}), 4);
    // (A base grid reaching below 0 is a compile error: fail/pow_negative_base.)
}

// floor / ceil / round / trunc of a value past INT64_MAX narrowed through
// imax.
TEST(MathTest, integer_roundings_past_int64) {
    const uint64 q{18446744073709551615u};
    EXPECT_EQ(math::floor(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::ceil(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::round(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::trunc(q).to<std::uint64_t>(), 18446744073709551615u);
}

// fmod's rational path divided loudly: a quotient past 64 bits stopped the
// build instead of taking the exact path.
TEST(MathTest, fmod_quotient_past_64_bits) {
    using X      = inside<{{0, 1000}, rational{1, 1024}}, round_nearest>;
    using Y      = inside<{{0, 1}, rational{1, 1ll << 60}}, round_nearest>;
    const auto r = math::fmod(X{rational{999999, 1024}}, Y{rational{17, 1ll << 60}});
    EXPECT_EQ(rational{*r}, (rational{15, 1ll << 60}));
}

// A wrap output of exp, exp2, sinh, cosh or pow_base stood in 2^(KMax+2) for
// a result past Out's range, and decide() took any two indices past it as
// one: the wrapped slot was wrong.
TEST(MathTest, wrap_output_folds_the_true_value) {
    using In   = inside<{{-20, 20}, rational{1, 16}}>;
    using Wide = inside<{{-rational{1ll << 40}, rational{1ll << 40}}, rational{1, 1024}}, round_nearest>;
    using Wrap = inside<{{-1, 1}, rational{1, 1024}}, round_nearest | wrap>;
    int bad    = 0;
    for (imax k = -320; k <= 320; k += 3) {
        const In x{rational{k, 16}};
        bad += !(Wrap{math::exp_into<Wide>(x), wrap} == math::exp_into<Wrap>(x));
        bad += !(Wrap{math::exp2_into<Wide>(x), wrap} == math::exp2_into<Wrap>(x));
        bad += !(Wrap{math::sinh_into<Wide>(x), wrap} == math::sinh_into<Wrap>(x));
        bad += !(Wrap{math::cosh_into<Wide>(x), wrap} == math::cosh_into<Wrap>(x));
        bad += !(Wrap{math::pow_base_into<Wide, 3>(x), wrap} == math::pow_base_into<Wrap, 3>(x));
    }
    EXPECT_EQ(bad, 0);
    // pow, with a base reaching 0 (bounded: the exponent is not negative).
    using B = inside<{{0, 9}, rational{1, 4}}>;
    using P = inside<{{0, 6}, rational{1, 2}}>;
    for (int i = 0; i <= 36; i += 5)
        for (int e = 0; e <= 12; ++e) {
            const B b{rational{i, 4}};
            const P p{rational{e, 2}};
            bad += !(Wrap{math::pow_into<Wide>(b, p), wrap} == math::pow_into<Wrap>(b, p));
        }
    EXPECT_EQ(bad, 0);
}

// asinh formed x² + 1 in a word sized for |x|: past about 2^(S/2) the square
// overflowed and the result came out near log|x|, ln 2 short.
TEST(MathTest, asinh_of_a_large_argument) {
    using I64 = std::int64_t;
    using O   = inside<{{0, 64}, 0x1p-56}, round_floor>;
    // floor(asinh(1e9)·2^56) (mpmath, 300 bits).
    const auto r = math::asinh_into<O>(inside<{0, 1000000000}>{1000000000});
    EXPECT_EQ(rational(r), (rational{1543215194964068268ull, 1ull << 56}));
    static_assert([] {
        using P      = inside<{{0, 64}, 0x1p-20}, round_nearest>;
        const auto c = math::asinh_into<P>(inside<{0, I64{1} << 40}>{I64{1} << 40});
        return c > 28.41_r && c < 28.43_r; // asinh(2^40) = 28.4190_r…; log(2^40) is 27.7_r
    }());
}

// A deduced exp (or pow) whose range reaches a tiny result (e^-57 ≈ 2^-82)
// divided by zero while forming its bound.
TEST(MathTest, deduced_exp_with_a_tiny_endpoint) {
    using X = inside<{-57, 10}, round_nearest>;
    EXPECT_EQ(math::exp(X{-57}), 0);
    EXPECT_EQ(math::exp(X{0}), 1);
}

// abs(INT64_MIN) negated it in imax.
TEST(MathTest, abs_of_int64_min) {
    using I64             = std::int64_t;
    constexpr I64 I64_MIN = std::numeric_limits<I64>::min();
    using X               = inside<{I64_MIN, 0}>;
    static_assert(math::abs(X{I64_MIN}) == rational{std::uint64_t{1} << 63});
    EXPECT_EQ(math::abs(X{-5}), 5);
}

//---------------------------------------------------------------------------
// raw_from_offset (inside/detail/storage.hpp) with a signed offset, reached from the
// integer fast path of math::fmod (fmod_int_fast, math.hpp). Needs non-rational integer grids and a divisor
// that excludes zero. math::fmod was otherwise only static_assert-tested.
//---------------------------------------------------------------------------
// math::fmod integer fast path (raw_from_offset imax)
TEST(MathTest, fmod_integer_fast_path) {
    using in_t      = inside<{{-8, 8}, per<16384>}, round_nearest>; // integer-backed
    using divisor_t = inside<{{1, 8}, per<16384>}, round_nearest>;  // excludes zero
    using out_t     = inside<{{-8, 8}, per<16384>}, round_nearest>;

    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{7_r}, divisor_t{3_r})) == 1));
    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{-7_r}, divisor_t{3_r})) == -1)); // signed offset
    ASSERT_TRUE((static_cast<rational>(math::fmod<out_t>(in_t{5.5_r}, divisor_t{2_r})) == rational{3u, 2}));
}

TEST(MathTest, integer_copysign_and_fmod_match_rationals) {
    expect_copysign_fmod_match_rational<inside<{{-2, 2}, per<4>}, round_nearest>,
                                        inside<{{-3, 3}, per<2>}, round_nearest>>();
    expect_copysign_fmod_match_rational<inside<{0, 20}, round_nearest>, inside<{1, 7}, round_nearest>>();
    expect_copysign_fmod_match_rational<inside<{{-1, 3}, per<10>}, round_nearest>,
                                        inside<{{-1, 1}, per<5>}, round_nearest>>();
}

TEST(MathTest, integer_algebraic_fast_path_matches_rationals) {
    expect_algebraic_matches_rational<inside<{{-8, 8}, per<4>}, round_nearest>>();
    expect_algebraic_matches_rational<inside<{{-3, 5}, per<10>}, round_nearest>>();
    expect_algebraic_matches_rational<inside<{{0, 6}, rational{3, 4}}, round_nearest>>();
}

TEST(MathTest, sign_grid_follows_the_input_signs) {
    using S = inside<{-50, 50}>;
    static_assert(grid_of<decltype(math::sign(S{0}))> == grid{-1, 1});
    EXPECT_EQ(math::sign(S{-7}), -1);
    EXPECT_EQ(math::sign(S{0}), 0);
    EXPECT_EQ(math::sign(S{7}), 1);

    using U = inside<{0, 100}>;
    static_assert(grid_of<decltype(math::sign(U{0}))> == grid{0, 1});
    using P = inside<{{1, 4}, per<4>}>;
    static_assert(grid_of<decltype(math::sign(P{2}))> == grid{1, 1});
    using N = inside<{{-3, -0.5}, per<2>}>;
    EXPECT_EQ(math::sign(N{-0.5}), -1);
}

TEST(MathTest, copysign_takes_the_magnitude_and_the_sign) {
    using M = inside<{{-4, 2}, per<2>}>;
    using S = inside<{-1, 1}>;
    using R = decltype(math::copysign(M{0}, S{0}));
    static_assert(grid_of<R> == grid{{-4, 4}, per<2>});
    EXPECT_EQ((rational{math::copysign(M{-1.5}, S{1})}), (rational{3, 2}));
    EXPECT_EQ((rational{math::copysign(M{1.5}, S{-1})}), (rational{-3, 2}));
    EXPECT_EQ((rational{math::copysign(M{-1.5}, S{0})}), (rational{3, 2})); // 0 counts as +

    using Pos = inside<{2, 5}>; // |mag| ∈ [2, 5]
    using Neg = inside<{-3, -1}>;
    static_assert(grid_of<decltype(math::copysign(Pos{2}, Neg{-1}))> == grid{-5, -2});
}

} // namespace pinned_tests
