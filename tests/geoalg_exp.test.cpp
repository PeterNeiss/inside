// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// math::exp of multivectors (units/detail/geoalg_exp.hpp): every part
// correctly rounded against a long double reference of the closed form, in
// the trig, hyperbolic, nilpotent and mixed cases; the exact cases; constant
// evaluation; the deduced output and the CoDomain's rules; and, under
// INSIDE_MATH_INSTRUMENT, the fast path bit for bit against the
// integer path (IntegerOnly).

#define INSIDE_MATH_INSTRUMENT 1
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

#include "support.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace geoalg_exp_tests {
using namespace ins;
using ins::math::driver;

namespace {
// A named output that can miss is a std::expected; the sweeps here stay inside its range.
template <class R>
auto unwrap(const R& r) {
    if constexpr (requires { r.has_value(); })
        return r.value();
    else
        return r;
}

// G(3,0)
inline constexpr auto e1 = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>;
inline constexpr auto e2 = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>;
inline constexpr auto e3 = basis_vector<"e₃", geometric<+1>, {.ascii = "e3"}>;
// -(e₁e₃)
inline constexpr auto i = unit<"i", e1 * e2>;
inline constexpr auto j = unit<"j", e3 * e1>;
inline constexpr auto k = unit<"k", e2 * e3>;
// A null vector, and spacetime: γ₀ (+1), γ₁ γ₂ (−1).
inline constexpr auto eps = basis_vector<"ε", geometric<0>, {.ascii = "eps"}>;
inline constexpr auto g0  = basis_vector<"γ₀", geometric<+1>, {.ascii = "g0"}>;
inline constexpr auto g1  = basis_vector<"γ₁", geometric<-1>, {.ascii = "g1"}>;
inline constexpr auto g2  = basis_vector<"γ₂", geometric<-1>, {.ascii = "g2"}>;
inline constexpr auto g3  = basis_vector<"γ₃", geometric<-1>, {.ascii = "g3"}>;

template <policy_flag M>
using in_t = id<key{grid{{-4, 4}, per<64>}, M}>;
template <policy_flag M>
using out_t = id<key{grid{{-64, 64}, per<1 << 20>}, M}>;
template <policy_flag M>
using fine_t = id<key{grid{{-64, 64}, per<(std::int64_t{1} << 40)>}, M}>;
template <policy_flag M>
using dec_t = id<key{grid{{-64, 64}, rational{1, 1000}}, M}>;

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
    using I   = in_t<round_half_away>;
    using O   = Out<M>;
    using dom = multivector<I, id<quantity_key<i, I>>>;
    using cod = multivector<O, id<quantity_key<i, O>>>;
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
    EXPECT_EQ((complex_mismatches<out_t, round_half_away>(3)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_floor>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_ceil>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_toward_zero>(7)), 0);
    EXPECT_EQ((complex_mismatches<out_t, round_half_even>(7)), 0);
    EXPECT_EQ((complex_mismatches<dec_t, round_half_away>(7)), 0);
    EXPECT_EQ((complex_mismatches<fine_t, round_half_away>(29)), 0);
}

TEST(GeoalgExpTest, rotors_from_bivectors) {
    using I   = in_t<round_half_away>;
    using O   = out_t<round_half_away>;
    using dom = multivector<id<quantity_key<i, I>>, id<quantity_key<j, I>>, id<quantity_key<k, I>>>;
    using cod = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>;
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
                bad += !rounded_ok<O, round_half_away>(coef(std::get<0>(r.parts())), c.C);
                bad += !rounded_ok<O, round_half_away>(coef(std::get<1>(r.parts())), c.S * y[0]);
                bad += !rounded_ok<O, round_half_away>(coef(std::get<2>(r.parts())), c.S * y[1]);
                bad += !rounded_ok<O, round_half_away>(coef(std::get<3>(r.parts())), c.S * y[2]);
            }
    EXPECT_EQ(bad, 0);
    // A quarter turn about e₃ (θ = π/2 in the e₁e₂ plane): exp(-i π/4).
    const dom q{test::store<I>(rational{-50, 64}) * i + I{0} * j + I{0} * k};
    const cod r = math::exp<cod>(q);
    EXPECT_TRUE(std::get<2>(r.parts()) == 0_q * j); // exact 0
    EXPECT_TRUE(std::get<3>(r.parts()) == 0_q * k);
}

