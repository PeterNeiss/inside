// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// math::exp of multivectors (units/detail/geoalg_exp.hpp): every part
// correctly rounded against a long double reference of the closed form, in
// the trig, hyperbolic, nilpotent and mixed cases; the exact cases; constant
// evaluation; the deduced output and the CoDomain's rules; and, under
// BEMAN_INSIDE_MATH_INSTRUMENT, the fast path bit for bit against the
// integer path (IntegerOnly).

#define BEMAN_INSIDE_MATH_INSTRUMENT 1
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

#include "support.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace geoalg_exp_tests {
using namespace beman::inside;
using namespace beman::inside::units;
using beman::inside::math::driver;

namespace {
// G(3,0)
inline constexpr basis_vector e1_def{"e₁", +1, {.ascii = "e1"}};
inline constexpr basis_vector e2_def{"e₂", +1, {.ascii = "e2"}};
inline constexpr basis_vector e3_def{"e₃", +1, {.ascii = "e3"}};
inline constexpr auto         e1 = unit_c<e1_def>;
inline constexpr auto         e2 = unit_c<e2_def>;
inline constexpr auto         e3 = unit_c<e3_def>;
inline constexpr named_unit   i_def{"i", e1 * e2};
inline constexpr named_unit   j_def{"j", e3 * e1}; // -(e₁e₃)
inline constexpr named_unit   k_def{"k", e2 * e3};
inline constexpr auto         i = unit_c<i_def>;
inline constexpr auto         j = unit_c<j_def>;
inline constexpr auto         k = unit_c<k_def>;
// A null vector, and spacetime: γ₀ (+1), γ₁ γ₂ (−1).
inline constexpr basis_vector eps_def{"ε", 0, {.ascii = "eps"}};
inline constexpr auto         eps = unit_c<eps_def>;
inline constexpr basis_vector g0_def{"γ₀", +1, {.ascii = "g0"}};
inline constexpr basis_vector g1_def{"γ₁", -1, {.ascii = "g1"}};
inline constexpr basis_vector g2_def{"γ₂", -1, {.ascii = "g2"}};
inline constexpr basis_vector g3_def{"γ₃", -1, {.ascii = "g3"}};
inline constexpr auto         g0 = unit_c<g0_def>;
inline constexpr auto         g1 = unit_c<g1_def>;
inline constexpr auto         g2 = unit_c<g2_def>;
inline constexpr auto         g3 = unit_c<g3_def>;

template <policy_flag M>
using in_t = inside<{{-4, 4}, per<64>}, M>;
template <policy_flag M>
using out_t = inside<{{-64, 64}, per<1 << 20>}, M>;
template <policy_flag M>
using fine_t = inside<{{-64, 64}, per<(std::int64_t{1} << 40)>}, M>;
template <policy_flag M>
using dec_t = inside<{{-64, 64}, rational{1, 1000}}, M>;

long double ld(const rational& r) { return static_cast<long double>(r.Num) / static_cast<long double>(r.Den); }
template <class P>
long double coef(const P& p) {
    if constexpr (quantity_like<P>)
        return ld(static_cast<rational>(p.numerical_value_in(unit_t<P::unit>{})));
    else
        return ld(static_cast<rational>(p));
}

// The value lies within half a notch of a nearest-mode result, within one
// notch below a floor (above a ceil, toward 0 from a toward-zero) result.
// The tolerance covers the long double reference's own error (a few units
// of 2^-64, relative), far below half of the finest notch, 2^-41.
template <class O, policy_flag M>
bool rounded_ok(long double got, long double want) {
    const long double n   = ld(grid_of<O>.Notch);
    const long double tol = 1e-16L * (1 + std::fabs(want));
    if constexpr (M == round_floor)
        return got <= want + tol && want - got < n + tol;
    else if constexpr (M == round_ceil)
        return got >= want - tol && got - want < n + tol;
    else if constexpr (M == round_toward_zero)
        return want < 0 ? rounded_ok<O, round_ceil>(got, want) : rounded_ok<O, round_floor>(got, want);
    else
        return std::fabs(got - want) <= n / 2 + tol;
}

// The closed form in long double: x = a + Σ yⱼ Bⱼ with Bⱼ² = sqⱼ.
struct closed {
    long double E, C, S;
};
closed reference(long double a, const long double* y, const int* sq, int n) {
    long double z = 0;
    for (int m = 0; m < n; ++m)
        z -= sq[m] * y[m] * y[m];
    closed r{std::exp(a), 1, 1};
    if (z > 0) {
        const long double l = std::sqrt(z);
        r.C                 = std::cos(l);
        r.S                 = std::sin(l) / l;
    } else if (z < 0) {
        const long double l = std::sqrt(-z);
        r.C                 = std::cosh(l);
        r.S                 = std::sinh(l) / l;
    }
    return r;
}

// Complex numbers onto Out: every input of a coarse grid.
template <template <policy_flag> class Out, policy_flag M>
int complex_mismatches(int stride) {
    using I   = in_t<round_nearest>;
    using O   = Out<M>;
    using dom = multivector<I, quantity<i, I>>;
    using cod = multivector<O, quantity<i, O>>;
    int bad   = 0;
    for (int ra = -256; ra <= 256; ra += stride)
        for (int rb = -256; rb <= 256; rb += stride) {
            const dom         x{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * i};
            const cod         r = math::exp<cod>(x);
            const long double a = ra / 64.0L, b = rb / 64.0L;
            const int         sq[] = {-1};
            const closed      c    = reference(a, &b, sq, 1);
            bad += !rounded_ok<O, M>(coef(std::get<0>(r.parts())), c.E * c.C);
            bad += !rounded_ok<O, M>(coef(std::get<1>(r.parts())), c.E * c.S * b);
        }
    return bad;
}
} // namespace

