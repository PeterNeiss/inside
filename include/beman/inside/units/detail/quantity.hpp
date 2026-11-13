// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// quantity<R, Rep>: construction, arithmetic, casts and inside's algorithms
// on quantities. Internal — include <beman/inside/units.hpp> or
// <beman/inside/units/core.hpp>.
#ifndef BEMAN_INSIDE_UNITS_DETAIL_QUANTITY_HPP
#define BEMAN_INSIDE_UNITS_DETAIL_QUANTITY_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/detail/algebra.hpp>

namespace beman::inside::units {

//---------------------------------------------------------------------------
// quantity<R, Rep> — a number Rep (an inside) of the reference R.
//---------------------------------------------------------------------------
template <reference R, insidable Rep>
class quantity;

namespace detail {
// The library's own way into a quantity: one of type Q from a number in Q's
// unit, and the number back. The public API names the unit both ways
// (Q{v, u}, q.numerical_value_in(u)); the code here holds the unit in R.
struct quantity_access {
    template <class Q, class V>
    static constexpr Q make(V&& v) {
        return Q{typename Q::from_number{}, std::forward<V>(v)};
    }
    template <reference R, class V>
    static constexpr const V& number(const quantity<R, V>& q) noexcept {
        return q.Value;
    }
};
// q's number in its own unit R.
template <reference R, class V>
constexpr const V& value_of(const quantity<R, V>& q) noexcept {
    return quantity_access::number(q);
}
} // namespace detail

// A sum of quantities of different blades (units/geoalg.hpp). The empty one
// is the zero of any dimension: a product that vanishes, ε·ε with ε² = 0.
template <class... Parts>
class multivector;
template <>
class multivector<> {
  public:
    friend constexpr bool operator==(const multivector&, const multivector&) = default;
};

namespace detail {
template <class T>
inline constexpr bool is_quantity_v = false;
template <reference R, class Rep>
inline constexpr bool is_quantity_v<quantity<R, Rep>> = true;
} // namespace detail

template <class T>
concept quantity_like = detail::is_quantity_v<std::remove_cvref_t<T>>;

namespace detail {
// A multivector part T (an inside, or a quantity of a blade) from a number in
// its unit, with one-shot policies.
template <class T, class N, class... Pol>
constexpr T part_of_number(N&& n, const Pol&... pol) {
    if constexpr (!quantity_like<T>)
        return T(std::forward<N>(n), pol...);
    else if constexpr (sizeof...(Pol) == 0)
        return quantity_access::make<T>(std::forward<N>(n));
    else
        return T(std::forward<N>(n), reference_t<T::reference>{}, pol...);
}
} // namespace detail

namespace detail {
// An exact conversion factor from unit From to unit To: it holds no π.
template <unit From, unit To>
inline constexpr bool exact_rescale_v = exact_factor_v<conversion_factor<From, To>>;

template <reference From, reference To>
inline constexpr bool implicit_v =
    exact_rescale_v<From.U, To.U> && dimension_of(From.U) == dimension_of(To.U) && spec_converts(From, To);
// A quantity of From on V that converts implicitly to To; one of R that
// converts to unit U (q.in<U>()).
template <reference From, reference To, class V>
concept implicit_rescalable = implicit_v<From, To> && rescalable<From.U, To.U, V>;
template <reference R, unit U, class V>
concept in_rescalable = in_error_of<R.U, U>() == in_error::none && rescalable<R.U, U, V>;

// A dimensionless quantity that is a plain number: no is_kind spec (an
// angle), and an exact factor to unit one.
template <reference R>
inline constexpr bool number_like_v = dimension_of(R.U) == dim_one && exact_factor_v<conversion_factor<R.U, unit{}>> &&
                                      (!R.Spec || !kind_root(R.Spec.P)->IsKind);

// Its number's type; void for any other quantity (never forming the
// rescale, whose pi check would fire for an angle in degrees).
template <reference R, class V, bool = number_like_v<R>>
struct number_type {
    using type = void;
};
template <reference R, class V>
struct number_type<R, V, true> {
    using type = decltype(rescale<conversion_factor<R.U, unit{}>>(std::declval<const V&>()));
};
template <reference R, class V>
using number_t = typename number_type<R, V>::type;

template <reference From, reference To, insidable V>
using rescaled_t = decltype(rescale<conversion_factor<From.U, To.U>>(std::declval<const V&>()));

// q's number in unit U: exact, its grid rescaled.
template <unit U, reference R, class V>
constexpr auto number_in_unit(const quantity<R, V>& q) {
    return rescale<conversion_factor<R.U, U>>(value_of(q));
}

// A one-shot store's source (Q{x, clamp}, checked_cast<Q>(x)): a quantity,
// converted to Q's unit exactly first. A bare number names its unit instead:
// Q{v, u, clamp}.
template <class Q, class X>
constexpr auto number_in(const X& x) {
    return number_in_unit<Q::reference.U>(x);
}
template <class Q, class X>
concept castable_quantity =
    quantity_like<X> && implicit_v<X::reference, Q::reference> && exact_rescale_v<X::reference.U, Q::reference.U>;
template <class Q, class X>
concept castable_from = quantity_like<Q> && castable_quantity<Q, X>;
// A number: an inside, a built-in, a rational or a grid number.
template <class V>
concept number_value = beman::inside::detail::numeric<std::remove_cvref_t<V>> ||
                       std::same_as<std::remove_cvref_t<V>, beman::inside::detail::grid_rational>;
// A bare number where a quantity is wanted: the deleted overloads' test.
template <class Q, class X>
concept bare_number_for = quantity_like<Q> && !quantity_like<X> && number_value<X>;

// Q{v, u}: v a number in the unit of the tag u (a unit, or spec[unit]) that
// names a value of Q. In Q's own unit any number Rep stores; in another unit
// an inside, rescaled exactly first (a raw number has no grid to rescale).
template <class Tag>
inline constexpr reference reference_of_tag = reference_of(Tag{});
template <class Q, class Tag>
concept unit_tag_of = is_reference_tag_v<std::remove_cvref_t<Tag>> &&
                      implicit_v<reference_of_tag<std::remove_cvref_t<Tag>>, std::remove_cvref_t<Q>::reference>;
template <class Q, class Tag>
inline constexpr bool own_unit_v = reference_of_tag<std::remove_cvref_t<Tag>>.U == Q::reference.U;
template <class Q, class V, class Tag>
concept number_in_tag =
    unit_tag_of<Q, Tag> && !quantity_like<V> &&
    ((own_unit_v<Q, Tag> && number_value<V>) ||
     (!own_unit_v<Q, Tag> && insidable<std::remove_cvref_t<V>> &&
      implicit_rescalable<reference_of_tag<std::remove_cvref_t<Tag>>, Q::reference, std::remove_cvref_t<V>>));
// v as a number in Q's unit.
template <class Q, class Tag, class V>
constexpr decltype(auto) number_from_tag(V&& v) {
    if constexpr (own_unit_v<Q, Tag>)
        return std::forward<V>(v);
    else
        return rescale<conversion_factor<reference_of_tag<std::remove_cvref_t<Tag>>.U, Q::reference.U>>(v);
}
template <class Q, class V, class Tag>
using number_from_tag_t = decltype(number_from_tag<Q, Tag>(std::declval<V>()));
// A raw number in another unit than Q's: the deleted overload's test.
template <class Q, class V, class Tag>
concept raw_in_other_unit =
    unit_tag_of<Q, Tag> && !own_unit_v<Q, Tag> && number_value<V> && !insidable<std::remove_cvref_t<V>>;
// A unit of another quantity than Q's.
template <class Q, class Tag>
concept foreign_unit_tag = is_reference_tag_v<std::remove_cvref_t<Tag>> && !unit_tag_of<Q, Tag>;

using beman::inside::detail::is_expected_v;

// Q from v, or an expected Q from an expected v (its error passed on). A
// quantity Q takes v as its number, in its unit.
template <class Q, class V>
constexpr auto lift_to(V&& v) {
    if constexpr (is_expected_v<std::remove_cvref_t<V>>)
        return std::forward<V>(v).transform([](auto&& x) { return lift_to<Q>(std::forward<decltype(x)>(x)); });
    else if constexpr (quantity_like<Q> && !quantity_like<V>)
        return quantity_access::make<Q>(std::forward<V>(v));
    else
        return Q{std::forward<V>(v)};
}

// R with the sign of its scale dropped (see below).
template <reference R>
consteval reference unsigned_reference() {
    reference r     = R;
    r.U.Scale.Ratio = beman::inside::detail::abs(r.U.Scale.Ratio);
    return r;
}

// A quantity of reference R on a number that inside's arithmetic returned: an
// inside, or an expected of one (division by a grid holding 0). A product of
// blades can leave a sign in R's scale (e₂e₁ = -e₁e₂): it goes to the number.
// A zero scale is a product that vanishes: the zero multivector.
template <reference R, class V>
constexpr auto make_quantity(V&& v) {
    if constexpr (R.U.Scale.Ratio.Num.is_zero())
        return multivector<>{};
    else if constexpr (R.U.Scale.Ratio.Num.negative()) {
        constexpr reference P = unsigned_reference<R>();
        if constexpr (is_expected_v<std::remove_cvref_t<V>>)
            return make_quantity<P>(std::forward<V>(v).transform([](const auto& x) { return -x; }));
        else
            return make_quantity<P>(-v);
    } else
        return lift_to<quantity<R, beman::inside::detail::unwrap_t<V>>>(std::forward<V>(v));
}

// q.on_wrap(f) and the other hooks: the inside's policy_ref on q's number,
// with stores taking quantities (rescaled exactly, as +=) and f called on the
// quantity. The other arguments (carry, overshoot, errc) are the inside's.
template <class F, class Q>
constexpr auto on_quantity(Q& q, F&& f) {
    return [&q, f = std::forward<F>(f)](auto&, auto&&... a) mutable -> decltype(auto) {
        return f(q, std::forward<decltype(a)>(a)...);
    };
}

// q.on_clamp(f): f(q, overshoot), the inside's overshoot as a quantity in q's
// unit and kind.
template <reference R, class F, class Q>
constexpr auto on_quantity_clamp(Q& q, F&& f) {
    return [&q, f = std::forward<F>(f)](auto&, auto&& over) mutable -> decltype(auto) {
        return f(q, make_quantity<R>(std::forward<decltype(over)>(over)));
    };
}

// The op of a store back into Rep that Rep deletes because the grids cannot
// prove it (its policy(ec) form, which reports, compiles).
struct plus_assign {
    template <class N, class V>
    constexpr void operator()(N&& n, const V& v) const
        requires requires { n += v; }
    {
        n += v;
    }
};
struct minus_assign {
    template <class N, class V>
    constexpr void operator()(N&& n, const V& v) const
        requires requires { n -= v; }
    {
        n -= v;
    }
};
struct times_assign {
    template <class N, class V>
    constexpr void operator()(N&& n, const V& v) const
        requires requires { n *= v; }
    {
        n *= v;
    }
};
struct divide_assign {
    template <class N, class V>
    constexpr void operator()(N&& n, const V& v) const
        requires requires { n /= v; }
    {
        n /= v;
    }
};
template <class Rep, class V, class Op>
concept needs_channel = !std::invocable<Op, Rep&, const V&> &&
                        requires(Rep& n, errc& ec) { requires std::invocable<Op, decltype(n.policy(ec)), const V&>; };
template <class Rep>
concept step_needs_channel = insidable<Rep> && notch_of<Rep> != 0 && !requires(Rep& n) { ++n; };

template <reference R, class Q, class Ref>
struct quantity_action_ref {
    Q&  Self;
    Ref NumRef;

    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r = v; }
    constexpr Q& operator=(const quantity<R2, V2>& q) {
        NumRef = number_in_unit<R.U>(q);
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r += v; }
    constexpr Q& operator+=(const quantity<R2, V2>& q) {
        NumRef += number_in_unit<R.U>(q);
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r -= v; }
    constexpr Q& operator-=(const quantity<R2, V2>& q) {
        NumRef -= number_in_unit<R.U>(q);
        return Self;
    }
    template <insidable V>
        requires requires(Ref& r, const V& v) { r *= v; }
    constexpr Q& operator*=(const V& v) {
        NumRef *= v;
        return Self;
    }
    template <insidable V>
        requires requires(Ref& r, const V& v) { r /= v; }
    constexpr Q& operator/=(const V& v) {
        NumRef /= v;
        return Self;
    }
};
template <reference R, class Q, class Ref>
constexpr auto make_action_ref(Q& q, Ref&& r) {
    return quantity_action_ref<R, Q, std::remove_cvref_t<Ref>>{q, std::forward<Ref>(r)};
}
} // namespace detail

