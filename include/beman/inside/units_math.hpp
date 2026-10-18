// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: math on units::quantity, over the correctly rounded engine
// (cmath.hpp). Kept apart from units.hpp, which stays free of the engine's
// compile time.
//
//   value_cast<si::radian, Out>(q)   degree → radian (a pi factor): the exact
//                                    x·π/180, rounded once onto Out's grid
//   sqrt(q), cbrt(q)                 the unit's root (m² → m), the number's
//   abs, floor, ceil, round, trunc   on the number, in q's unit
//   sin, cos, tan(q)                 of an angle in radians: a plain inside
//   asin, acos, atan(x), atan2(y, x) an angle in radians
//   hypot(x, y)                      in the common unit
//   fn_into<Out>(…)                  each of them with the result's number
//                                    named: Out rounds, the input need not
//
// Every result rounds as the inside function does: onto the output grid,
// under its rounding mode (the deduced forms take the input's grid).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_MATH_HPP
#define BEMAN_INSIDE_UNITS_MATH_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/cmath.hpp>
#include <beman/inside/units.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace beman::inside::math::detail::ax {
// x·(n/d)·π^P for P = ±1: π from pi_q at T = S + B bits, B covering |x·n/d|,
// so its error adds at most one unit; the division truncates one more. The
// factor is 64-bit (KF = 1), or KF limbs for a factor past 64 bits (C++26 big
// grids: ħ = h/2π in J·s).
template <std::size_t E, int Bits, int P, std::size_t KF = 1>
struct pi_scale_core {
    using factor = std::conditional_t<KF == 1, umax, beman::inside::detail::wide_uint<KF>>;
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
    using factor = std::conditional_t<KF == 1, umax, beman::inside::detail::wide_uint<KF>>;
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
        const unsigned q   = static_cast<unsigned>(k.Word[0] & 3u);
        auto           neg = [](fx<K> f) { return fx<K>{-f.Value, f.Error}; };
        const fx<K>    v   = Cos ? ((q == 0)   ? cos_series<S>(r, 2)
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
} // namespace beman::inside::math::detail::ax

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
inline constexpr bool pi_angle_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).Pi == 1 &&
                                   beman::inside::detail::fits_rational(magnitude_of(R.U).Ratio);

// sin_into / cos_into of a π-scaled angle: one rounding onto Out.
template <insidable Out, bool Cos, reference R, class V>
constexpr Out pi_trig_into(const quantity<R, V>& q) {
    namespace ax = beman::inside::math::detail::ax;
    beman::inside::math::adaptive::require_rounding<Out>();
    constexpr rational r = beman::inside::detail::to_rational(magnitude_of(R.U).Ratio);
    using core           = ax::pi_trig_core<ax::input_limbs<V>, Cos>;
    return ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(q.numerical_value()),
                                                       static_cast<umax>(r.Numerator < 0 ? -r.Numerator : r.Numerator),
                                                       beman::inside::detail::abs_den(r.Denominator),
                                                       (r.Numerator < 0) != (r.Denominator < 0)});
}
} // namespace detail

// value_cast<U, Out>(q) across a pi factor (degree ↔ radian): the exact
// x·r·π^±1, rounded once onto Out's grid under its rounding mode.
template <auto U, insidable Out, reference R, class V>
    requires is_unit_tag_v<decltype(U)> &&
             (!detail::exact_factor_v<detail::conversion_factor<R.U, decltype(U)::value>>)
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    namespace ax            = beman::inside::math::detail::ax;
    constexpr units::unit u = decltype(U)::value;
    constexpr magnitude   f = detail::conversion_factor<R.U, u>;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    static_assert(f.Pi == 1 || f.Pi == -1, "units: value_cast - only a factor of pi or 1/pi rounds");
    beman::inside::math::adaptive::require_rounding<Out>();
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    const auto          x = ax::exact_input(q.numerical_value());
    if constexpr (beman::inside::detail::fits_rational(f.Ratio)) {
        constexpr rational r = beman::inside::detail::to_rational(f.Ratio);
        constexpr int      B = ax::in_mag<V> + static_cast<int>(std::bit_width(r.Numerator)) + 4;
        using core           = ax::pi_scale_core<ax::input_limbs<V>, B, f.Pi>;
        return quantity<to, Out>{ax::evaluate<Out, ax::start_bits<Out>>(
            core{x, r.Numerator, beman::inside::detail::abs_den(r.Denominator), r.Denominator < 0})};
    }
