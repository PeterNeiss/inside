// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: the math functions on units::quantity, over the correctly rounded
// engine (math.hpp). Kept apart from units.hpp, which stays free of the
// engine's compile time. Each math::fn takes quantities too, in every call
// form (math/detail/functions.hpp):
//
//   math::sqrt(q), math::cbrt(q)       the unit's root (m² → m), the number's
//   abs, floor, ceil, round, trunc     on the number, in q's unit
//   sin, cos, tan(q)                   of an angle in radians: a number; sin
//                                      and cos also of an angle in a unit with
//                                      a factor of π (°, a turn), reduced
//                                      exactly by quarter turns, exact at
//                                      Niven's values (name the notch then:
//                                      sin<per<K>>(q))
//   asin, acos, atan(x), atan2(y, x)   an angle in radians (x a dimensionless
//                                      quantity; atan2 of two that add)
//   hypot(x, y)                        in the common unit
//   exp … atanh, pow_base<B>, pow      of numbers: dimensionless quantities
//                                      (and insides): a number
//   math::exp(x) of a multivector      units/detail/geoalg_exp.hpp
//   value_cast<si::radian, Out>(q)     degree → radian (a pi factor): the
//                                      exact x·π/180, rounded once onto Out
//
// fn<L>(q) names only the result's number L (the unit follows), fn<C>(q) the
// whole quantity, whose unit must be the result's. Every result rounds as
// the inside function does: onto the output grid, under its rounding mode.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_MATH_HPP
#define BEMAN_INSIDE_UNITS_MATH_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace beman::inside::math::detail::engine {
// x·(n/d)·π^P for P = ±1: π from pi_q at T = S + B bits, B covering |x·n/d|,
// so its error adds at most one unit; the division truncates one more. The
// factor is 64-bit (KF = 1), or KF limbs for a factor past 64 bits (ħ = h/2π
// in J·s).
template <std::size_t E, int Bits, int P, std::size_t KF = 1>
struct pi_scale_core {
    using factor = std::conditional_t<KF == 1, std::uint64_t, beman::inside::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }

    template <int W>
    constexpr auto run() const {
        constexpr int         S = W + 2;
        constexpr int         T = S + Bits + 4;
        constexpr std::size_t KJ =
            limbs_for_bits(64 * static_cast<int>(E) + 64 * static_cast<int>(KF) + T + 2 * S + 16);
        using J = wide_sint<KJ>;
        using I = fixed_t<Bits + S + 4>;
        const J pi_fixed{static_cast<J>(pi_q<T>)};
        const J num = J{X.Num} * J{N};
        const J den = J{X.Den} * J{D};
        J       y;
        if constexpr (P > 0)
            y = (num * pi_fixed) / (den << (T - S));
        else
            y = (num << (S + T)) / (den * pi_fixed);
        const I v = static_cast<I>(y);
        return approx<limbs_of<I>>{Negative ? -v : v, S, 2};
    }
};

// sin or cos of an angle x·(n/d)·π (a unit with a factor of π: °, a turn):
// t = x·n/d half turns is reduced exactly by k = round(2t) quarter turns, so
// only r = π·(t − k/2), |r| ≤ π/4, is approximated. Exact where Niven's
// theorem allows a rational value: 0, ±1/2, ±1.
template <std::size_t E, bool Cos, std::size_t KF = 1>
struct pi_trig_core {
    using factor = std::conditional_t<KF == 1, std::uint64_t, beman::inside::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    template <class J>
    constexpr J numerator() const {
        const J num = J{X.Num} * J{N};
        return Negative ? -num : num;
    }