template <reference R, insidable Rep>
class quantity {
    static_assert(!R.U.Scale.Ratio.Num.negative() && !R.U.Scale.Ratio.Num.is_zero(),
                  "units: this unit carries a sign (e2 * e1 is -(e1 * e2)) - write the unit in another order and "
                  "negate the number");
    Rep Value;

  public:
    using rep                                   = Rep;
    static constexpr units::reference reference = R;
    static constexpr units::unit      unit      = R.U;
    static constexpr units::dimension dimension = dimension_of(R.U);
    // The spec as a value: its tag for a named spec (isq::height), else the
    // spec expression of its recipe. Compare with ==: q.quantity_spec == isq::length.
    static constexpr auto quantity_spec = [] {
        if constexpr (static_cast<bool>(R.Spec))
            return spec_t<R.Spec.P>{};
        else
            return detail::spec_expr{R.Derived, dimension_of(R.U)};
    }();

    constexpr quantity() = default;

    // From a number and its unit: quantity<si::metre, inside<{0, 100}>>{42,
    // si::metre} is 42 m, stored under Rep's policy. An inside in another unit
    // of the same quantity converts exactly first: Q{n, si::kilo(si::metre)}.
    template <class V, class Tag>
        requires detail::number_in_tag<quantity, V, Tag> &&
                 std::constructible_from<Rep, detail::number_from_tag_t<quantity, V, Tag>>
    constexpr quantity(V&& v, Tag) : Value(detail::number_from_tag<quantity, Tag>(std::forward<V>(v))) {}
    // A one-shot policy for this store, as for an inside: a flag or
    // make_policy<F>(ec) (Q{v, si::metre, clamp}, Q{q, round_floor}).
    template <class V, class Tag, class Pol>
        requires(beman::inside::detail::flag_like<Pol> || beman::inside::detail::policy_like<Pol>) &&
                detail::number_in_tag<quantity, V, Tag> &&
                std::constructible_from<Rep, detail::number_from_tag_t<quantity, V, Tag>, Pol>
    constexpr quantity(V&& v, Tag, Pol&& pol)
        : Value(detail::number_from_tag<quantity, Tag>(std::forward<V>(v)), std::forward<Pol>(pol)) {}
    // x a quantity, converted to R's unit exactly first.
    template <class X, class Pol>
        requires(beman::inside::detail::flag_like<Pol> || beman::inside::detail::policy_like<Pol>) &&
                detail::castable_from<quantity, X> &&
                std::constructible_from<Rep, decltype(detail::number_in<quantity>(std::declval<const X&>())), Pol>
    constexpr quantity(const X& x, Pol&& pol) : Value(detail::number_in<quantity>(x), std::forward<Pol>(pol)) {}