TEST(GeoalgExpTest, complex_numbers_round_correctly) {
    EXPECT_EQ((complex_mismatches<out_t, round_nearest>(3)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_floor>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_ceil>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_toward_zero>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_half_even>(7)), 0);
    EXPECT_EQ((complex_mismatches<dec_t, round_nearest>(7)), 0);
    EXPECT_EQ((complex_mismatches<fine_t, round_nearest>(29)), 0);
}

TEST(GeoalgExpTest, rotors_from_bivectors) {
    using I   = in_t<round_nearest>;
    using O   = out_t<round_nearest>;
    using dom = multivector<quantity<i, I>, quantity<j, I>, quantity<k, I>>;
    using cod = multivector<O, quantity<i, O>, quantity<j, O>, quantity<k, O>>;
    int bad   = 0;
    for (int u = -256; u <= 256; u += 37)
        for (int v = -256; v <= 256; v += 41)
            for (int w = -256; w <= 256; w += 43) {
                const dom         x{test::store<I>(rational{u, 64}) * i + test::store<I>(rational{v, 64}) * j +
                                    test::store<I>(rational{w, 64}) * k};
                const cod         r     = math::exp<cod>(x);
                const long double y[3]  = {u / 64.0L, v / 64.0L, w / 64.0L};
                const int         sq[3] = {-1, -1, -1};
                const closed      c     = reference(0, y, sq, 3);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<0>(r.parts())), c.C);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<1>(r.parts())), c.S * y[0]);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<2>(r.parts())), c.S * y[1]);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<3>(r.parts())), c.S * y[2]);
            }
    EXPECT_EQ(bad, 0);
    // A quarter turn about e₃ (θ = π/2 in the e₁e₂ plane): exp(-i π/4).
    const dom q{test::store<I>(rational{-50, 64}) * i + I{0} * j + I{0} * k};
    const cod r = math::exp<cod>(q);
    EXPECT_TRUE(std::get<2>(r.parts()) == just<0> * j); // exact 0
    EXPECT_TRUE(std::get<3>(r.parts()) == just<0> * k);
}