    constexpr maybe_exact<E> exact() const {
        using J     = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + 8)>;
        const J six = numerator<J>() * J{6}, den = J{X.Den} * J{D};
        if (!(six % den).is_zero())
            return std::nullopt;
        J m = (six / den) % J{12}; // 6t mod 12: the angle in multiples of 30°
        if (m.negative())
            m += J{12};
        // 2·sin and 2·cos at m·30°; 3 marks an irrational value (√3/2).
        constexpr signed char twice_sin[12] = {0, 1, 3, 2, 3, 1, 0, -1, 3, -2, 3, -1};
        constexpr signed char twice_cos[12] = {2, 3, 1, 0, -1, 3, -2, 3, -1, 0, 1, 3};
        const int             v             = (Cos ? twice_cos : twice_sin)[m.Word[0]];
        if (v == 3)
            return std::nullopt;
        return wide_rational<E>{wide_sint<E>{v}, wide_sint<E>{2}};
    }

    template <int W>
    constexpr auto run() const {
        constexpr int S               = W + 10;
        constexpr int P               = S + 4;
        using J                       = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + P + 16)>;
        using I                       = fixed_t<S + 2>;
        constexpr std::size_t K       = limbs_of<I>;
        const J               two_num = J{2} * numerator<J>(), den = J{X.Den} * J{D};
        const J               k = nearest_div(two_num, den); // quarter turns
        // r = π·(2t − k)/2 at scale S, within 1 unit.
        const I r = static_cast<I>(nearest_div((two_num - k * den) * static_cast<J>(pi_q<P>), den << (P - S + 1)));
        const unsigned        q   = static_cast<unsigned>(k.Word[0] & 3u);
        auto                  neg = [](fixed_approx<K> f) { return fixed_approx<K>{-f.Value, f.Error}; };
        const fixed_approx<K> v   = Cos ? ((q == 0)   ? cos_series<S>(r, 2)
                                           : (q == 1) ? neg(sin_series<S>(r, 2))
                                           : (q == 2) ? neg(cos_series<S>(r, 2))
                                                      : sin_series<S>(r, 2))
                                        : ((q == 0)   ? sin_series<S>(r, 2)
                                           : (q == 1) ? cos_series<S>(r, 2)
                                           : (q == 2) ? neg(sin_series<S>(r, 2))
                                                      : neg(cos_series<S>(r, 2)));
        return approx<K>{v.Value, S, v.Error};
    }
};
} // namespace beman::inside::math::detail::engine

namespace beman::inside::units {
namespace detail {
// The n-th root of a unit: every factor's exponent must divide.
consteval unit unit_root(const unit& u, int n) {
    unit r{};
    for (std::int8_t i = 0; i < u.Size; ++i) {
        if (u.Atoms[i].Exp % n != 0)
            beman::inside::detail::constexpr_error<
                "units: the root of a unit needs exponents it divides (m², not m)">();
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, static_cast<std::int8_t>(u.Atoms[i].Exp / n)});
    }
    if (!u.Scale.is_one())
        beman::inside::detail::constexpr_error<
            "units: the root of a scaled unit - convert it to a named unit first">();
    return r;
}

// The root of a reference's spec: its recipe's, when the exponents divide.
consteval reference spec_root(const reference& a, int n, const unit& u) {
    if (is_generic(a))
        return reference{nullptr, false, {}, u};
    const recipe f = a.Spec ? recipe_of(a.KindOf ? kind_root(a.Spec.P) : a.Spec.P) : a.Derived;
    recipe       r{};
    for (std::int8_t i = 0; i < f.Size; ++i) {
        if (f.F[i].Exp % n != 0)
            return reference{nullptr, false, {}, u};
        recipe_add(r, f.F[i].Spec, f.F[i].Exp / n);
    }
    if (r.Size == 1 && r.F[0].Exp == 1)
        return reference{r.F[0].Spec, a.KindOf, {}, u};
    return reference{nullptr, a.KindOf, r, u};
}

template <reference R, int N>
consteval reference root_reference() {
    const unit u = unit_root(R.U, N);
    return spec_root(R, N, u);
}

// An angle in radians: a dimensionless quantity of magnitude one.
template <reference R>
inline constexpr bool radians_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).is_one();

inline constexpr reference radian_reference = reference{si::radian};

// An angle in a unit with a factor of π (°, a turn): sin and cos reduce it exactly.
template <reference R>
inline constexpr bool pi_angle_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).Pi == 1;