TEST(GeoalgExpTest, hyperbolic_nilpotent_and_mixed) {
    using I = in_t<round_half_away>;
    using O = out_t<round_half_away>;
    int bad = 0;
    // a + b e₁, e₁² = +1: e^a (cosh b + e₁ sinh b).
    using hdom = multivector<I, id<quantity_key<e1, I>>>;
    using hcod = multivector<O, id<quantity_key<e1, O>>>;
    // a + b ε, ε² = 0: e^a (1 + b ε).
    using ndom = multivector<I, id<quantity_key<eps, I>>>;
    using ncod = multivector<O, id<quantity_key<eps, O>>>;
    // a ≤ 1.5 and |b| ≤ 2.5 keep e^a cosh b within CoDomain's [-64, 64].
    for (int ra = -192; ra <= 96; ra += 11)
        for (int rb = -160; rb <= 160; rb += 13) {
            const long double a = ra / 64.0L, b = rb / 64.0L;
            {
                const hcod r =
                    unwrap(math::exp<hcod>(hdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * e1}));
                const int    sq[] = {1};
                const closed c    = reference(a, &b, sq, 1);
                bad += !rounded_ok<O, round_half_away>(coef(std::get<0>(r.parts())), c.E * c.C);
                bad += !rounded_ok<O, round_half_away>(coef(std::get<1>(r.parts())), c.E * c.S * b);
            }
            {
                const ncod r =
                    unwrap(math::exp<ncod>(ndom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * eps}));
                bad += !rounded_ok<O, round_half_away>(coef(std::get<0>(r.parts())), std::exp(a));
                bad += !rounded_ok<O, round_half_away>(coef(std::get<1>(r.parts())), std::exp(a) * b);
            }
        }
    // A named output whose parts do not hold every value is an expected: e³·cosh 4 ≈ 548 > 64 misses,
    // e·cosh 1 fits; a rotor's parts on [-1, 1] hold every value (|cos|, |sin| ≤ 1 exactly), so it stays plain.
    static_assert(std::same_as<decltype(math::exp<hcod>(hdom{})), std::expected<hcod, error>>);
    EXPECT_EQ(math::exp<hcod>(hdom{I{3} + I{4} * e1}).error(), error::overflow);
    EXPECT_EQ(math::exp<ncod>(ndom{I{3} + I{4} * eps}).error(), error::overflow);
    EXPECT_TRUE(math::exp<hcod>(hdom{I{1} + I{1} * e1}).has_value());
    using unit_part = id<"[-1..1] 1/1024 round_half_away">;
    using rcod1     = multivector<unit_part, id<"[-1..1] 1/1024 [i] round_half_away">>;
    static_assert(std::same_as<decltype(math::exp<rcod1>(id<quantity_key<i, I>>{})), rcod1>);
    // A boost and a rotation that share γ₁: they anticommute, and z changes sign.
    using mdom = multivector<id<quantity_key<g0 * g1, I>>, id<quantity_key<g1 * g2, I>>>;
    using mcod = multivector<O, id<quantity_key<g0 * g1, O>>, id<quantity_key<g1 * g2, O>>>;
    for (int u = -256; u <= 256; u += 17)
        for (int v = -256; v <= 256; v += 19) {
            const long double y[2]  = {u / 64.0L, v / 64.0L};
            const int         sq[2] = {1, -1};
            const closed      c     = reference(0, y, sq, 2);
            const mcod        r     = unwrap(math::exp<mcod>(
                mdom{test::store<I>(rational{u, 64}) * (g0 * g1) + test::store<I>(rational{v, 64}) * (g1 * g2)}));
            bad += !rounded_ok<O, round_half_away>(coef(std::get<0>(r.parts())), c.C);
            bad += !rounded_ok<O, round_half_away>(coef(std::get<1>(r.parts())), c.S * y[0]);
            bad += !rounded_ok<O, round_half_away>(coef(std::get<2>(r.parts())), c.S * y[1]);
        }
    EXPECT_EQ(bad, 0);
}