TEST(GeoalgExpTest, hyperbolic_nilpotent_and_mixed) {
    using I = in_t<round_nearest>;
    using O = out_t<round_nearest>;
    int bad = 0;
    // a + b e₁, e₁² = +1: e^a (cosh b + e₁ sinh b).
    using hdom = multivector<I, quantity<e1, I>>;
    using hcod = multivector<O, quantity<e1, O>>;
    // a + b ε, ε² = 0: e^a (1 + b ε).
    using ndom = multivector<I, quantity<eps, I>>;
    using ncod = multivector<O, quantity<eps, O>>;
    // a ≤ 1.5 and |b| ≤ 2.5 keep e^a cosh b within CoDomain's [-64, 64].
    for (int ra = -192; ra <= 96; ra += 11)
        for (int rb = -160; rb <= 160; rb += 13) {
            const long double a = ra / 64.0L, b = rb / 64.0L;
            {
                const hcod r =
                    math::exp<hcod>(hdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * e1});
                const int    sq[] = {1};
                const closed c    = reference(a, &b, sq, 1);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<0>(r.parts())), c.E * c.C);
                bad += !rounded_ok<O, round_nearest>(coef(std::get<1>(r.parts())), c.E * c.S * b);
            }
            {
                const ncod r =
                    math::exp<ncod>(ndom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * eps});
                bad += !rounded_ok<O, round_nearest>(coef(std::get<0>(r.parts())), std::exp(a));
                bad += !rounded_ok<O, round_nearest>(coef(std::get<1>(r.parts())), std::exp(a) * b);
            }
        }
    // Past CoDomain's range: a named multivector output does not report as
    // an expected yet - the store reaches the handler (a known gap).
    // e³·cosh 4 ≈ 548 > 64.
    EXPECT_DEATH((void)(math::exp<hcod>(hdom{I{3} + I{4} * e1})), "beman::inside");
    EXPECT_DEATH((void)(math::exp<ncod>(ndom{I{3} + I{4} * eps})), "beman::inside");
    // A boost and a rotation that share γ₁: they anticommute, and z changes sign.
    using mdom = multivector<quantity<g0 * g1, I>, quantity<g1 * g2, I>>;
    using mcod = multivector<O, quantity<g0 * g1, O>, quantity<g1 * g2, O>>;
    for (int u = -256; u <= 256; u += 17)
        for (int v = -256; v <= 256; v += 19) {
            const long double y[2]  = {u / 64.0L, v / 64.0L};
            const int         sq[2] = {1, -1};
            const closed      c     = reference(0, y, sq, 2);
            const mcod        r     = math::exp<mcod>(
                mdom{test::store<I>(rational{u, 64}) * (g0 * g1) + test::store<I>(rational{v, 64}) * (g1 * g2)});
            bad += !rounded_ok<O, round_nearest>(coef(std::get<0>(r.parts())), c.C);
            bad += !rounded_ok<O, round_nearest>(coef(std::get<1>(r.parts())), c.S * y[0]);
            bad += !rounded_ok<O, round_nearest>(coef(std::get<2>(r.parts())), c.S * y[1]);
        }
    EXPECT_EQ(bad, 0);
}