// sin or cos of a π-scaled angle: one rounding onto Out. The factor n/d in
// one limb when it is a 64-bit rational, else in as many as it needs.
template <insidable Out, bool Cos, reference R, class V>
constexpr Out pi_trig(const quantity<R, V>& q) {
    namespace engine = beman::inside::math::detail::engine;
    using beman::inside::detail::grid_int;
    beman::inside::math::detail::entry::require_rounding<Out>();
    constexpr auto        f  = magnitude_of(R.U).Ratio;
    constexpr grid_int    n  = beman::inside::detail::grid_numerator(f);
    constexpr grid_int    d  = beman::inside::detail::grid_denominator(f);
    constexpr bool        ng = n.negative();
    constexpr std::size_t KF = beman::inside::detail::fits_rational(f)
                                   ? 1
                                   : beman::inside::detail::limbs_for_bits(std::max(n.bit_width(), d.bit_width()) + 1);
    using core               = engine::pi_trig_core<engine::input_limbs<V>, Cos, KF>;
    using factor             = typename core::factor;
    return engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_input(units::detail::value_of(q)),
                                                               static_cast<factor>(ng ? -n : n),
                                                               static_cast<factor>(d),
                                                               ng});
}
} // namespace detail

// value_cast<U, Out>(q) across a pi factor (degree ↔ radian): the exact
// x·r·π^±1, rounded once onto Out's grid under its rounding mode.
template <auto U, insidable Out, reference R, class V>
    requires detail::is_unit_tag_v<decltype(U)> &&
             (!detail::exact_factor_v<detail::conversion_factor<R.U, decltype(U)::value>>)
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    namespace engine        = beman::inside::math::detail::engine;
    constexpr units::unit u = decltype(U)::value;
    constexpr magnitude   f = detail::conversion_factor<R.U, u>;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    static_assert(f.Pi == 1 || f.Pi == -1, "units: value_cast - only a factor of pi or 1/pi rounds");
    beman::inside::math::detail::entry::require_rounding<Out>();
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    // The factor n/d in one limb when it is a 64-bit rational, else in as
    // many as it needs (ħ = h/2π in J·s).
    using beman::inside::detail::grid_int;
    constexpr grid_int    n  = beman::inside::detail::grid_numerator(f.Ratio);
    constexpr grid_int    d  = beman::inside::detail::grid_denominator(f.Ratio);
    constexpr bool        ng = n.negative();
    constexpr int         NB = n.bit_width();
    constexpr std::size_t KF = beman::inside::detail::fits_rational(f.Ratio)
                                   ? 1
                                   : beman::inside::detail::limbs_for_bits(std::max(NB, d.bit_width()) + 1);
    using core               = engine::pi_scale_core<engine::input_limbs<V>, engine::in_mag<V> + NB + 4, f.Pi, KF>;
    using factor             = typename core::factor;
    return detail::quantity_access::make<quantity<to, Out>>(
        engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_input(units::detail::value_of(q)),
                                                            static_cast<factor>(ng ? -n : n),
                                                            static_cast<factor>(d),
                                                            ng}));
}