#if BEMAN_INSIDE_BIG_GRIDS
    else {
        // A factor past 64 bits (C++26 big grids): the same core on wide integers.
        using beman::inside::detail::grid_int;
        constexpr grid_int    n  = beman::inside::detail::wide_numerator(f.Ratio);
        constexpr grid_int    d  = beman::inside::detail::wide_denominator(f.Ratio);
        constexpr bool        ng = n.negative();
        constexpr int         NB = n.bit_width();
        constexpr std::size_t KF = beman::inside::detail::limbs_for_bits(std::max(NB, d.bit_width()) + 1);
        using WF                 = beman::inside::detail::wide_uint<KF>;
        constexpr WF  wn         = static_cast<WF>(ng ? -n : n);
        constexpr WF  wd         = static_cast<WF>(d);
        constexpr int B          = ax::in_mag<V> + NB + 4;
        using core               = ax::pi_scale_core<ax::input_limbs<V>, B, f.Pi, KF>;
        return quantity<to, Out>{ax::evaluate<Out, ax::start_bits<Out>>(core{x, wn, wd, ng})};
    }
#endif
}

// The number's functions, in q's unit.
template <reference R, class V>
[[nodiscard]] constexpr auto abs(const quantity<R, V>& q) {
    return detail::make_quantity<R>(beman::inside::math::abs(q.numerical_value()));
}
template <reference R, class V>
[[nodiscard]] constexpr auto floor(const quantity<R, V>& q) {
    return detail::make_quantity<R>(beman::inside::math::floor(q.numerical_value()));
}
template <reference R, class V>
[[nodiscard]] constexpr auto ceil(const quantity<R, V>& q) {
    return detail::make_quantity<R>(beman::inside::math::ceil(q.numerical_value()));
}
template <reference R, class V>
[[nodiscard]] constexpr auto round(const quantity<R, V>& q) {
    return detail::make_quantity<R>(beman::inside::math::round(q.numerical_value()));
}
template <reference R, class V>
[[nodiscard]] constexpr auto trunc(const quantity<R, V>& q) {
    return detail::make_quantity<R>(beman::inside::math::trunc(q.numerical_value()));
}

// Roots: the unit's (m² → m, m³ → m) and the number's.
template <reference R, class V>
[[nodiscard]] constexpr auto sqrt(const quantity<R, V>& q) {
    return detail::make_quantity<detail::root_reference<R, 2>()>(beman::inside::math::sqrt(q.numerical_value()));
}
template <reference R, class V>
[[nodiscard]] constexpr auto cbrt(const quantity<R, V>& q) {
    return detail::make_quantity<detail::root_reference<R, 3>()>(beman::inside::math::cbrt(q.numerical_value()));
}

// Trigonometry of an angle in radians: a plain number.
template <reference R, class V>
[[nodiscard]] constexpr auto sin(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>,
                  "units: sin takes an angle in radians - sin_into<Out>(q) takes a unit with a factor of pi");
    return beman::inside::math::sin(q.numerical_value());
}
template <reference R, class V>
[[nodiscard]] constexpr auto cos(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>,
                  "units: cos takes an angle in radians - cos_into<Out>(q) takes a unit with a factor of pi");
    return beman::inside::math::cos(q.numerical_value());
}
template <reference R, class V>
[[nodiscard]] constexpr auto tan(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: tan takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::tan(q.numerical_value());
}