TEST(GeoalgExpTest, exact_cases_and_constant_evaluation) {
    using I   = in_t<round_floor>;
    using O   = out_t<round_floor>;
    using dom = multivector<I, quantity<i, I>>;
    using cod = multivector<O, quantity<i, O>>;
    // exp(0) = 1 exactly, under floor: no neighbour below.
    static_assert(math::exp<cod>(dom{}) == just<1>);
    // A light-like boost plus rotation: z = 0, exp(y) = 1 + y exactly.
    using mdom = multivector<quantity<g0 * g1, I>, quantity<g1 * g2, I>>;
    using mcod = multivector<O, quantity<g0 * g1, O>, quantity<g1 * g2, O>>;
    const mdom n{test::store<I>(rational{3, 4}) * (g0 * g1) + test::store<I>(rational{3, 4}) * (g1 * g2)};
    EXPECT_TRUE((math::exp<mcod>(n) == just<1> + n));
    // A compile-time value equals the run-time one.
    constexpr dom x{I{1} + I{rational{1, 2}} * i};
    constexpr cod c = math::exp<cod>(x);
    EXPECT_TRUE((c == math::exp<cod>(x)));
    // A part CoDomain has beyond the argument's blades is 0; a signed
    // spelling (j = -e₁e₃) gets its sign.
    using qdom   = quantity<j, I>;
    using qcod   = multivector<O, quantity<i, O>, quantity<j, O>>;
    const qcod q = math::exp<qcod>(qdom{test::store<I>(rational{1, 2}), j});
    EXPECT_TRUE(std::get<1>(q.parts()) == just<0> * i);
    EXPECT_NEAR(static_cast<double>(coef(std::get<2>(q.parts()))), std::sin(0.5), 1e-6);
}

// The deduced output: the scalar part and every blade of the argument, each
// part's range a compile-time bound (so the decides skip their range
// checks), on the argument's notch or the one named; a named number is every
// part's.
TEST(GeoalgExpTest, the_deduced_output) {
    using I   = in_t<round_nearest>;
    using O   = out_t<round_nearest>;
    using dom = multivector<quantity<i, I>, quantity<j, I>, quantity<k, I>>;
    using cod = multivector<O, quantity<i, O>, quantity<j, O>, quantity<k, O>>;
    const dom x =
        test::store<I>(rational{5, 8}) * i + test::store<I>(rational{-3, 8}) * j + test::store<I>(rational{1, 4}) * k;
    // A rotor: every part within ±1, its range 1 + one notch.
    using R = decltype(math::exp<per<1 << 20>>(x));
    using P = inside<{{-1 - rational{1, 1 << 20}, 1 + rational{1, 1 << 20}}, per<1 << 20>}, round_nearest>;
    static_assert(std::same_as<R, multivector<P, quantity<i, P>, quantity<j, P>, quantity<k, P>>>);
    static_assert(units::detail::gaexp::facts<dom, R>::in_range == std::array{true, true, true, true});
    const R   r = math::exp<per<1 << 20>>(x);
    const cod c = math::exp<cod>(x);
    EXPECT_EQ(coef(std::get<0>(r.parts())), coef(std::get<0>(c.parts())));
    EXPECT_EQ(coef(std::get<1>(r.parts())), coef(std::get<1>(c.parts())));
    EXPECT_EQ(coef(std::get<2>(r.parts())), coef(std::get<2>(c.parts())));
    EXPECT_EQ(coef(std::get<3>(r.parts())), coef(std::get<3>(c.parts())));
    // On the argument's notch, and onto a number for every part.
    static_assert(notch_of<std::tuple_element_t<0, decltype(math::exp(x))::parts_type>> == rational{1, 64});
    EXPECT_EQ(math::exp<O>(x), c);
    EXPECT_EQ((math::exp<O, dom>(x)), c);
    // A complex number with a scalar part: e^amax bounds it.
    using cdom = multivector<I, quantity<i, I>>;
    using C    = decltype(math::exp<per<1024>>(cdom{I{2} + I{1} * i}));
    static_assert(upper_of<std::tuple_element_t<0, C::parts_type>> >= 54 &&
                  upper_of<std::tuple_element_t<0, C::parts_type>> < 55); // e^4 = 54.598…
    // A boost (hyperbolic) and a dual number deduce too.
    EXPECT_NEAR(
        static_cast<double>(coef(std::get<0>(math::exp<per<1 << 20>>(quantity<g0 * g1, I>{1, g0 * g1}).parts()))),
        std::cosh(1.0),
        1e-6);
    EXPECT_EQ(coef(std::get<1>(math::exp<per<64>>(quantity<eps, I>{3, eps}).parts())), 3); // e^(3ε) = 1 + 3ε
}