    // A number alone names no unit.
    template <class V>
        requires detail::bare_number_for<quantity, V>
    quantity(V&&) = delete ("units: a number alone has no unit - write Q{42, si::metre}, or just<42> * si::metre");
    template <class V, class Pol>
        requires(beman::inside::detail::flag_like<std::remove_cvref_t<Pol>> ||
                 beman::inside::detail::policy_like<std::remove_cvref_t<Pol>>) &&
                    detail::bare_number_for<quantity, V>
    quantity(V&&, Pol&&) = delete ("units: a number alone has no unit - write Q{v, si::metre, clamp}");
    template <class V, class Tag, class... Pol>
        requires detail::raw_in_other_unit<quantity, V, Tag>
    quantity(V&&, Tag, Pol&&...) = delete ("units: Q{v, u} - a raw number in another unit than Q's has no grid to "
                                           "rescale: make it an inside first, or write it in Q's unit");
    template <class V, class Tag, class... Pol>
        requires detail::foreign_unit_tag<quantity, Tag>
    quantity(V&&, Tag, Pol&&...) = delete ("units: Q{v, u} - u is a unit of another quantity than Q's");

    // From another quantity: implicit when the spec converts implicitly and
    // the number, in R's unit, converts implicitly into Rep.
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> && std::constructible_from<Rep, detail::rescaled_t<R2, R, V2>>
    constexpr explicit(!std::convertible_to<detail::rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q)
        : Value(detail::number_in_unit<R.U>(q)) {}

    void numerical_value() const = delete ("units: a number alone has no unit - read q.numerical_value_in(si::metre)");

    // The same quantity in unit U: exact, the number's grid rescaled.
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto in() const {
        constexpr units::reference to{R.Spec, R.KindOf, R.Derived, U};
        return detail::make_quantity<to>(detail::rescale<detail::conversion_factor<R.U, U>>(Value));
    }
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto in(units::unit_t<U>) const {
        return in<U>();
    }
    // The number in unit U: q.numerical_value_in(si::metre).
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto numerical_value_in(units::unit_t<U>) const {
        if constexpr (U == R.U)
            return Value;
        else
            return detail::value_of(in<U>());
    }
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in() const = delete ("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in(units::unit_t<U>) const = delete ("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in() const = delete (BEMAN_INSIDE_UNITS_WHY_PI);
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in(units::unit_t<U>) const = delete (BEMAN_INSIDE_UNITS_WHY_PI);

    // A dimensionless quantity is a number: its value in unit one (km/m is
    // ×1000, % is ×1/100), into any inside that takes it — implicitly when
    // that number converts implicitly. An angle (an is_kind spec) stays a
    // quantity; numerical_value_in(u) reads its number.
    template <insidable T>
        requires detail::number_like_v<R> && std::constructible_from<T, detail::number_t<R, Rep>>
    constexpr explicit(!std::convertible_to<detail::number_t<R, Rep>, T>) operator T() const {
        return T(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Value));
    }
    template <insidable T>
        requires(dimension_of(R.U) == dim_one && !detail::number_like_v<R>)
    operator T() const = delete ("units: an angle (an is_kind quantity) or a pi-scaled ratio is no plain "
                                 "number - read numerical_value_in(si::radian), or value_cast it first");

    // A dimensionless quantity's number as a built-in, as inside's as<T>() and
    // to<T>(): its value in unit one (5 % is 1/20), under the same rule as the
    // conversion above.
    template <typename T, policy_flag Round = beman::inside::detail::none>
        requires detail::number_like_v<R> &&
                 requires(const detail::number_t<R, Rep>& n) { beman::inside::as<T, Round>(n); }
    [[nodiscard]] constexpr T as() const {
        return beman::inside::as<T, Round>(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Value));
    }
    template <typename T, policy_flag Round = beman::inside::detail::none>
        requires detail::number_like_v<R> &&
                 requires(const detail::number_t<R, Rep>& n) { beman::inside::to<T, Round>(n); }
    [[nodiscard]] constexpr auto to() const {
        return beman::inside::to<T, Round>(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Value));
    }

    [[nodiscard]] constexpr auto operator+() const { return detail::make_quantity<R>(+Value); }
    [[nodiscard]] constexpr auto operator-() const { return detail::make_quantity<R>(-Value); }

    // Compound assignment keeps the type: the number is stored back under Rep's
    // policy (checked by default).
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n += r; }
    constexpr quantity& operator+=(const quantity<R2, V2>& q) {
        Value += detail::number_in_unit<R.U>(q);
        return *this;
    }
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n -= r; }
    constexpr quantity& operator-=(const quantity<R2, V2>& q) {
        Value -= detail::number_in_unit<R.U>(q);
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n *= v; }
    constexpr quantity& operator*=(const V& v) {
        Value *= v;
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n /= v; }
    constexpr quantity& operator/=(const V& v) {
        Value /= v;
        return *this;
    }

    // A compound assignment whose result the grids cannot prove to fit Rep
    // (Rep's own op is deleted, its policy(ec) form is not) is deleted with
    // the fix, as on an inside.
#define BEMAN_INSIDE_UNITS_WHY_unproven                                                                 \
    "units: the result may fall outside this quantity's number at run time - store with clamp or wrap " \
    "(q.policy<clamp>() += d), report with q.policy(ec) += d or q.on_error(f) += d, or opt out with unsafe"
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                     detail::needs_channel<Rep, detail::rescaled_t<R2, R, V2>, detail::plus_assign>
    quantity& operator+=(const quantity<R2, V2>&) = delete (BEMAN_INSIDE_UNITS_WHY_unproven);
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                     detail::needs_channel<Rep, detail::rescaled_t<R2, R, V2>, detail::minus_assign>
    quantity& operator-=(const quantity<R2, V2>&) = delete (BEMAN_INSIDE_UNITS_WHY_unproven);
    template <insidable V>
        requires detail::needs_channel<Rep, V, detail::times_assign>
    quantity& operator*=(const V&) = delete (BEMAN_INSIDE_UNITS_WHY_unproven);
    template <insidable V>
        requires detail::needs_channel<Rep, V, detail::divide_assign>
    quantity& operator/=(const V&) = delete (BEMAN_INSIDE_UNITS_WHY_unproven);
#undef BEMAN_INSIDE_UNITS_WHY_unproven

    // One notch of the number up or down, as on an inside, under Rep's policy.
    constexpr quantity& operator++()
        requires requires(Rep& n) { ++n; }
    {
        ++Value;
        return *this;
    }
    constexpr quantity& operator--()
        requires requires(Rep& n) { --n; }
    {
        --Value;
        return *this;
    }
    constexpr quantity operator++(int)
        requires requires(Rep& n) { ++n; }
    {
        auto old = *this;
        ++Value;
        return old;
    }
    constexpr quantity operator--(int)
        requires requires(Rep& n) { --n; }
    {
        auto old = *this;
        --Value;
        return old;
    }