// Inverse trigonometry: an angle in radians.
template <insidable V>
[[nodiscard]] constexpr auto asin(const V& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::asin(x));
}
template <insidable V>
[[nodiscard]] constexpr auto acos(const V& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::acos(x));
}
template <insidable V>
[[nodiscard]] constexpr auto atan(const V& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::atan(x));
}
// atan2(y, x) of two quantities of one dimension, in their common unit.
template <reference A, class VA, reference B, class VB>
    requires(!detail::sum_diagnosed<A, B>()) &&
            (detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::atan2(x, y);
             })
[[nodiscard]] constexpr auto atan2(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<detail::radian_reference>(
        beman::inside::math::atan2(detail::rescale<detail::conversion_factor<A.U, C.U>>(y.numerical_value()),
                                   detail::rescale<detail::conversion_factor<B.U, C.U>>(x.numerical_value())));
}

// hypot(x, y): √(x² + y²) in the common unit.
template <reference A, class VA, reference B, class VB>
    requires(!detail::sum_diagnosed<A, B>()) &&
            (detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::hypot(x, y);
             })
[[nodiscard]] constexpr auto hypot(const quantity<A, VA>& x, const quantity<B, VB>& y) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::math::hypot(detail::rescale<detail::conversion_factor<A.U, C.U>>(x.numerical_value()),
                                   detail::rescale<detail::conversion_factor<B.U, C.U>>(y.numerical_value())));
}

//---------------------------------------------------------------------------
// The _into forms: Out names the result's number (an inside with a rounding
// mode); the input needs no rounding mode — an arithmetic result (checked)
// goes straight in. As math::fn_into, a result may be an expected (sqrt of a
// grid that reaches below 0, tan).
//---------------------------------------------------------------------------
namespace detail {
// x's number as an inside: an inside itself, or a dimensionless quantity's.
template <class X>
constexpr auto number_of(const X& x) {
    if constexpr (insidable<X>)
        return x;
    else {
        static_assert(quantity_like<X> && number_like_v<X::reference>,
                      "units: the argument must be a number - an inside or a dimensionless quantity");
        return rescale<conversion_factor<X::reference.U, unit{}>>(x.numerical_value());
    }
}
} // namespace detail

template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto sqrt_into(const quantity<R, V>& q) {
    return detail::make_quantity<detail::root_reference<R, 2>()>(
        beman::inside::math::sqrt_into<Out>(q.numerical_value()));
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto cbrt_into(const quantity<R, V>& q) {
    return detail::make_quantity<detail::root_reference<R, 3>()>(
        beman::inside::math::cbrt_into<Out>(q.numerical_value()));
}

// An angle in radians, or in a unit with a factor of π (°, a turn, 2π/65536
// rad): that one is reduced exactly, and the result rounds once.
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto sin_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R> || detail::pi_angle_v<R>,
                  "units: sin_into takes an angle in radians or in a unit with a factor of pi");
    if constexpr (detail::radians_v<R>)
        return beman::inside::math::sin_into<Out>(q.numerical_value());
    else
        return detail::pi_trig_into<Out, false>(q);
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto cos_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R> || detail::pi_angle_v<R>,
                  "units: cos_into takes an angle in radians or in a unit with a factor of pi");
    if constexpr (detail::radians_v<R>)
        return beman::inside::math::cos_into<Out>(q.numerical_value());
    else
        return detail::pi_trig_into<Out, true>(q);
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto tan_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: tan takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::tan_into<Out>(q.numerical_value());
}

template <insidable Out, class X>
[[nodiscard]] constexpr auto asin_into(const X& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::asin_into<Out>(detail::number_of(x)));
}
template <insidable Out, class X>
[[nodiscard]] constexpr auto acos_into(const X& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::acos_into<Out>(detail::number_of(x)));
}
template <insidable Out, class X>
[[nodiscard]] constexpr auto atan_into(const X& x) {
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::atan_into<Out>(detail::number_of(x)));
}

