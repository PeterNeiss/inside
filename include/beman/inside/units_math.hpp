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

#include <beman/inside/cmath.hpp>
#include <beman/inside/units.hpp>

#include <cstddef>
#include <type_traits>

namespace beman::inside::math::detail::ax {
// x·(n/d)·π^P for P = ±1: π from pi_q at T = S + B bits, B covering |x·n/d|,
// so its error adds at most one unit; the division truncates one more.
template <std::size_t E, int Bits, int P>
struct pi_scale_core {
    wide_rational<E> X;
    umax             N; // the rational factor n/d
    umax             D;
    bool             Negative;

    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }

    template <int W>
    constexpr auto run() const {
        constexpr int         S  = W + 2;
        constexpr int         T  = S + Bits + 4;
        constexpr std::size_t KJ = limbs_for_bits(64 * static_cast<int>(E) + 64 + T + 2 * S + 16);
        using J                  = wide_sint<KJ>;
        using I                  = fixed_t<Bits + S + 4>;
        const J pi{static_cast<J>(pi_q<T>)};
        const J num = J{X.Num} * J{N};
        const J den = J{X.Den} * J{D};
        J       y;
        if constexpr (P > 0)
            y = (num * pi) / (den << (T - S));
        else
            y = (num << (S + T)) / (den * pi);
        const I v = static_cast<I>(y);
        return approx<limbs_of<I>>{Negative ? -v : v, S, 2};
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
    static_assert(beman::inside::detail::fits_rational(f.Ratio), "units: value_cast - the factor needs 64 bits");
    beman::inside::math::adaptive::require_rounding<Out>();
    constexpr rational  r = beman::inside::detail::to_rational(f.Ratio);
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    const auto          x = ax::exact_input(q.numerical_value());
    constexpr int       B = ax::in_mag<V> + static_cast<int>(std::bit_width(r.Numerator)) + 4;
    using core            = ax::pi_scale_core<ax::input_limbs<V>, B, f.Pi>;
    return quantity<to, Out>{ax::evaluate<Out, ax::start_bits<Out>>(
        core{x, r.Numerator, beman::inside::detail::abs_den(r.Denominator), r.Denominator < 0})};
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
    static_assert(detail::radians_v<R>, "units: sin takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::sin(q.numerical_value());
}
template <reference R, class V>
[[nodiscard]] constexpr auto cos(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: cos takes an angle in radians - value_cast<si::radian, Out>(q) first");
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
[[nodiscard]] constexpr auto atan2(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<detail::radian_reference>(
        beman::inside::math::atan2(detail::rescale<detail::conversion_factor<A.U, C.U>>(y.numerical_value()),
                                   detail::rescale<detail::conversion_factor<B.U, C.U>>(x.numerical_value())));
}

// hypot(x, y): √(x² + y²) in the common unit.
template <reference A, class VA, reference B, class VB>
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

template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto sin_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: sin takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::sin_into<Out>(q.numerical_value());
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto cos_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: cos takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::cos_into<Out>(q.numerical_value());
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
template <insidable Out, reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto atan2_into(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<detail::radian_reference>(beman::inside::math::atan2_into<Out>(
        detail::rescale<detail::conversion_factor<A.U, C.U>>(y.numerical_value()),
        detail::rescale<detail::conversion_factor<B.U, C.U>>(x.numerical_value())));
}
template <insidable Out, reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto hypot_into(const quantity<A, VA>& x, const quantity<B, VB>& y) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(beman::inside::math::hypot_into<Out>(
        detail::rescale<detail::conversion_factor<A.U, C.U>>(x.numerical_value()),
        detail::rescale<detail::conversion_factor<B.U, C.U>>(y.numerical_value())));
}
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_MATH_HPP