TEST(GeoalgExpTest, exact_cases_and_constant_evaluation) {
    using I   = in_t<round_floor>;
    using O   = out_t<round_floor>;
    using dom = multivector<I, id<quantity_key<i, I>>>;
    using cod = multivector<O, id<quantity_key<i, O>>>;
    // exp(0) = 1 exactly, under floor: no neighbour below.
    static_assert(math::exp<cod>(dom{}) == one);
    // A light-like boost plus rotation: z = 0, exp(y) = 1 + y exactly.
    using mdom = multivector<id<quantity_key<g0 * g1, I>>, id<quantity_key<g1 * g2, I>>>;
    using mcod = multivector<O, id<quantity_key<g0 * g1, O>>, id<quantity_key<g1 * g2, O>>>;
    const mdom n{test::store<I>(rational{3, 4}) * (g0 * g1) + test::store<I>(rational{3, 4}) * (g1 * g2)};
    EXPECT_TRUE((math::exp<mcod>(n) == 1_q + n));
    // A compile-time value equals the run-time one.
    constexpr dom x{I{1} + I{rational{1, 2}} * i};
    constexpr cod c = math::exp<cod>(x);
    EXPECT_TRUE((c == math::exp<cod>(x)));
    // A part CoDomain has beyond the argument's blades is 0; a signed
    // spelling (j = -e₁e₃) gets its sign.
    using qdom   = id<quantity_key<j, I>>;
    using qcod   = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>>;
    const qcod q = math::exp<qcod>(qdom{test::store<I>(rational{1, 2}), j});
    EXPECT_TRUE(std::get<1>(q.parts()) == 0_q * i);
    EXPECT_NEAR(static_cast<double>(coef(std::get<2>(q.parts()))), std::sin(0.5), 1e-6);
}

// The deduced output: the scalar part and every blade of the argument, each
// part's range a compile-time bound (so the decides skip their range
// checks), on the argument's notch or the one named; a named number is every
// part's.
TEST(GeoalgExpTest, the_deduced_output) {
    using I   = in_t<round_half_away>;
    using O   = out_t<round_half_away>;
    using dom = multivector<id<quantity_key<i, I>>, id<quantity_key<j, I>>, id<quantity_key<k, I>>>;
    using cod = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>;
    const dom x =
        test::store<I>(rational{5, 8}) * i + test::store<I>(rational{-3, 8}) * j + test::store<I>(rational{1, 4}) * k;
    // A rotor: every part within ±1, its range 1 + one notch.
    using R = decltype(math::exp<per<1 << 20>>(x));
    using P = id<key{grid{{-1 - rational{1, 1 << 20}, 1 + rational{1, 1 << 20}}, per<1 << 20>}, round_half_away}>;
    static_assert(std::same_as<R, multivector<P, id<quantity_key<i, P>>, id<quantity_key<j, P>>, id<quantity_key<k, P>>>>);
    static_assert(ins::detail::gaexp::facts<dom, R>::in_range == std::array{true, true, true, true});
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
    using cdom = multivector<I, id<quantity_key<i, I>>>;
    using C    = decltype(math::exp<per<1024>>(cdom{I{2} + I{1} * i}));
    static_assert(upper_of<std::tuple_element_t<0, C::parts_type>> >= 54 &&
                  upper_of<std::tuple_element_t<0, C::parts_type>> < 55); // e^4 = 54.598…
    // A boost (hyperbolic) and a dual number deduce too.
    EXPECT_NEAR(
        static_cast<double>(coef(std::get<0>(math::exp<per<1 << 20>>(id<quantity_key<g0 * g1, I>>{1, g0 * g1}).parts()))),
        std::cosh(1.0),
        1e-6);
    EXPECT_EQ(coef(std::get<1>(math::exp<per<64>>(id<quantity_key<eps, I>>{3, eps}).parts())), 3); // e^(3ε) = 1 + 3ε
}

// A deleted overload is visible only through a template: a non-template
// requires-expression naming it is a hard error.
template <class D, class C>
concept can_exp = requires(const D& d) { math::exp<C>(d); };