namespace detail {
// x's number as an inside: an inside itself, or a dimensionless quantity's.
template <class X>
constexpr auto number_of(const X& x) {
    if constexpr (insidable<X>)
        return x;
    else {
        static_assert(quantity_like<X> && number_like_v<X::reference>,
                      "units: the argument must be a number - an inside or a dimensionless quantity");
        return rescale<conversion_factor<X::reference.U, unit{}>>(units::detail::value_of(x));
    }
}
template <class X>
using number_of_t = decltype(number_of(std::declval<const X&>()));

//---------------------------------------------------------------------------
// The math functions on quantities (math/detail/functions.hpp): each takes
// its arguments' numbers to the inside function, by one of these rules, and
// puts the result in its unit.
//---------------------------------------------------------------------------
namespace m = beman::inside::math::detail;
enum class math_rule {
    none,
    same,   // abs, floor, ceil, round, trunc: in q's unit
    root2,  // sqrt: the unit's root (m² → m)
    root3,  // cbrt
    trig,   // sin, cos: of an angle in radians or with a factor of π; a number
    tan,    // tan: of an angle in radians; a number
    arc,    // asin, acos, atan: of a number; an angle in radians
    arc2,   // atan2: of two quantities that add; an angle in radians
    common, // hypot: of two quantities that add; in their common unit
    number, // exp … atanh, pow_base, pow: of numbers; a number
};
template <class F>
inline constexpr math_rule rule_of = math_rule::none;
template <>
inline constexpr math_rule rule_of<m::abs_fn> = math_rule::same;
template <>
inline constexpr math_rule rule_of<m::floor_fn> = math_rule::same;
template <>
inline constexpr math_rule rule_of<m::ceil_fn> = math_rule::same;
template <>
inline constexpr math_rule rule_of<m::round_fn> = math_rule::same;
template <>
inline constexpr math_rule rule_of<m::trunc_fn> = math_rule::same;
template <>
inline constexpr math_rule rule_of<m::entry::sqrt_fn> = math_rule::root2;
template <>
inline constexpr math_rule rule_of<m::entry::cbrt_fn> = math_rule::root3;
template <>
inline constexpr math_rule rule_of<m::entry::sin_fn> = math_rule::trig;
template <>
inline constexpr math_rule rule_of<m::entry::cos_fn> = math_rule::trig;
template <>
inline constexpr math_rule rule_of<m::entry::tan_fn> = math_rule::tan;
template <>
inline constexpr math_rule rule_of<m::entry::asin_fn> = math_rule::arc;
template <>
inline constexpr math_rule rule_of<m::entry::acos_fn> = math_rule::arc;
template <>
inline constexpr math_rule rule_of<m::entry::atan_fn> = math_rule::arc;
template <>
inline constexpr math_rule rule_of<m::entry::atan2_fn> = math_rule::arc2;
template <>
inline constexpr math_rule rule_of<m::entry::hypot_fn> = math_rule::common;
#define BEMAN_INSIDE_UNITS_NUMBER_RULE(fn) \
    template <>                            \
    inline constexpr math_rule rule_of<m::entry::fn##_fn> = math_rule::number;
BEMAN_INSIDE_UNITS_NUMBER_RULE(exp)
BEMAN_INSIDE_UNITS_NUMBER_RULE(exp2)
BEMAN_INSIDE_UNITS_NUMBER_RULE(log)
BEMAN_INSIDE_UNITS_NUMBER_RULE(log2)
BEMAN_INSIDE_UNITS_NUMBER_RULE(log10)
BEMAN_INSIDE_UNITS_NUMBER_RULE(sinh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(cosh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(tanh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(asinh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(acosh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(atanh)
BEMAN_INSIDE_UNITS_NUMBER_RULE(pow)
#undef BEMAN_INSIDE_UNITS_NUMBER_RULE
template <auto B>
inline constexpr math_rule rule_of<m::entry::pow_base_fn<B>> = math_rule::number;

// The output for specs S of a result in reference To (a quantity), or a
// plain number (Number), whose number is the output of the numbers' kind NK.
template <class O>
struct rep_of {
    using type = typename O::rep;
};
template <class NK, bool Number, reference To>
struct output_in {
    template <class N>
    using with = std::conditional_t<Number, N, quantity<To, N>>;
    template <class S>
    struct out {
        using type = with<typename NK::template output<S>>;
    };
    template <class C>
    struct out<m::named<C>> {
        static consteval auto pick() {
            if constexpr (insidable<C>)
                return std::type_identity<with<C>>{};
            else {
                static_assert(!Number, "beman::inside::math: f<C>(q) - the result is a number: C is an inside");
                static_assert(quantity_like<C>, "beman::inside::math: f<C>(q) - C is a quantity or its number");
                static_assert(C::reference.U == To.U && implicit_v<To, C::reference>,
                              "beman::inside::math: f<C>(q) - C's unit must be exactly the result's (or name only "
                              "the number)");
                return std::type_identity<C>{};
            }
        }
        using type = typename decltype(pick())::type;
    };

    // The output's number: O itself, or its rep.
    template <class O>
    using number_t = typename std::conditional_t<Number, std::type_identity<O>, rep_of<O>>::type;
    template <class O, class V>
    static constexpr auto make(V&& v) {
        if constexpr (Number)
            return std::forward<V>(v);
        else
            return lift_to<O>(std::forward<V>(v));
    }
};

// Each rule: whether it takes the arguments (takes), the kind of their
// numbers (NK), whether the result is a plain number or else its reference
// (number, to), and the specs the numbers' kind sees (number_spec).
template <math_rule Rule, class F, class... Args>
struct quantity_rule {
    static constexpr bool takes = false;
};
struct rule_base {
    template <class S>
    using number_spec = S;
};

template <math_rule Rule, class F, reference R, class V>
    requires(Rule == math_rule::same || Rule == math_rule::root2 || Rule == math_rule::root3)
struct quantity_rule<Rule, F, quantity<R, V>> : rule_base {
    using NK                          = m::kind<F, 1, V>;
    static constexpr bool      takes  = NK::takes;
    static constexpr bool      number = false;
    static constexpr reference to     = Rule == math_rule::same    ? R
                                        : Rule == math_rule::root2 ? root_reference<R, 2>()
                                                                   : root_reference<R, 3>();
};

// An angle in radians: the number's function; with a factor of π (sin and
// cos): reduced exactly by quarter turns, the result on the named notch.
template <math_rule Rule, class F, reference R, class V>
    requires(Rule == math_rule::trig || Rule == math_rule::tan)
struct quantity_rule<Rule, F, quantity<R, V>> {
    static_assert(radians_v<R> || (Rule == math_rule::trig && pi_angle_v<R>),
                  "units: sin, cos and tan take an angle in radians, sin and cos one in a unit with a factor of pi "
                  "too - value_cast<si::radian, Out>(q) first");
    using NK                          = m::kind<F, 1, V>;
    static constexpr bool      takes  = NK::takes;
    static constexpr bool      number = true;
    static constexpr reference to     = {};
    template <class S>
    struct spec {
        using type = S;
    };
    template <m::out_spec S>
    struct spec<m::specs<S>> {
        static_assert(radians_v<R> || S.HasNotch,
                      "units: sin and cos of an angle with a factor of pi (°, a turn) - name the result's notch: "
                      "sin<per<K>>(q)");
        using type = m::specs<S>;
    };
    template <class S>
    using number_spec = typename spec<S>::type;
};

// A number: an angle in radians.
template <class F, class X>
struct quantity_rule<math_rule::arc, F, X> : rule_base {
    using NK                          = m::kind<F, 1, number_of_t<X>>;
    static constexpr bool      takes  = NK::takes;
    static constexpr bool      number = false;
    static constexpr reference to     = radian_reference;
};

// Two quantities that add, in their common unit: an angle in radians
// (atan2), or in that unit (hypot). (Two that do not add take nothing, and
// form no common unit: the deleted overloads below say why.)
template <class F, reference A, class VA, reference B, class VB, bool = common_rescalable<A, VA, B, VB>>
struct common_rule {
    using NK                      = m::kind<F, 0>;
    static constexpr reference to = {};
};
template <class F, reference A, class VA, reference B, class VB>
struct common_rule<F, A, VA, B, VB, true> {
    using NK                      = m::kind<F, 1, common_number_t<A, VA, B>, common_number_t<B, VB, A>>;
    static constexpr reference to = sum_reference<A, B>();
};
template <math_rule Rule, class F, reference A, class VA, reference B, class VB>
    requires(Rule == math_rule::arc2 || Rule == math_rule::common)
struct quantity_rule<Rule, F, quantity<A, VA>, quantity<B, VB>> : rule_base {
    using C                           = common_rule<F, A, VA, B, VB>;
    using NK                          = typename C::NK;
    static constexpr bool      takes  = NK::takes;
    static constexpr bool      number = false;
    static constexpr reference to     = Rule == math_rule::arc2 ? radian_reference : C::to;
};

// Numbers (insides or dimensionless quantities): a number.
template <class F, class... X>
struct quantity_rule<math_rule::number, F, X...> : rule_base {
    using NK                          = m::kind<F, 1, number_of_t<X>...>;
    static constexpr bool      takes  = NK::takes;
    static constexpr bool      number = true;
    static constexpr reference to     = {};
};
} // namespace detail
} // namespace beman::inside::units

namespace beman::inside::math::detail {
// A quantity's rank: 2, or 3 when its unit holds a blade (a part of a
// multivector, which exp takes: units/detail/geoalg_exp.hpp).
template <class T>
    requires units::quantity_like<T>
inline constexpr int kind_rank<T> = units::detail::has_blade(dimension_of(T::reference.U)) ? 3 : 2;

// Quantities: the rule of F.
template <class F, class... Args>
struct kind<F, 2, Args...> {
    using rule                  = units::detail::quantity_rule<units::detail::rule_of<F>, F, Args...>;
    static constexpr bool takes = rule::takes;
    using io                    = units::detail::output_in<typename rule::NK, rule::number, rule::to>;

    template <class S>
    using output = typename io::template out<typename rule::template number_spec<S>>::type;

    template <class O>
    [[gnu::always_inline]] static constexpr auto eval(const Args&... x) {
        using NO         = typename io::template number_t<O>;
        constexpr auto r = units::detail::rule_of<F>;
        if constexpr (r == units::detail::math_rule::trig || r == units::detail::math_rule::tan) {
            constexpr units::reference R = Args...[0] ::reference;
            if constexpr (units::detail::radians_v<R>)
                return F::template eval<NO>(units::detail::value_of(x...[0]));
            else
                return units::detail::pi_trig<NO, std::same_as<F, entry::cos_fn>>(x...[0]);
        } else if constexpr (r == units::detail::math_rule::arc2 || r == units::detail::math_rule::common) {
            constexpr units::reference A = Args...[0] ::reference, B = Args...[1] ::reference;
            return io::template make<O>(F::template eval<NO>(units::detail::in_common_unit<A, B>(x...[0]),
                                                             units::detail::in_common_unit<A, B>(x...[1])));
        } else if constexpr (r == units::detail::math_rule::arc || r == units::detail::math_rule::number)
            return io::template make<O>(F::template eval<NO>(units::detail::number_of(x)...));
        else
            return io::template make<O>(F::template eval<NO>(units::detail::value_of(x)...));
    }

    // A named output f<C>(q): the numbers' kind decides it (C itself, or an
    // expected where the result can miss C), lifted to the quantity.
    template <class O>
    [[gnu::always_inline]] static constexpr auto eval_named(const Args&... x) {
        using NO             = typename io::template number_t<O>;
        constexpr auto r     = units::detail::rule_of<F>;
        const auto     named = [](const auto&... n) {
            return detail::eval_named<F, NO, std::remove_cvref_t<decltype(n)>...>(n...);
        };
        if constexpr (r == units::detail::math_rule::trig || r == units::detail::math_rule::tan) {
            constexpr units::reference R = Args...[0] ::reference;
            if constexpr (units::detail::radians_v<R>)
                return named(units::detail::value_of(x...[0]));
            else // (a degree input: the pi-scaled engine, into NO as named)
                return units::detail::pi_trig<NO, std::same_as<F, entry::cos_fn>>(x...[0]);
        } else if constexpr (r == units::detail::math_rule::arc2 || r == units::detail::math_rule::common) {
            constexpr units::reference A = Args...[0] ::reference, B = Args...[1] ::reference;
            return io::template make<O>(
                named(units::detail::in_common_unit<A, B>(x...[0]), units::detail::in_common_unit<A, B>(x...[1])));
        } else if constexpr (r == units::detail::math_rule::arc || r == units::detail::math_rule::number)
            return io::template make<O>(named(units::detail::number_of(x)...));
        else
            return io::template make<O>(named(units::detail::value_of(x)...));
    }
};
} // namespace beman::inside::math::detail

namespace beman::inside::math {
// atan2 and hypot of two quantities that do not add (another dimension or
// kind, no common unit).
template <auto... Spec, units::reference A, class VA, units::reference B, class VB>
    requires(units::detail::sum_diagnosed<A, B>())
void atan2(const units::quantity<A, VA>&, const units::quantity<B, VB>&) =
    delete ("units: atan2 takes two quantities that add - one dimension and kind, a common unit");
template <auto... Spec, units::reference A, class VA, units::reference B, class VB>
    requires(units::detail::sum_diagnosed<A, B>())
void hypot(const units::quantity<A, VA>&, const units::quantity<B, VB>&) =
    delete ("units: hypot takes two quantities that add - one dimension and kind, a common unit");
} // namespace beman::inside::math

// exp of multivectors (units/geoalg.hpp).
#include <beman/inside/units/detail/geoalg_exp.hpp>

#endif // BEMAN_INSIDE_UNITS_MATH_HPP