#define BEMAN_INSIDE_UNITS_WHY_step                                                                                \
    "units: ++ and -- may step out of this quantity's number - use a clamp or wrap number, q.policy(ec) += step, " \
    "or iterate with a cursor"
    quantity& operator++()
        requires detail::step_needs_channel<Rep>
    = delete (BEMAN_INSIDE_UNITS_WHY_step);
    quantity& operator--()
        requires detail::step_needs_channel<Rep>
    = delete (BEMAN_INSIDE_UNITS_WHY_step);
    quantity operator++(int)
        requires detail::step_needs_channel<Rep>
    = delete (BEMAN_INSIDE_UNITS_WHY_step);
    quantity operator--(int)
        requires detail::step_needs_channel<Rep>
    = delete (BEMAN_INSIDE_UNITS_WHY_step);
#undef BEMAN_INSIDE_UNITS_WHY_step

    // *t of a quantity cursor (cursor<Q, Step>): its value as a quantity of Q's
    // grid without the end, as on an inside cursor.
    [[nodiscard]] constexpr auto operator*() const
        requires requires(const Rep& n) { *n; }
    {
        return detail::make_quantity<R>(*Value);
    }

    // A store with a hook, as on inside: q.on_wrap(f) += d calls f(q, carry)
    // when the sum wraps. Stores take quantities; f gets the quantity, then
    // the carry (a count of periods), the overshoot (a quantity) or the errc.
    template <class F>
    [[nodiscard]] constexpr auto on_wrap(F&& f) {
        return detail::make_action_ref<R>(*this, Value.on_wrap(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_clamp(F&& f) {
        return detail::make_action_ref<R>(*this,
                                          Value.on_clamp(detail::on_quantity_clamp<R>(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_error(F&& f) {
        return detail::make_action_ref<R>(*this, Value.on_error(detail::on_quantity(*this, std::forward<F>(f))));
    }

    // A store under Rep's policy plus F, as inside's policy<F>(): its =, +=
    // and -= take quantities, *= and /= numbers. policy(ec) sets ec on a
    // failure instead of raising.
    template <policy_flag F = beman::inside::detail::none>
    [[nodiscard]] constexpr auto policy() & {
        return detail::make_action_ref<R>(*this, Value.template policy<F>());
    }
    template <policy_flag F = beman::inside::detail::none>
    [[nodiscard]] constexpr auto policy(errc& ec) {
        return detail::make_action_ref<R>(*this, Value.template policy<F>(ec));
    }

    // Rep's try_make of a number and its unit: quantity<si::metre,
    // M>::try_make(12.5, si::metre), an expected quantity.
    template <policy_flag F = beman::inside::detail::none, class V, class Tag>
        requires detail::number_in_tag<quantity, V, Tag> &&
                 requires(const V& v) { Rep::template try_make<F>(detail::number_from_tag<quantity, Tag>(v)); }
    [[nodiscard]] static constexpr auto try_make(const V& v, Tag) {
        return detail::make_quantity<R>(Rep::template try_make<F>(detail::number_from_tag<quantity, Tag>(v)));
    }
    template <policy_flag F = beman::inside::detail::none, class V>
    static void try_make(const V&) = delete ("units: a number alone has no unit - write Q::try_make(v, si::metre)");

  private:
    friend struct detail::quantity_access;
    struct from_number {};
    template <class V>
    constexpr quantity(from_number, V&& v) : Value(std::forward<V>(v)) {}
};

// A quantity chains through inside's expected-lift operators: a division by
// a grid holding 0 and the rest of the expression stay one std::expected.
} // namespace beman::inside::units
namespace beman::inside::detail {
template <units::reference R, class Rep>
inline constexpr bool lift_operand_v<units::quantity<R, Rep>> = true;

// cursor<Q, Step>, values<Q> and end(t) step over Q's number in Q's unit.
template <units::reference R, class Rep>
struct stepping<units::quantity<R, Rep>> {
    using number = Rep;
    template <class N>
    using rebind = units::quantity<R, N>;
    template <class N>
    static constexpr rebind<N> make(const N& n) {
        return units::detail::quantity_access::make<rebind<N>>(n);
    }
};
} // namespace beman::inside::detail
namespace beman::inside::units {

//---------------------------------------------------------------------------
// Construction: number * unit, number * spec[unit]. A raw scalar has no grid.
//---------------------------------------------------------------------------
template <insidable V, class Tag>
    requires detail::is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const V& v, Tag t) {
    return detail::make_quantity<reference_of(t)>(v);
}
template <class N, class Tag>
    requires detail::is_reference_tag_v<Tag> && std::is_arithmetic_v<N>
void operator*(N, Tag) =
    delete ("units: a raw number has no grid - write just<42> * unit, or Q{42, unit} into a quantity type Q");
// number / unit: 5 / s.
template <insidable V, unit U>
[[nodiscard]] constexpr auto operator/(const V& v, unit_t<U>) {
    return detail::make_quantity<reference_of(detail::unit_pow(U, -1))>(v);
}

// A magnitude scales a quantity's unit, not its number: mag_pi * q is q
// in [π·unit], exact and free (π is never rounded; value_cast to a plain
// unit rounds it once, units/math.hpp). mag<2> * q, q / mag_pi likewise.
namespace detail {
consteval reference scaled_reference(reference r, const magnitude& m) {
    r.U = unit_scaled(r.U, m);
    return r;
}
} // namespace detail
template <magnitude M, reference R, class V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const quantity<R, V>& q) {
    return detail::quantity_access::make<quantity<detail::scaled_reference(R, M), V>>(units::detail::value_of(q));
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, mag_t<M>) {
    return detail::quantity_access::make<quantity<detail::scaled_reference(R, M), V>>(units::detail::value_of(q));
}
// A magnitude times a number: the number in the scaled unit one ([π]), so
// mag_pi * just<2> * si::second is 2 in [π s].
template <magnitude M, insidable V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const V& v) {
    return detail::quantity_access::make<quantity<reference_of(detail::unit_scaled(unit{}, M)), V>>(v);
}
template <insidable V, magnitude M>
[[nodiscard]] constexpr auto operator*(const V& v, mag_t<M>) {
    return detail::quantity_access::make<quantity<reference_of(detail::unit_scaled(unit{}, M)), V>>(v);
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, mag_t<M>) {
    return detail::quantity_access::make<quantity<detail::scaled_reference(R, magnitude{} / M), V>>(
        units::detail::value_of(q));
}

//---------------------------------------------------------------------------
// Arithmetic
//---------------------------------------------------------------------------
namespace detail {
template <reference A, reference B>
consteval reference sum_reference() {
    static_assert(dimension_of(A.U) == dimension_of(B.U),
                  "units: adding or comparing quantities of different dimensions");
    constexpr spec_result s = common_spec(A, B);
    static_assert(s.Ok, "units: adding or comparing quantities of different kinds");
    reference r = s.R;
    r.U         = common_unit(A.U, B.U);
    return r;
}

// Why a sum or comparison of A and B is a unit error, if it is: other
// dimensions or kinds, or no common unit. Each reason is a deleted overload
// with its message.
// Dimensions that differ only in basis vectors are a blade error: their sum
// is a multivector (units/geoalg.hpp).
enum class sum_error : unsigned char { none, dimensions, blades, kinds, pi };

template <reference A, reference B>
consteval sum_error sum_error_of() {
    if (dimension_of(A.U) != dimension_of(B.U))
        return same_outside_blade(dimension_of(A.U), dimension_of(B.U)) ? sum_error::blades : sum_error::dimensions;
    if (!common_spec(A, B).Ok)
        return sum_error::kinds;
    const magnitude ma = magnitude_of(A.U), mb = magnitude_of(B.U);
    if (ma.Pi != mb.Pi)
        return sum_error::pi;
    return sum_error::none;
}
template <reference A, reference B>
consteval bool sum_diagnosed() {
    return sum_error_of<A, B>() != sum_error::none;
}
// Two quantities of different blades: units/geoalg.hpp adds them, its
// overloads more constrained than the deleted ones here.
template <reference A, reference B>
concept blade_sum = sum_error_of<A, B>() == sum_error::blades;
// A number and a quantity of a blade without a unit (3 + 4 i).
template <reference A>
concept number_blade_sum = has_blade(dimension_of(A.U)) && same_outside_blade(dimension_of(A.U), dim_one);

// Whether the two quantities have a common unit (no sum diagnostic) and both
// numbers rescale to it.
template <reference A, class VA, reference B, class VB>
concept common_rescalable = (!sum_diagnosed<A, B>()) && rescalable<A.U, sum_reference<A, B>().U, VA> &&
                            rescalable<B.U, sum_reference<A, B>().U, VB>;
// The number of a quantity of A on V, in A and B's common unit.
template <reference A, class V, reference B>
using common_number_t = rescaled_t<A, sum_reference<A, B>(), V>;

template <reference A, class VA, reference B, class VB, char Op>
concept summable =
    common_rescalable<A, VA, B, VB> && number_op<common_number_t<A, VA, B>, common_number_t<B, VB, A>, Op>;

template <reference A, reference B, int Sign>
consteval reference product_reference() {
    const unit u = unit_mul(A.U, B.U, Sign);
    return spec_product(A, B, Sign, u);
}

// q's number in the common unit of A and B: the unit of their sum.
template <reference A, reference B, reference R, class V>
constexpr auto in_common_unit(const quantity<R, V>& q) {
    return number_in_unit<sum_reference<A, B>().U>(q);
}
} // namespace detail

template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '+'>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) + detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '-'>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) - detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '%'>
[[nodiscard]] constexpr auto operator%(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) % detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '='>
[[nodiscard]] constexpr bool operator==(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::in_common_unit<A, B>(a) == detail::in_common_unit<A, B>(b);
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '<'>
[[nodiscard]] constexpr auto operator<=>(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::in_common_unit<A, B>(a) <=> detail::in_common_unit<A, B>(b);
}

// min, max and midpoint of two quantities of one kind, in their common unit
// as + takes it; the number is inside's min, max or midpoint (exact).
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '<'>
[[nodiscard]] constexpr auto min(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::min(detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b)));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '<'>
[[nodiscard]] constexpr auto max(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::max(detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b)));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '+'>
[[nodiscard]] constexpr auto midpoint(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::midpoint(detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b)));
}