TEST(GeoalgExpTest, the_closed_form_is_required) {
    using I = in_t<round_half_away>;
    using O = out_t<round_half_away>;
    // e₁e₂ and γ... in 4-D: two commuting bivectors.
    using four = multivector<id<quantity_key<g0 * g1, I>>, id<quantity_key<g2 * g3, I>>>;
    using cod  = multivector<O, id<quantity_key<g0 * g1, O>>, id<quantity_key<g2 * g3, O>>, id<quantity_key<g0 * g1 * g2 * g3, O>>>;
    static_assert(!can_exp<four, cod>);
    using ok = multivector<id<quantity_key<i, I>>, id<quantity_key<j, I>>, id<quantity_key<k, I>>>;
    static_assert(can_exp<ok, multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>>);
}

template <policy_flag M>
using p52_t = id<key{grid{{-8, 8}, per<(std::int64_t{1} << 52)>}, M}>;

// make(u/16, v/16) for |u| ≤ U, |v| ≤ V, every step-th: exp onto Cod by the
// fast path against IntegerOnly.
template <class Dom, class Cod, class Make>
int same_as_integer(int U, int V, int step, Make make) {
    int bad = 0;
    for (int u = -U; u <= U; u += step)
        for (int v = -V; v <= V; v += step) {
            const Dom x        = make(rational{u, 16}, rational{v, 16});
            driver.IntegerOnly = false;
            const Cod f        = unwrap(math::exp<Cod>(x));
            driver.IntegerOnly = true;
            const Cod s        = unwrap(math::exp<Cod>(x));
            driver.IntegerOnly = false;
            bad += !(f == s);
        }
    return bad;
}