// A deleted overload is visible only through a template: a non-template
// requires-expression naming it is a hard error.
template <class D, class C>
concept can_exp = requires(const D& d) { math::exp<C>(d); };

TEST(GeoalgExpTest, the_closed_form_is_required) {
    using I = in_t<round_nearest>;
    using O = out_t<round_nearest>;
    // e₁e₂ and γ... in 4-D: two commuting bivectors.
    using four = multivector<quantity<g0 * g1, I>, quantity<g2 * g3, I>>;
    using cod  = multivector<O, quantity<g0 * g1, O>, quantity<g2 * g3, O>, quantity<g0 * g1 * g2 * g3, O>>;
    static_assert(!can_exp<four, cod>);
    using ok = multivector<quantity<i, I>, quantity<j, I>, quantity<k, I>>;
    static_assert(can_exp<ok, multivector<O, quantity<i, O>, quantity<j, O>, quantity<k, O>>>);
}

template <policy_flag M>
using p52_t = inside<{{-8, 8}, per<(std::int64_t{1} << 52)>}, M>;

// make(u/16, v/16) for |u| ≤ U, |v| ≤ V, every step-th: exp onto Cod by the
// fast path against IntegerOnly.
template <class Dom, class Cod, class Make>
int same_as_integer(int U, int V, int step, Make make) {
    int bad = 0;
    for (int u = -U; u <= U; u += step)
        for (int v = -V; v <= V; v += step) {
            const Dom x        = make(rational{u, 16}, rational{v, 16});
            driver.IntegerOnly = false;
            const Cod f        = math::exp<Cod>(x);
            driver.IntegerOnly = true;
            const Cod s        = math::exp<Cod>(x);
            driver.IntegerOnly = false;
            bad += !(f == s);
        }
    return bad;
}