// A sum, difference, remainder or comparison of two units that cannot meet:
// a deleted overload per reason, so a requires-test is false and a call says
// why.
#define BEMAN_INSIDE_UNITS_SUM_DELETED(ret, op, blades)                           \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::dimensions)   \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different dimensions"); \
    template <reference A, class VA, reference B, class VB>                       \
        requires detail::blade_sum<A, B>                                          \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) = delete (blades);     \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::kinds)        \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different kinds");      \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::pi)           \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) = delete (             \
        "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first");
#define BEMAN_INSIDE_UNITS_WHY_GEOALG \
    "units: quantities of different blades add to a multivector - include <beman/inside/units/geoalg.hpp>"
#define BEMAN_INSIDE_UNITS_WHY_ORDER "units: quantities of different blades have no order and no remainder"
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator+, BEMAN_INSIDE_UNITS_WHY_GEOALG)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator-, BEMAN_INSIDE_UNITS_WHY_GEOALG)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator%, BEMAN_INSIDE_UNITS_WHY_ORDER)
BEMAN_INSIDE_UNITS_SUM_DELETED(bool, operator==, BEMAN_INSIDE_UNITS_WHY_GEOALG)
BEMAN_INSIDE_UNITS_SUM_DELETED(std::strong_ordering, operator<=>, BEMAN_INSIDE_UNITS_WHY_ORDER)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, min, BEMAN_INSIDE_UNITS_WHY_ORDER)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, max, BEMAN_INSIDE_UNITS_WHY_ORDER)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, midpoint, BEMAN_INSIDE_UNITS_WHY_ORDER)
#undef BEMAN_INSIDE_UNITS_SUM_DELETED

// A number plus a quantity of a blade: a multivector too.
template <insidable V, reference A, class VA>
    requires detail::number_blade_sum<A>
void operator+(const V&, const quantity<A, VA>&) = delete (BEMAN_INSIDE_UNITS_WHY_GEOALG);
template <reference A, class VA, insidable V>
    requires detail::number_blade_sum<A>
void operator+(const quantity<A, VA>&, const V&) = delete (BEMAN_INSIDE_UNITS_WHY_GEOALG);
template <insidable V, reference A, class VA>
    requires detail::number_blade_sum<A>
void operator-(const V&, const quantity<A, VA>&) = delete (BEMAN_INSIDE_UNITS_WHY_GEOALG);
template <reference A, class VA, insidable V>
    requires detail::number_blade_sum<A>
void operator-(const quantity<A, VA>&, const V&) = delete (BEMAN_INSIDE_UNITS_WHY_GEOALG);
#undef BEMAN_INSIDE_UNITS_WHY_ORDER

template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, 1>()>(units::detail::value_of(a) *
                                                                       units::detail::value_of(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, -1>()>(units::detail::value_of(a) /
                                                                        units::detail::value_of(b));
}

// A quantity scaled by a dimensionless number (an inside).
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(units::detail::value_of(a) * v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '*'>
[[nodiscard]] constexpr auto operator*(const V& v, const quantity<A, VA>& a) {
    return detail::make_quantity<A>(v * units::detail::value_of(a));
}
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(units::detail::value_of(a) / v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '/'>
[[nodiscard]] constexpr auto operator/(const V& v, const quantity<A, VA>& a) {
    constexpr reference R = detail::product_reference<reference{nullptr, true, {}, unit{}}, A, -1>();
    return detail::make_quantity<R>(v / units::detail::value_of(a));
}
// A quantity times a unit: (5 * m) / s.
template <reference A, class VA, class Tag>
    requires detail::is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), 1>()>(units::detail::value_of(a));
}
template <reference A, class VA, class Tag>
    requires detail::is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), -1>()>(units::detail::value_of(a));
}
// No raw scalars: q * 2 needs a grid for the 2.
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(const quantity<A, VA>&, N) = delete ("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(N, const quantity<A, VA>&) = delete ("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(const quantity<A, VA>&, N) = delete ("units: a raw number has no grid - divide by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(N, const quantity<A, VA>&) = delete ("units: a raw number has no grid - divide by just<2>, not 2");

// pow<N>(q) — q·q·…·q (N ≥ 1): exact, the grid of each product.
template <int N, reference R, class V>
    requires(N == 1 || (N % 2 == 1 && requires(const quantity<R, V>& q) { q * pow<N - 1>(q); }) ||
             (N >= 2 && N % 2 == 0 && requires(const quantity<R, V>& q) { pow<N / 2>(q) * pow<N / 2>(q); }))
[[nodiscard]] constexpr auto pow(const quantity<R, V>& q) {
    if constexpr (N == 1)
        return q;
    else if constexpr (N % 2 == 1)
        return q * pow<N - 1>(q);
    else {
        const auto h = pow<N / 2>(q);
        return h * h;
    }
}

//---------------------------------------------------------------------------
// Casts
//---------------------------------------------------------------------------
// quantity_cast<spec>(q) — the same number as another spec of the dimension:
// down the tree (length → height) or across one kind; never across IsKind.
namespace detail {
// Why quantity_cast<To> cannot take R, if it cannot.
enum class cast_error : unsigned char { none, dimension, is_kind };
template <reference R, const quantity_spec* To>
consteval cast_error cast_error_of() {
    if (!(To->Dim == dimension_of(R.U)))
        return cast_error::dimension;
    return spec_casts(R, To) ? cast_error::none : cast_error::is_kind;
}
} // namespace detail

template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return detail::quantity_access::make<quantity<reference{SpecTag::value, false, {}, R.U}, V>>(
        units::detail::value_of(q));
}
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity_cast<decltype(Spec)>(q);
}
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - another dimension");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - another dimension");
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - across an is_kind boundary");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - across an is_kind boundary");