// The fast tiers decide only what the integer path decides the same way.
TEST(GeoalgExpTest, fast_path_equals_the_integer_path) {
    using I   = id<"[-4..4] 1/16 round_half_away">;
    using O   = out_t<round_half_away>;
    using dom = multivector<I, id<quantity_key<i, I>>>;
    using cod = multivector<O, id<quantity_key<i, O>>>;
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
    using J    = id<"[-4..4] 1/4 round_half_away">;
    using rdom = multivector<id<"[-4..4] 1/4 [i] round_half_away">, id<"[-4..4] 1/4 [j] round_half_away">, id<"[-4..4] 1/4 [k] round_half_away">>;
    using rcod = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>;
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
    using mdom = multivector<id<quantity_key<g0 * g1, I>>, id<quantity_key<g1 * g2, I>>>;
    using mcod = multivector<O, id<quantity_key<g0 * g1, O>>, id<quantity_key<g1 * g2, O>>>;
    int bad2   = 0;
    for (int u = -48; u <= 48; ++u)
        for (int v = -48; v <= 48; ++v) {
            const mdom x{test::store<I>(rational{u, 16}) * (g0 * g1) + test::store<I>(rational{v, 16}) * (g1 * g2)};
            driver.IntegerOnly = false;
            const mcod f       = unwrap(math::exp<mcod>(x));
            driver.IntegerOnly = true;
            const mcod s       = unwrap(math::exp<mcod>(x));
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
    using F40 = fine_t<round_half_away>;
    using F52 = p52_t<round_floor>;
    using N52 = p52_t<round_half_away>;
    EXPECT_EQ((same_as_integer<dom, multivector<F40, id<quantity_key<i, F40>>>>(64, 64, 2, cplx)), 0);
    EXPECT_EQ((same_as_integer<dom, multivector<F52, id<quantity_key<i, F52>>>>(32, 64, 3, cplx)), 0);
    EXPECT_EQ(
        (same_as_integer<mdom, multivector<N52, id<quantity_key<g0 * g1, N52>>, id<quantity_key<g1 * g2, N52>>>>(32, 32, 1, mixed)),
        0);
    // One part of y: split-complex (e₁² = 1) and dual (ε² = 0) numbers.
    using hdom       = multivector<I, id<quantity_key<e1, I>>>;
    using ndom       = multivector<I, id<quantity_key<eps, I>>>;
    const auto split = [](rational a, rational b) { return hdom{test::store<I>(a) + test::store<I>(b) * e1}; };
    const auto dual  = [](rational a, rational b) { return ndom{test::store<I>(a) + test::store<I>(b) * eps}; };
    EXPECT_EQ((same_as_integer<hdom, multivector<O, id<quantity_key<e1, O>>>>(32, 32, 1, split)), 0);
    EXPECT_EQ((same_as_integer<hdom, multivector<N52, id<quantity_key<e1, N52>>>>(16, 24, 1, split)), 0);
    EXPECT_EQ((same_as_integer<ndom, multivector<O, id<quantity_key<eps, O>>>>(32, 64, 1, dual)), 0);
    EXPECT_EQ((same_as_integer<ndom, multivector<N52, id<quantity_key<eps, N52>>>>(16, 32, 1, dual)), 0);
    using r52 = multivector<N52, id<quantity_key<i, N52>>, id<quantity_key<j, N52>>, id<quantity_key<k, N52>>>;
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
// log, part by part, against the closed form in long double.
TEST(GeoalgLogTest, complex_quaternion_boost_and_dual) {
    using I = in_t<round_half_away>;
    using O = out_t<round_half_away>;
    int bad = 0;
    // log(a + b i) = ½ log(a² + b²) + atan2(b, a) i; at b = 0, a < 0 the principal value π i.
    using cdom = multivector<I, id<quantity_key<i, I>>>;
    using ccod = multivector<O, id<quantity_key<i, O>>>;
    for (int ra = -256; ra <= 256; ra += 8)
        for (int rb = -256; rb <= 256; rb += 9) {
            const auto r = math::log<ccod>(cdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * i});
            if (ra == 0 && rb == 0) {
                bad += r.has_value();
                continue;
            }
            const long double a = ra / 64.0L, b = rb / 64.0L;
            bad += !r.has_value() ||
                   !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), 0.5L * std::log(a * a + b * b)) ||
                   !rounded_ok<O, round_half_away>(coef(std::get<1>(r->parts())), std::atan2(b, a));
        }
    EXPECT_EQ(bad, 0);
    // Quaternions: log(s + v) = ½ log(s² + |v|²) + v·atan2(|v|, s)/|v|; a negative scalar has no axis.
    using qdom = multivector<I, id<quantity_key<i, I>>, id<quantity_key<j, I>>, id<quantity_key<k, I>>>;
    using qcod = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>;
    int bad_q = 0;
    for (int s = -192; s <= 192; s += 48)
        for (int u = -128; u <= 128; u += 64)
            for (int v = -128; v <= 128; v += 64)
                for (int w = -128; w <= 128; w += 64) {
                    const auto r = math::log<qcod>(qdom{test::store<I>(rational{s, 64}) +
                                                        test::store<I>(rational{u, 64}) * i +
                                                        test::store<I>(rational{v, 64}) * j +
                                                        test::store<I>(rational{w, 64}) * k});
                    const long double a = s / 64.0L, y[] = {u / 64.0L, v / 64.0L, w / 64.0L};
                    const long double n = std::sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
                    if (n == 0 && a <= 0) {
                        bad_q += !(!r.has_value() && r.error() == error::domain_error);
                        continue;
                    }
                    const long double f = n == 0 ? 1 / a : std::atan2(n, a) / n;
                    bad_q += !r.has_value() ||
                             !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), 0.5L * std::log(a * a + n * n));
                    if (r.has_value()) {
                        bad_q += !rounded_ok<O, round_half_away>(coef(std::get<1>(r->parts())), y[0] * f);
                        bad_q += !rounded_ok<O, round_half_away>(coef(std::get<2>(r->parts())), y[1] * f);
                        bad_q += !rounded_ok<O, round_half_away>(coef(std::get<3>(r->parts())), y[2] * f);
                    }
                }
    EXPECT_EQ(bad_q, 0);
    // A boost, e₁² = +1: log(a + b e₁) = ½ log(a² − b²) + atanh(b/a) e₁ for a > |b|, else no value.
    using hdom = multivector<I, id<quantity_key<e1, I>>>;
    using hcod = multivector<O, id<quantity_key<e1, O>>>;
    int bad_h  = 0;
    for (int ra = -256; ra <= 256; ra += 8)
        for (int rb = -256; rb <= 256; rb += 9) {
            const auto r = math::log<hcod>(hdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * e1});
            const long double a = ra / 64.0L, b = rb / 64.0L;
            if (!(a > std::fabs(b))) {
                bad_h += !(!r.has_value() && r.error() == error::domain_error);
                continue;
            }
            bad_h += !r.has_value() ||
                     !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), 0.5L * std::log(a * a - b * b)) ||
                     !rounded_ok<O, round_half_away>(coef(std::get<1>(r->parts())), std::atanh(b / a));
        }
    EXPECT_EQ(bad_h, 0);
    // Dual numbers, ε² = 0: log(a + b ε) = log a + (b/a) ε, the second part exact before its rounding.
    using ndom = multivector<I, id<quantity_key<eps, I>>>;
    using ncod = multivector<O, id<quantity_key<eps, O>>>;
    int bad_n  = 0;
    for (int ra = 8; ra <= 256; ra += 8)
        for (int rb = -256; rb <= 256; rb += 9) {
            const auto r = math::log<ncod>(ndom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * eps});
            const long double a = ra / 64.0L, b = rb / 64.0L;
            bad_n += !r.has_value() || !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), std::log(a)) ||
                     !rounded_ok<O, round_half_away>(coef(std::get<1>(r->parts())), b / a);
        }
    EXPECT_EQ(bad_n, 0);
}

