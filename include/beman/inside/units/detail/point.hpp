// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// quantity_point<R, O, Rep>: points counted from an origin, their exact
// shifts and comparisons. Internal — include <beman/inside/units.hpp> or
// <beman/inside/units/core.hpp>.
#ifndef BEMAN_INSIDE_UNITS_DETAIL_POINT_HPP
#define BEMAN_INSIDE_UNITS_DETAIL_POINT_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/detail/quantity.hpp>

namespace beman::inside::units {

//---------------------------------------------------------------------------
// quantity_point<R, O, Rep> — a point: a quantity of R counted from origin O
// (null: the implicit zero of the dimension). A change of origin shifts the
// grid by the exact offset, a change of unit rescales it.
//   auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});   // 20 ℃
//   t.quantity_from(si::absolute_zero)                         // 293.15 K… in ℃
//   t.point_for(si::absolute_zero).in(si::kelvin)              // the point 293.15 K
//---------------------------------------------------------------------------
template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point;

template <class T>
inline constexpr bool is_quantity_point_v = false;
template <reference R, detail::origin_ref O, class Rep>
inline constexpr bool is_quantity_point_v<quantity_point<R, O, Rep>> = true;

namespace detail {
// The number, in unit U, that takes a quantity from origin From to origin To:
// (offset(From) − offset(To)) / mag(U).
template <origin_ref From, origin_ref To, unit U>
consteval grid_rational origin_shift() {
    static_assert(same(absolute_of(From), absolute_of(To)), "units: points of different absolute origins");
    constexpr magnitude m = magnitude_of(U);
    static_assert(m.Pi == 0, "units: an origin shift in a unit with a pi magnitude has no exact grid");
    return (offset_of(From) - offset_of(To)) / m.Ratio;
}

// Whether shift<C> forms on V: inside's v + C exists.
template <grid_rational C, class V>
concept shift_forms = C == grid_rational{0} || requires(const V& v) { v + just<C>; };

template <grid_rational C, insidable V>
constexpr auto shift(const V& v) {
    if constexpr (C == grid_rational{0})
        return v;
    else
        return v + just<C>;
}

// The default origin of a unit's points: its named unit's (K, ℃, ℉).
consteval origin_ref origin_of_unit(const unit& u) { return is_single_named(u) ? u.Atoms[0].U->Origin : origin_ref{}; }

template <origin_ref O, class Q>
constexpr auto make_point(Q&& q) {
    using T = beman::inside::detail::unwrap_t<Q>;
    return lift_to<quantity_point<T::reference, O, typename T::rep>>(std::forward<Q>(q));
}
} // namespace detail

template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point {
    quantity<R, Rep> Q;

  public:
    using quantity_type                           = quantity<R, Rep>;
    using rep                                     = Rep;
    static constexpr units::reference   reference = R;
    static constexpr detail::origin_ref origin    = O;

    constexpr quantity_point() = default;
    constexpr explicit quantity_point(const quantity<R, Rep>& q) : Q{q} {}

    // From a point of another origin (of the same absolute one) or unit:
    // shifted, then rescaled, exactly; implicit as the quantities are.
    template <units::reference R2, detail::origin_ref O2, class V2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O))) && detail::implicit_v<R2, R> &&
                std::constructible_from<
                    quantity<R, Rep>,
                    decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{}))>
    constexpr explicit(
        !std::convertible_to<decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{})),
                             quantity<R, Rep>>) quantity_point(const quantity_point<R2, O2, V2>& p)
        : Q(p.quantity_from(origin_t<O>{})) {}

    // The quantity from this point's origin, or from another one.
    [[nodiscard]] constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept { return Q; }
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi == 0 &&
                 detail::shift_forms<detail::origin_shift<O, O2, R.U>(), Rep>)
    [[nodiscard]] constexpr auto quantity_from(origin_t<O2>) const {
        constexpr grid_rational c = detail::origin_shift<O, O2, R.U>();
        return quantity<R, std::remove_cvref_t<decltype(detail::shift<c>(Q.numerical_value()))>>{
            detail::shift<c>(Q.numerical_value())};
    }
    template <detail::origin_ref O2>
        requires(!detail::same(detail::absolute_of(O2), detail::absolute_of(O)))
    void quantity_from(origin_t<O2>) const = delete ("units: points of different absolute origins");
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi != 0)
    void quantity_from(origin_t<O2>) const =
        delete ("units: an origin shift in a unit with a pi magnitude has no exact grid");
    // The quantity from the absolute origin of the dimension (absolute zero,
    // or the implicit zero): 20 ℃ → 293.15 K counted in ℃.
    [[nodiscard]] constexpr auto quantity_from_zero() const
        requires requires(const quantity_point& p) { p.quantity_from(origin_t<detail::absolute_of(O)>{}); }
    {
        return quantity_from(origin_t<detail::absolute_of(O)>{});
    }
    // The same point counted from another origin.
    template <detail::origin_ref O2>
        requires requires(const quantity_point& p, origin_t<O2> o) { p.quantity_from(o); }
    [[nodiscard]] constexpr auto point_for(origin_t<O2> o) const {
        return detail::make_point<O2>(quantity_from(o));
    }
    // The same point in another unit.
    template <units::unit U>
        requires requires(const quantity<R, Rep>& q, unit_t<U> u) { q.in(u); }
    [[nodiscard]] constexpr auto in(unit_t<U> u) const {
        return detail::make_point<O>(Q.in(u));
    }
    // The same point in unit U, counted from U's own origin: 350 ℉ → 530/3 ℃.
    template <units::unit U>
        requires requires(const quantity_point& p, unit_t<U> u) {
            p.point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
        }
    [[nodiscard]] constexpr auto point_in(unit_t<U> u) const {
        return point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
    }

    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a += b; }
    constexpr quantity_point& operator+=(const quantity<R2, V2>& q) {
        Q += q;
        return *this;
    }
    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a -= b; }
    constexpr quantity_point& operator-=(const quantity<R2, V2>& q) {
        Q -= q;
        return *this;
    }
};