namespace detail {
// v stored into Rep2: Rep2 where the grids prove the store, else
// std::expected<Rep2, errc> (Rep2::try_make), lifted to the quantity Q.
template <class Q, insidable Rep2, class N>
constexpr auto cast_number(const N& v) {
    if constexpr (beman::inside::detail::store_proven<Rep2, N>)
        return lift_to<Q>(Rep2(v));
    else
        return lift_to<Q>(Rep2::try_make(v));
}
} // namespace detail

// value_cast<Rep2>(q) — another representation, the same unit: q's number
// stored into Rep2 by Rep2's policy. The quantity where the grids prove the
// store, else std::expected (try_value_cast always returns one).
template <insidable Rep2, reference R, class V>
    requires requires(const V& v) { Rep2::try_make(v); }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    return detail::cast_number<quantity<R, Rep2>, Rep2>(units::detail::value_of(q));
}
// value_cast<U, Rep2>(q) — in unit U with representation Rep2: the exact
// value in U, stored by Rep2's constructor. (A pi factor, degree ↔ radian,
// rounds once: units/math.hpp.)
template <auto U, insidable Rep2, reference R, class V>
    requires detail::is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2::try_make(units::detail::value_of(q.in(U))); }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    constexpr units::unit u = decltype(U)::value;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    return detail::cast_number<quantity<to, Rep2>, Rep2>(units::detail::value_of(q.in(U)));
}

// value_cast<Q>(q) — into the quantity type Q = quantity<R2, Rep2>: q in
// R2's unit, stored by Rep2's constructor. Q's spec must take q's, as for an
// implicit conversion; only the representation may round.
namespace detail {
template <class Q, reference R>
concept value_castable_to = quantity_like<Q> && dimension_of(Q::reference.U) == dimension_of(R.U) &&
                            spec_converts(R, Q::reference) && exact_rescale_v<R.U, Q::reference.U>;
}
template <class Q, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        Q::rep::try_make(units::detail::value_of(q.template in<Q::reference.U>()));
    }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    return detail::cast_number<Q, typename Q::rep>(units::detail::value_of(q.template in<Q::reference.U>()));
}
// try_value_cast<Q, F>(q) — the same through Q's Rep2::try_make<F>.
template <class Q, policy_flag F = beman::inside::detail::none, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        Q::rep::template try_make<F>(units::detail::value_of(q.template in<Q::reference.U>()));
    }
[[nodiscard]] constexpr std::expected<Q, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<Q>(Q::rep::template try_make<F>(units::detail::value_of(q.template in<Q::reference.U>())));
}

// try_value_cast<Rep2, F>(q) — as Rep2::try_make<F>: an expected.
template <insidable Rep2, policy_flag F = beman::inside::detail::none, reference R, class V>
    requires requires(const V& v) { Rep2::template try_make<F>(v); }
[[nodiscard]] constexpr std::expected<quantity<R, Rep2>, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<quantity<R, Rep2>>(Rep2::template try_make<F>(units::detail::value_of(q)));
}

// try_value_cast<U, Rep2, F>(q) — in unit U, as Rep2::try_make<F>.
template <auto U, insidable Rep2, policy_flag F = beman::inside::detail::none, reference R, class V>
    requires detail::is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2::template try_make<F>(units::detail::value_of(q.in(U))); }
[[nodiscard]] constexpr auto try_value_cast(const quantity<R, V>& q) {
    constexpr reference to{R.Spec, R.KindOf, R.Derived, decltype(U)::value};
    return detail::lift_to<quantity<to, Rep2>>(Rep2::template try_make<F>(units::detail::value_of(q.in(U))));
}

//---------------------------------------------------------------------------
// inside's algorithms on quantities: the number goes through the inside
// function in the target's unit, so each still rounds once and checks once.
//---------------------------------------------------------------------------
namespace detail {
// A plain inside is a quantity of unit one.
template <class T>
constexpr auto as_quantity(const T& x) {
    if constexpr (quantity_like<T>)
        return x;
    else
        return quantity_access::make<quantity<reference_of(one), T>>(x);
}
template <class T>
using as_quantity_t = decltype(as_quantity(std::declval<const T&>()));

// The reference an Out takes: a quantity's, or unit one for a plain inside.
template <class Out>
constexpr reference out_reference() {
    if constexpr (quantity_like<Out>)
        return Out::reference;
    else
        return reference_of(one);
}
template <class Out>
struct out_rep {
    using type = Out;
};
template <class Out>
    requires quantity_like<Out>
struct out_rep<Out> {
    using type = typename Out::rep;
};

// Out from its number (or an expected of one).
template <class Out, class V>
constexpr auto wrap_out(V&& v) {
    if constexpr (!quantity_like<Out>)
        return std::forward<V>(v);
    else
        return lift_to<Out>(std::forward<V>(v));
}

// a·b (E = 1) or a/b (E = −1) stored into Out: the product's reference must
// convert to Out's, implicitly; a plain Out takes a dimensionless result.
template <class Out, int E, class A, class B>
concept into_operands =
    (quantity_like<A> || quantity_like<B> || quantity_like<Out>) &&
    requires {
        requires implicit_v<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>(),
                            out_reference<Out>()>;
    } &&
    rescalable<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>().U,
               out_reference<Out>().U,
               typename as_quantity_t<A>::rep>;
template <class Out, int E, class A>
constexpr auto into_scaled(const A& a, const auto& b) {
    constexpr reference P =
        product_reference<as_quantity_t<A>::reference, std::remove_cvref_t<decltype(as_quantity(b))>::reference, E>();
    return rescale<conversion_factor<P.U, out_reference<Out>().U>>(units::detail::value_of(as_quantity(a)));
}
// The output of mul<C> / div<C> (E = ±1) of a and b: C, a quantity type or a
// plain number for a dimensionless result; or, for another result, C the
// number of a quantity in the product's unit.
template <class C, int E, class A, class B>
struct named_product {
    using type = C;
};
template <insidable C, int E, class A, class B>
    requires(quantity_like<A> || quantity_like<B>) &&
            (!number_like_v<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>()>)
struct named_product<C, E, A, B> {
    using type = quantity<
        unsigned_reference<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>()>(),
        C>;
};
template <class C, int E, class A, class B>
using named_product_t = typename named_product<C, E, A, B>::type;
} // namespace detail