TEST(GeoalgLogTest, domain_types_and_compile_time) {
    using I    = in_t<round_half_away>;
    using O    = out_t<round_half_away>;
    using P    = id<"[1..4] 1/64 round_half_away">;
    using ccod = multivector<O, id<quantity_key<i, O>>>;
    // s on [1, 4] proves the domain, and the parts fit: plain. Where s can be ≤ 0: an expected.
    using pdom = multivector<P, id<quantity_key<i, I>>>;
    using cdom = multivector<I, id<quantity_key<i, I>>>;
    static_assert(std::same_as<decltype(math::log<ccod>(pdom{})), ccod>);
    static_assert(std::same_as<decltype(math::log<ccod>(cdom{})), std::expected<ccod, error>>);
    EXPECT_EQ(math::log<ccod>(cdom{}).error(), error::domain_error); // log 0
    // The principal value: log(-1) = π i.
    const auto m = math::log<ccod>(cdom{I{-1} + I{0} * i});
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE((rounded_ok<O, round_half_away>(coef(std::get<1>(m->parts())), 3.14159265358979323846L)));
    // A named output too narrow for a part is an expected with error::overflow (π > 2).
    using narrow = id<"[-2..2] 1/1024 round_half_away">;
    using ncod   = multivector<narrow, id<"[-2..2] 1/1024 [i] round_half_away">>;
    EXPECT_EQ(math::log<ncod>(cdom{I{-4} + I{1} * i}).error(), error::overflow); // atan2(1, -4) ≈ 2.9
    EXPECT_TRUE(math::log<ncod>(cdom{I{1} - I{4} * i}).has_value());             // atan2(-4, 1) ≈ -1.33
    static_assert(std::same_as<decltype(math::log<ncod>(pdom{})), std::expected<ncod, error>>);
    // The deduced output takes the argument's notch, and log 1 is 0 exactly, at compile time too.
    constexpr auto one_plus = math::log<ccod>(pdom{P{1} + I{0} * i});
    static_assert(std::get<0>(one_plus.parts()) == 0 && std::get<1>(one_plus.parts()) == 0_q * i);
    static_assert(notch_of<std::tuple_element_t<0, std::remove_cvref_t<decltype(math::log(pdom{}).parts())>>> ==
                  rational{1, 64});
}