// The fast tiers decide only what the integer path decides the same way.
TEST(GeoalgExpTest, fast_path_equals_the_integer_path) {
    using I   = inside<{{-4, 4}, per<16>}, round_nearest>;
    using O   = out_t<round_nearest>;
    using dom = multivector<I, quantity<i, I>>;
    using cod = multivector<O, quantity<i, O>>;
    int bad   = 0;
    for (int ra = -64; ra <= 64; ++ra)
        for (int rb = -64; rb <= 64; ++rb) {
            const dom x{test::store<I>(rational{ra, 16}) + test::store<I>(rational{rb, 16}) * i};
            driver.IntegerOnly = false;
            const cod f        = math::exp<cod>(x);
            driver.IntegerOnly = true;
            const cod s        = math::exp<cod>(x);
            driver.IntegerOnly = false;
            bad += !(std::get<0>(f.parts()).raw() == std::get<0>(s.parts()).raw() &&
                     std::get<1>(f.parts()).numerical_value_in(i).raw() ==
                         std::get<1>(s.parts()).numerical_value_in(i).raw());
        }
    EXPECT_EQ(bad, 0);
    using J    = inside<{{-4, 4}, per<4>}, round_nearest>;
    using rdom = multivector<quantity<i, J>, quantity<j, J>, quantity<k, J>>;
    using rcod = multivector<O, quantity<i, O>, quantity<j, O>, quantity<k, O>>;
    int bad3   = 0;
    for (int u = -16; u <= 16; ++u)
        for (int v = -16; v <= 16; ++v)
            for (int w = -16; w <= 16; ++w) {
                const rdom x{test::store<J>(rational{u, 4}) * i + test::store<J>(rational{v, 4}) * j +
                             test::store<J>(rational{w, 4}) * k};
                driver.IntegerOnly = false;
                const rcod f       = math::exp<rcod>(x);
                driver.IntegerOnly = true;
                const rcod s       = math::exp<rcod>(x);
                driver.IntegerOnly = false;
                bad3 += !(f == s);
            }
    EXPECT_EQ(bad3, 0);
    // A boost and a rotation: z = v² - u² takes both signs, so the series,
    // trig and hyperbolic branches of both paths all run.
    using mdom = multivector<quantity<g0 * g1, I>, quantity<g1 * g2, I>>;
    using mcod = multivector<O, quantity<g0 * g1, O>, quantity<g1 * g2, O>>;
    int bad2   = 0;
    for (int u = -48; u <= 48; ++u)
        for (int v = -48; v <= 48; ++v) {
            const mdom x{test::store<I>(rational{u, 16}) * (g0 * g1) + test::store<I>(rational{v, 16}) * (g1 * g2)};
            driver.IntegerOnly = false;
            const mcod f       = math::exp<mcod>(x);
            driver.IntegerOnly = true;
            const mcod s       = math::exp<mcod>(x);
            driver.IntegerOnly = false;
            bad2 += !(f == s);
        }
    EXPECT_EQ(bad2, 0);
    // Parts past the double tier (2^-40, 2^-52): the dd tier against the
    // integer path, where a long double reference is too coarse to judge.
    const auto cplx  = [](rational a, rational b) { return dom{test::store<I>(a) + test::store<I>(b) * i}; };
    const auto mixed = [](rational a, rational b) {
        return mdom{test::store<I>(a) * (g0 * g1) + test::store<I>(b) * (g1 * g2)};
    };
    using F40 = fine_t<round_nearest>;
    using F52 = p52_t<round_floor>;
    using N52 = p52_t<round_nearest>;
    EXPECT_EQ((same_as_integer<dom, multivector<F40, quantity<i, F40>>>(64, 64, 2, cplx)), 0);
    EXPECT_EQ((same_as_integer<dom, multivector<F52, quantity<i, F52>>>(32, 64, 3, cplx)), 0);
    EXPECT_EQ(
        (same_as_integer<mdom, multivector<N52, quantity<g0 * g1, N52>, quantity<g1 * g2, N52>>>(32, 32, 1, mixed)),
        0);
    // One part of y: split-complex (e₁² = 1) and dual (ε² = 0) numbers.
    using hdom       = multivector<I, quantity<e1, I>>;
    using ndom       = multivector<I, quantity<eps, I>>;
    const auto split = [](rational a, rational b) { return hdom{test::store<I>(a) + test::store<I>(b) * e1}; };
    const auto dual  = [](rational a, rational b) { return ndom{test::store<I>(a) + test::store<I>(b) * eps}; };
    EXPECT_EQ((same_as_integer<hdom, multivector<O, quantity<e1, O>>>(32, 32, 1, split)), 0);
    EXPECT_EQ((same_as_integer<hdom, multivector<N52, quantity<e1, N52>>>(16, 24, 1, split)), 0);
    EXPECT_EQ((same_as_integer<ndom, multivector<O, quantity<eps, O>>>(32, 64, 1, dual)), 0);
    EXPECT_EQ((same_as_integer<ndom, multivector<N52, quantity<eps, N52>>>(16, 32, 1, dual)), 0);
    using r52 = multivector<N52, quantity<i, N52>, quantity<j, N52>, quantity<k, N52>>;
    int bad4  = 0;
    for (int u = -16; u <= 16; u += 2)
        for (int v = -16; v <= 16; v += 2)
            for (int w = -16; w <= 16; w += 2) {
                const rdom x{test::store<J>(rational{u, 4}) * i + test::store<J>(rational{v, 4}) * j +
                             test::store<J>(rational{w, 4}) * k};
                driver.IntegerOnly = false;
                const r52 f        = math::exp<r52>(x);
                driver.IntegerOnly = true;
                const r52 s        = math::exp<r52>(x);
                driver.IntegerOnly = false;
                bad4 += !(f == s);
            }
    EXPECT_EQ(bad4, 0);
    EXPECT_EQ(driver.Guesses.load(), 0u);
}
} // namespace geoalg_exp_tests