// mul<C>(a, b [, flag]) / div<C>(a, b [, flag]): a·b or a/b rounded once into
// C, a quantity type, or its number (the unit is the product's; a plain
// number for a dimensionless result). The unit factor is folded into the
// exact product: mul<usd>(eth, usd/eth).
template <class C, class A, class B, class... Fl>
    requires(!insidable<A> || !insidable<B>) && detail::into_operands<detail::named_product_t<C, 1, A, B>, 1, A, B> &&
            requires(const A& a, const B& b, const Fl&... f) {
                beman::inside::mul<typename detail::out_rep<detail::named_product_t<C, 1, A, B>>::type>(
                    detail::into_scaled<detail::named_product_t<C, 1, A, B>, 1>(a, b),
                    units::detail::value_of(detail::as_quantity(b)),
                    f...);
            }
[[nodiscard]] constexpr auto mul(const A& a, const B& b, const Fl&... f) {
    using Out = detail::named_product_t<C, 1, A, B>;
    return detail::wrap_out<Out>(beman::inside::mul<typename detail::out_rep<Out>::type>(
        detail::into_scaled<Out, 1>(a, b), units::detail::value_of(detail::as_quantity(b)), f...));
}
template <class C, class A, class B, class... Fl>
    requires(!insidable<A> || !insidable<B>) &&
            detail::into_operands<detail::named_product_t<C, -1, A, B>, -1, A, B> &&
            requires(const A& a, const B& b, const Fl&... f) {
                beman::inside::div<typename detail::out_rep<detail::named_product_t<C, -1, A, B>>::type>(
                    detail::into_scaled<detail::named_product_t<C, -1, A, B>, -1>(a, b),
                    units::detail::value_of(detail::as_quantity(b)),
                    f...);
            }
[[nodiscard]] constexpr auto div(const A& a, const B& b, const Fl&... f) {
    using Out = detail::named_product_t<C, -1, A, B>;
    return detail::wrap_out<Out>(beman::inside::div<typename detail::out_rep<Out>::type>(
        detail::into_scaled<Out, -1>(a, b), units::detail::value_of(detail::as_quantity(b)), f...));
}

// add_all / mul_all with at least one quantity: the same folds as on inside.
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto add_all(const First& first, const Rest&... rest) {
    return (first + ... + rest);
}
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto mul_all(const First& first, const Rest&... rest) {
    return (first * ... * rest);
}

// sum<Q>(r): the quantities of r in Q's unit, added by inside's sum with one
// range check.
template <class Q, std::ranges::input_range Rng>
    requires quantity_like<Q> && quantity_like<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>> &&
             detail::implicit_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference, Q::reference> &&
             detail::exact_rescale_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference.U,
                                     Q::reference.U>
[[nodiscard]] constexpr auto sum(Rng&& r) {
    return detail::lift_to<Q>(
        beman::inside::sum<typename Q::rep>(std::forward<Rng>(r) | std::views::transform([](const auto& q) {
                                                return units::detail::value_of(q.template in<Q::reference.U>());
                                            })));
}

// conversion_overflows<Q>(v, u) / conversion_rounds / conversion_loses: v, a
// number in unit u, against Q's grid (an inside in another unit than Q's
// rescaled exactly first, as Q{v, u} does).
template <class Q, beman::inside::detail::numeric A, class Tag>
    requires quantity_like<Q> && detail::number_in_tag<Q, A, Tag>
[[nodiscard]] constexpr bool conversion_overflows(A value, Tag) noexcept {
    return beman::inside::conversion_overflows<typename Q::rep>(detail::number_from_tag<Q, Tag>(value));
}
template <class Q, beman::inside::detail::numeric A>
    requires quantity_like<Q>
bool conversion_overflows(A) =
    delete ("units: a number alone has no unit - write conversion_overflows<Q>(v, si::metre)");
template <class Q, beman::inside::detail::numeric A, class Tag>
    requires quantity_like<Q> && detail::number_in_tag<Q, A, Tag>
[[nodiscard]] constexpr bool conversion_rounds(A value, Tag) noexcept {
    return beman::inside::conversion_rounds<typename Q::rep>(detail::number_from_tag<Q, Tag>(value));
}
template <class Q, beman::inside::detail::numeric A>
    requires quantity_like<Q>
bool conversion_rounds(A) = delete ("units: a number alone has no unit - write conversion_rounds<Q>(v, si::metre)");
template <class Q, beman::inside::detail::numeric A, class Tag>
    requires quantity_like<Q> && detail::number_in_tag<Q, A, Tag>
[[nodiscard]] constexpr bool conversion_loses(A value, Tag) noexcept {
    return beman::inside::conversion_loses<typename Q::rep>(detail::number_from_tag<Q, Tag>(value));
}
template <class Q, beman::inside::detail::numeric A>
    requires quantity_like<Q>
bool conversion_loses(A) = delete ("units: a number alone has no unit - write conversion_loses<Q>(v, si::metre)");

// checked_cast<Q>(q): q, a quantity converted to Q's unit exactly, stored
// into Q exactly - Q, or std::expected<Q, errc> where it can fail, as
// inside's checked_cast.
template <class Q, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::checked_cast<typename Q::rep>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr auto checked_cast(const X& x) {
    return detail::lift_to<Q>(beman::inside::checked_cast<typename Q::rep>(detail::number_in<Q>(x)));
}
template <class Q, class X>
    requires detail::bare_number_for<Q, X>
Q checked_cast(const X&) = delete ("units: a number alone has no unit - write checked_cast<Q>(v * si::metre) or "
                                   "Q::try_make(v, si::metre)");

// as<T>(q) / to<T>(q): the member forms, free, as for an inside.
template <typename T, policy_flag Round = beman::inside::detail::none, quantity_like Q>
    requires requires(const Q& q) { q.template as<T, Round>(); }
[[nodiscard]] constexpr T as(const Q& q) {
    return q.template as<T, Round>();
}
template <typename T, policy_flag Round = beman::inside::detail::none, quantity_like Q>
    requires requires(const Q& q) { q.template to<T, Round>(); }
[[nodiscard]] constexpr auto to(const Q& q) {
    return q.template to<T, Round>();
}

// dot, cross, lerp on quantity components (lerp's t is a plain number).
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto dot(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * bx + ay * by) {
    return ax * bx + ay * by;
}
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto cross(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * by - ay * bx) {
    return ax * by - ay * bx;
}
// lerp in a and b's common unit, as + takes it: inside's lerp on the numbers,
// so with t in [0, 1] the result's range is that of a and b together.
template <reference A, class VA, reference B, class VB, insidable T>
    requires detail::summable<A, VA, B, VB, '+'> &&
             requires(const quantity<A, VA>& a, const quantity<B, VB>& b, T t) { a + (b - a) * t; }
[[nodiscard]] constexpr auto lerp(const quantity<A, VA>& a, const quantity<B, VB>& b, const T& t) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::lerp(detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b), t));
}

// divmod(a, b, args...) of two quantities of one dimension, in their common
// unit: the quotient is a plain number, the remainder a quantity.
template <class Q, class R>
struct divmod_result {
    Q Quotient;
    R Remainder;
};
namespace detail {
// The remainder as a quantity in C (an expected's error passed on).
template <reference C, class D>
constexpr auto quantity_divmod(D&& d) {
    const auto with_unit = [](const auto& v) {
        using M = quantity<C, decltype(v.Remainder)>;
        return divmod_result<decltype(v.Quotient), M>{v.Quotient, quantity_access::make<M>(v.Remainder)};
    };
    if constexpr (is_expected_v<std::remove_cvref_t<D>>)
        return d.transform(with_unit);
    else
        return with_unit(d);
}
} // namespace detail
template <reference A, class VA, reference B, class VB, class... Args>
    requires detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x,
                      const detail::common_number_t<B, VB, A>& y,
                      Args&&... args) { beman::inside::divmod(x, y, std::forward<Args>(args)...); }