// √, part by part, against std::complex and the closed form in long double.
TEST(GeoalgSqrtTest, complex_quaternion_boost_and_exact_roots) {
    using I = in_t<round_half_away>;
    using O = out_t<round_half_away>;
    int bad = 0;
    // Complex numbers: every value has a root (the principal one), so the result is plain.
    using cdom = multivector<I, id<quantity_key<i, I>>>;
    using ccod = multivector<O, id<quantity_key<i, O>>>;
    static_assert(std::same_as<decltype(math::sqrt<ccod>(cdom{})), ccod>);
    for (int ra = -256; ra <= 256; ra += 8)
        for (int rb = -256; rb <= 256; rb += 9) {
            const ccod r = math::sqrt<ccod>(cdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * i});
            const std::complex<long double> w = std::sqrt(std::complex<long double>(ra / 64.0L, rb / 64.0L));
            bad += !rounded_ok<O, round_half_away>(coef(std::get<0>(r.parts())), w.real()) ||
                   !rounded_ok<O, round_half_away>(coef(std::get<1>(r.parts())), w.imag());
        }
    EXPECT_EQ(bad, 0);
    // Exact roots: √(3 + 4i) = 2 + i, √(-4) = 2i, √0 = 0; in the quaternions √(2i) = 1 + i.
    EXPECT_TRUE(math::sqrt<ccod>(cdom{I{3} + I{4} * i}) == 2_q + i);
    EXPECT_TRUE(math::sqrt<ccod>(cdom{I{-4} + I{0} * i}) == 2_q * i);
    EXPECT_TRUE(math::sqrt<ccod>(cdom{}) == multivector<>{});
    using qdom = multivector<I, id<quantity_key<i, I>>, id<quantity_key<j, I>>, id<quantity_key<k, I>>>;
    using qcod = multivector<O, id<quantity_key<i, O>>, id<quantity_key<j, O>>, id<quantity_key<k, O>>>;
    EXPECT_TRUE(*math::sqrt<qcod>(qdom{I{0} + I{2} * i + I{0} * j + I{0} * k}) == 1_q + i);
    // A quaternion's negative scalar has no axis; √x · √x = x elsewhere, within the roundings.
    EXPECT_EQ(math::sqrt<qcod>(qdom{I{-1} + I{0} * i + I{0} * j + I{0} * k}).error(), error::domain_error);
    int bad_q = 0;
    for (int s = -192; s <= 192; s += 48)
        for (int u = -128; u <= 128; u += 64)
            for (int v = -128; v <= 128; v += 64)
                for (int w = -128; w <= 128; w += 64) {
                    const auto r = math::sqrt<qcod>(qdom{test::store<I>(rational{s, 64}) +
                                                         test::store<I>(rational{u, 64}) * i +
                                                         test::store<I>(rational{v, 64}) * j +
                                                         test::store<I>(rational{w, 64}) * k});
                    const long double a = s / 64.0L, y[] = {u / 64.0L, v / 64.0L, w / 64.0L};
                    const long double z = y[0] * y[0] + y[1] * y[1] + y[2] * y[2];
                    if (z == 0 && a < 0) {
                        bad_q += r.has_value();
                        continue;
                    }
                    const long double m = std::sqrt(a * a + z), ww = std::sqrt(2 * (a + m));
                    bad_q += !r.has_value() || !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), ww / 2);
                    if (r.has_value() && ww > 0)
                        for (int n = 0; n < 3; ++n) {
                            const long double got = n == 0   ? coef(std::get<1>(r->parts()))
                                                    : n == 1 ? coef(std::get<2>(r->parts()))
                                                             : coef(std::get<3>(r->parts()));
                            bad_q += !rounded_ok<O, round_half_away>(got, y[n] / ww);
                        }
                }
    EXPECT_EQ(bad_q, 0);
    // A boost: √(a + b e₁) = (√(a + b) + √(a − b))/2 + e₁·b/(√(a + b) + √(a − b)) for a ≥ |b|, else no value.
    using hdom = multivector<I, id<quantity_key<e1, I>>>;
    using hcod = multivector<O, id<quantity_key<e1, O>>>;
    int bad_h  = 0;
    for (int ra = -256; ra <= 256; ra += 8)
        for (int rb = -256; rb <= 256; rb += 9) {
            const auto r = math::sqrt<hcod>(hdom{test::store<I>(rational{ra, 64}) + test::store<I>(rational{rb, 64}) * e1});
            const long double a = ra / 64.0L, b = rb / 64.0L;
            if (a < std::fabs(b) || (a == 0 && b == 0)) {
                bad_h += (a == 0 && b == 0) ? !r.has_value() : r.has_value();
                continue;
            }
            const long double p = std::sqrt(a + b), q = std::sqrt(a - b);
            bad_h += !r.has_value() || !rounded_ok<O, round_half_away>(coef(std::get<0>(r->parts())), (p + q) / 2) ||
                     !rounded_ok<O, round_half_away>(coef(std::get<1>(r->parts())), b / (p + q));
        }
    EXPECT_EQ(bad_h, 0);
}

} // namespace geoalg_exp_tests