// value_cast on a point: its quantity from the origin cast, the origin kept.
template <insidable Rep2, reference R, detail::origin_ref O, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity_point<R, O, Rep2> value_cast(const quantity_point<R, O, V>& p) {
    return quantity_point<R, O, Rep2>{value_cast<Rep2>(p.quantity_from_origin())};
}
template <auto U, insidable Rep2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R, V>& q) { value_cast<U, Rep2>(q); }
[[nodiscard]] constexpr auto value_cast(const quantity_point<R, O, V>& p) {
    const auto q = value_cast<U, Rep2>(p.quantity_from_origin());
    return quantity_point<std::remove_cvref_t<decltype(q)>::reference, O, Rep2>{q};
}

// point<si::degree_Celsius>(v) — a point at the unit's default origin (the
// implicit zero for a unit without one).
template <auto U, insidable V>
[[nodiscard]] constexpr auto point(const V& v) {
    return quantity_point<reference_of(U), detail::origin_of_unit(decltype(U)::value), V>{
        quantity<reference_of(U), V>{v}};
}
// delta<si::degree_Celsius>(v) — a difference: a plain quantity.
template <auto U, insidable V>
[[nodiscard]] constexpr auto delta(const V& v) {
    return v * U;
}

// origin + quantity: si::absolute_zero + just<300> * si::kelvin.
template <detail::origin_ref O, reference R, class V>
[[nodiscard]] constexpr auto operator+(origin_t<O>, const quantity<R, V>& q) {
    return quantity_point<R, O, V>{q};
}
template <reference R, class V, detail::origin_ref O>
[[nodiscard]] constexpr auto operator+(const quantity<R, V>& q, origin_t<O>) {
    return quantity_point<R, O, V>{q};
}

// point ± quantity: a point at the same origin, in the point's unit when the
// quantity converts to it exactly (5 K onto a ℃ point stays ℃), else in the
// common unit; point − point: a quantity (the second counted from the first's
// origin).
namespace detail {
template <unit U, reference R2, class V2>
constexpr auto in_point_unit(const quantity<R2, V2>& q) {
    if constexpr (requires { q.template in<U>(); })
        return q.template in<U>();
    else
        return q;
}
} // namespace detail
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a + detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator+(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() + detail::in_point_unit<R.U>(q));
}
template <reference R2, class V2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R2, V2>& a, const quantity<R, V>& b) { detail::in_point_unit<R.U>(a) + b; }
[[nodiscard]] constexpr auto operator+(const quantity<R2, V2>& q, const quantity_point<R, O, V>& p) {
    return detail::make_point<O>(detail::in_point_unit<R.U>(q) + p.quantity_from_origin());
}
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a - detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() - detail::in_point_unit<R.U>(q));
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr bool operator==(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr auto operator<=>(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
}
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_DETAIL_POINT_HPP