// Exponentials, logarithms and the hyperbolic functions of a number: an
// inside or a dimensionless quantity (10^(L / 20 dB), log10(P / 1 mW)).
#define BEMAN_INSIDE_UNITS_NUMBER_FN(fn)                                  \
    template <insidable Out, class X>                                     \
    [[nodiscard]] constexpr auto fn##_into(const X& x) {                  \
        return beman::inside::math::fn##_into<Out>(detail::number_of(x)); \
    }
BEMAN_INSIDE_UNITS_NUMBER_FN(exp)
BEMAN_INSIDE_UNITS_NUMBER_FN(exp2)
BEMAN_INSIDE_UNITS_NUMBER_FN(log)
BEMAN_INSIDE_UNITS_NUMBER_FN(log2)
BEMAN_INSIDE_UNITS_NUMBER_FN(log10)
BEMAN_INSIDE_UNITS_NUMBER_FN(sinh)
BEMAN_INSIDE_UNITS_NUMBER_FN(cosh)
BEMAN_INSIDE_UNITS_NUMBER_FN(tanh)
BEMAN_INSIDE_UNITS_NUMBER_FN(asinh)
BEMAN_INSIDE_UNITS_NUMBER_FN(acosh)
BEMAN_INSIDE_UNITS_NUMBER_FN(atanh)
#undef BEMAN_INSIDE_UNITS_NUMBER_FN
template <insidable Out, imax Base, class X>
[[nodiscard]] constexpr auto pow_base_into(const X& x) {
    return beman::inside::math::pow_base_into<Out, Base>(detail::number_of(x));
}
template <insidable Out, class B, class E>
[[nodiscard]] constexpr auto pow_into(const B& base, const E& exp) {
    return beman::inside::math::pow_into<Out>(detail::number_of(base), detail::number_of(exp));
}
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(!detail::sum_diagnosed<A, B>()) &&
            (detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::atan2_into<Out>(x, y);
             })
[[nodiscard]] constexpr auto atan2_into(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::atan2_into<Out>(
        detail::rescale<detail::conversion_factor<A.U, C.U>>(y.numerical_value()),
        detail::rescale<detail::conversion_factor<B.U, C.U>>(x.numerical_value())));
}
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(!detail::sum_diagnosed<A, B>()) &&
            (detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::hypot_into<Out>(x, y);
             })
[[nodiscard]] constexpr auto hypot_into(const quantity<A, VA>& x, const quantity<B, VB>& y) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(beman::inside::math::hypot_into<Out>(
        detail::rescale<detail::conversion_factor<A.U, C.U>>(x.numerical_value()),
        detail::rescale<detail::conversion_factor<B.U, C.U>>(y.numerical_value())));
}
// atan2 and hypot of two quantities that do not add (another dimension or
// kind, no common unit).
BEMAN_INSIDE_DELETE_BEGIN
template <reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void atan2(const quantity<A, VA>&, const quantity<B, VB>&) =
    BEMAN_INSIDE_DELETE("units: atan2 takes two quantities that add - one dimension and kind, a common unit");
template <reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void hypot(const quantity<A, VA>&, const quantity<B, VB>&) =
    BEMAN_INSIDE_DELETE("units: hypot takes two quantities that add - one dimension and kind, a common unit");
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void atan2_into(const quantity<A, VA>&, const quantity<B, VB>&) =
    BEMAN_INSIDE_DELETE("units: atan2 takes two quantities that add - one dimension and kind, a common unit");
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void hypot_into(const quantity<A, VA>&, const quantity<B, VB>&) =
    BEMAN_INSIDE_DELETE("units: hypot takes two quantities that add - one dimension and kind, a common unit");
BEMAN_INSIDE_DELETE_END
} // namespace beman::inside::units

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_UNITS_MATH_HPP