[[nodiscard]] constexpr auto divmod(const quantity<A, VA>& a, const quantity<B, VB>& b, Args&&... args) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::quantity_divmod<C>(beman::inside::divmod(
        detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b), std::forward<Args>(args)...));
}

// add / sub / mod(a, b, args...) of two quantities of one kind, in their
// common unit as + takes it, and mul / div of two quantities (units multiply)
// or of a quantity and a number (the number scales): inside's free arithmetic
// on the numbers, with its per-call policy, flag or errc&.
#define BEMAN_INSIDE_UNITS_SUM_FN(name)                                                                      \
    template <reference A, class VA, reference B, class VB, class... Args>                                   \
        requires detail::common_rescalable<A, VA, B, VB> &&                                                  \
                 requires(const detail::common_number_t<A, VA, B>& x,                                        \
                          const detail::common_number_t<B, VB, A>& y,                                        \
                          Args&&... args) { beman::inside::name(x, y, std::forward<Args>(args)...); }        \
    [[nodiscard]] constexpr auto name(const quantity<A, VA>& a, const quantity<B, VB>& b, Args&&... args) {  \
        constexpr reference C = detail::sum_reference<A, B>();                                               \
        return detail::make_quantity<C>(beman::inside::name(                                                 \
            detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b), std::forward<Args>(args)...)); \
    }
BEMAN_INSIDE_UNITS_SUM_FN(add)
BEMAN_INSIDE_UNITS_SUM_FN(sub)
BEMAN_INSIDE_UNITS_SUM_FN(mod)
#undef BEMAN_INSIDE_UNITS_SUM_FN

#define BEMAN_INSIDE_UNITS_PRODUCT_FN(name, sign)                                                           \
    template <int&... Barrier, reference A, class VA, reference B, class VB, class... Args>                 \
        requires requires(const VA& x, const VB& y, Args&&... args) {                                       \
            beman::inside::name(x, y, std::forward<Args>(args)...);                                         \
        }                                                                                                   \
    [[nodiscard]] constexpr auto name(const quantity<A, VA>& a, const quantity<B, VB>& b, Args&&... args) { \
        return detail::make_quantity<detail::product_reference<A, B, sign>()>(beman::inside::name(          \
            units::detail::value_of(a), units::detail::value_of(b), std::forward<Args>(args)...));          \
    }                                                                                                       \
    template <int&... Barrier, reference A, class VA, insidable V, class... Args>                           \
        requires requires(const VA& x, const V& y, Args&&... args) {                                        \
            beman::inside::name(x, y, std::forward<Args>(args)...);                                         \
        }                                                                                                   \
    [[nodiscard]] constexpr auto name(const quantity<A, VA>& a, const V& v, Args&&... args) {               \
        return detail::make_quantity<A>(                                                                    \
            beman::inside::name(units::detail::value_of(a), v, std::forward<Args>(args)...));               \
    }
BEMAN_INSIDE_UNITS_PRODUCT_FN(mul, 1)
BEMAN_INSIDE_UNITS_PRODUCT_FN(div, -1)
#undef BEMAN_INSIDE_UNITS_PRODUCT_FN

// A number times or over a quantity, as the operators. (The leading int&...
// takes no argument: mul<C>(a, b) is the named form above.)
template <int&... Barrier, insidable V, reference A, class VA, class... Args>
    requires requires(const V& x, const VA& y, Args&&... args) {
        beman::inside::mul(x, y, std::forward<Args>(args)...);
    }
[[nodiscard]] constexpr auto mul(const V& v, const quantity<A, VA>& a, Args&&... args) {
    return detail::make_quantity<A>(beman::inside::mul(v, units::detail::value_of(a), std::forward<Args>(args)...));
}
template <int&... Barrier, insidable V, reference A, class VA, class... Args>
    requires requires(const V& x, const VA& y, Args&&... args) {
        beman::inside::div(x, y, std::forward<Args>(args)...);
    }
[[nodiscard]] constexpr auto div(const V& v, const quantity<A, VA>& a, Args&&... args) {
    constexpr reference R = detail::product_reference<reference{nullptr, true, {}, unit{}}, A, -1>();
    return detail::make_quantity<R>(beman::inside::div(v, units::detail::value_of(a), std::forward<Args>(args)...));
}

// A hook to add ... divmod would be ignored, as on an inside: a deleted
// overload with the reason.
namespace detail {
template <class L, class R, class... Args>
concept arith_with_hook = (quantity_like<L> || quantity_like<R>) &&
                          (beman::inside::detail::is_action_tag_v<std::remove_cvref_t<Args>> || ...);
}
#define BEMAN_INSIDE_UNITS_HOOK_DELETED(name)                                                                    \
    template <int&... Barrier, class L, class R, class... Args>                                                  \
        requires detail::arith_with_hook<L, R, Args...>                                                          \
    auto name(const L&, const R&, Args&&...) = delete (                                                          \
        "units: add/sub/mul/div/mod/divmod take no hooks: on_clamp / on_wrap / on_error apply to a store - use " \
        "q.on_clamp(f) etc.");
BEMAN_INSIDE_UNITS_HOOK_DELETED(add)
BEMAN_INSIDE_UNITS_HOOK_DELETED(sub)
BEMAN_INSIDE_UNITS_HOOK_DELETED(mul)
BEMAN_INSIDE_UNITS_HOOK_DELETED(div)
BEMAN_INSIDE_UNITS_HOOK_DELETED(mod)
BEMAN_INSIDE_UNITS_HOOK_DELETED(divmod)
#undef BEMAN_INSIDE_UNITS_HOOK_DELETED
} // namespace beman::inside::units

// std::hash and std::numeric_limits of a quantity: its number's, the limits
// as quantities in its unit.
template <beman::inside::units::reference R, class Rep>
struct std::hash<beman::inside::units::quantity<R, Rep>> {
    constexpr std::size_t operator()(const beman::inside::units::quantity<R, Rep>& q) const noexcept {
        return std::hash<Rep>{}(beman::inside::units::detail::value_of(q));
    }
};

template <beman::inside::units::reference R, class Rep>
struct std::numeric_limits<beman::inside::units::quantity<R, Rep>> : std::numeric_limits<Rep> {
  private:
    using Q = beman::inside::units::quantity<R, Rep>;
    using N = std::numeric_limits<Rep>;

  public:
    static constexpr Q min() noexcept { return beman::inside::units::detail::quantity_access::make<Q>(N::min()); }
    static constexpr Q max() noexcept { return beman::inside::units::detail::quantity_access::make<Q>(N::max()); }
    static constexpr Q lowest() noexcept {
        return beman::inside::units::detail::quantity_access::make<Q>(N::lowest());
    }
    static constexpr Q epsilon() noexcept
        requires requires { N::epsilon(); }
    {
        return beman::inside::units::detail::quantity_access::make<Q>(N::epsilon());
    }
    static constexpr Q round_error() noexcept
        requires requires { N::round_error(); }
    {
        return beman::inside::units::detail::quantity_access::make<Q>(N::round_error());
    }
};

#endif // BEMAN_INSIDE_UNITS_DETAIL_QUANTITY_HPP
