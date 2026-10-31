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

template <class T>
inline constexpr bool is_quantity_v = false;
template <reference R, class Rep>
inline constexpr bool is_quantity_v<quantity<R, Rep>> = true;

template <class T>
concept quantity_like = is_quantity_v<std::remove_cvref_t<T>>;

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
    return rescale<conversion_factor<R.U, U>>(q.numerical_value());
}

using beman::inside::detail::is_expected_v;

// Q from v, or an expected Q from an expected v (its error passed on).
template <class Q, class V>
constexpr auto lift_to(V&& v) {
    if constexpr (is_expected_v<std::remove_cvref_t<V>>)
        return std::forward<V>(v).transform([](auto&& x) { return Q{std::forward<decltype(x)>(x)}; });
    else
        return Q{std::forward<V>(v)};
}

// A quantity of reference R on a number that inside's arithmetic returned: an
// inside, or an expected of one (division by a grid holding 0).
template <reference R, class V>
constexpr auto make_quantity(V&& v) {
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
};
template <reference R, class Q, class Ref>
constexpr auto make_action_ref(Q& q, Ref&& r) {
    return quantity_action_ref<R, Q, std::remove_cvref_t<Ref>>{q, std::forward<Ref>(r)};
}
} // namespace detail

template <reference R, insidable Rep>
class quantity {
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

    // From a number in the unit R: quantity<si::metre, inside<{0, 100}>>{42}.
    template <class V>
        requires std::constructible_from<Rep, V> && (!quantity_like<V>)
    constexpr explicit quantity(V&& v) : Value(std::forward<V>(v)) {}

    // From another quantity: implicit when the spec converts implicitly and
    // the number, in R's unit, converts implicitly into Rep.
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> && std::constructible_from<Rep, detail::rescaled_t<R2, R, V2>>
    constexpr explicit(!std::convertible_to<detail::rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q)
        : Value(detail::number_in_unit<R.U>(q)) {}

    [[nodiscard]] constexpr const Rep& numerical_value() const noexcept { return Value; }

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
    // The number in unit U.
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto numerical_value_in(units::unit_t<U>) const {
        return in<U>().numerical_value();
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
    // quantity; numerical_value() reads its number.
    template <insidable T>
        requires detail::number_like_v<R> && std::constructible_from<T, detail::number_t<R, Rep>>
    constexpr explicit(!std::convertible_to<detail::number_t<R, Rep>, T>) operator T() const {
        return T(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Value));
    }
    template <insidable T>
        requires(dimension_of(R.U) == dim_one && !detail::number_like_v<R>)
    operator T() const = delete ("units: an angle (an is_kind quantity) or a pi-scaled ratio is no plain "
                                 "number - read numerical_value(), or value_cast it first");

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

    // One unit up or down: ++q is q += 1·unit, under Rep's policy.
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

    // A store with a hook, as on inside: q.on_wrap(f) += d calls f(q, carry)
    // when the sum wraps. Stores take quantities; f gets the quantity.
    template <class F>
    [[nodiscard]] constexpr auto on_wrap(F&& f) {
        return detail::make_action_ref<R>(*this, Value.on_wrap(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_clamp(F&& f) {
        return detail::make_action_ref<R>(*this, Value.on_clamp(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_error(F&& f) {
        return detail::make_action_ref<R>(*this, Value.on_error(detail::on_quantity(*this, std::forward<F>(f))));
    }

    // Rep's try_make in the unit R: quantity<si::metre, M>::try_make(12.5).
    template <policy_flag F = none, class V>
        requires requires(const V& v) { Rep::template try_make<F>(v); }
    [[nodiscard]] static constexpr auto try_make(const V& v) {
        return detail::make_quantity<R>(Rep::template try_make<F>(v));
    }
};

// A quantity chains through inside's expected-lift operators: a division by
// a grid holding 0 and the rest of the expression stay one std::expected.
} // namespace beman::inside::units
namespace beman::inside::detail {
template <units::reference R, class Rep>
inline constexpr bool lift_operand_v<units::quantity<R, Rep>> = true;
} // namespace beman::inside::detail
namespace beman::inside::units {

//---------------------------------------------------------------------------
// Construction: number * unit, number * spec[unit]. A raw scalar has no grid.
//---------------------------------------------------------------------------
template <insidable V, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const V& v, Tag t) {
    return quantity<reference_of(t), V>{v};
}
template <class N, class Tag>
    requires is_reference_tag_v<Tag> && std::is_arithmetic_v<N>
void operator*(N, Tag) =
    delete ("units: a raw number has no grid - write just<42> * unit, or quantity<unit, inside<…>>{42}");
// number / unit: 5 / s.
template <insidable V, unit U>
[[nodiscard]] constexpr auto operator/(const V& v, unit_t<U>) {
    return quantity<reference_of(detail::unit_pow(U, -1)), V>{v};
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
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
// A magnitude times a number: the number in the scaled unit one ([π]), so
// mag_pi * just<2> * si::second is 2 in [π s].
template <magnitude M, insidable V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const V& v) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <insidable V, magnitude M>
[[nodiscard]] constexpr auto operator*(const V& v, mag_t<M>) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, magnitude{} / M), V>{q.numerical_value()};
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
enum class sum_error : unsigned char { none, dimensions, kinds, pi };

template <reference A, reference B>
consteval sum_error sum_error_of() {
    if (dimension_of(A.U) != dimension_of(B.U))
        return sum_error::dimensions;
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

// A sum, difference, remainder or comparison of two units that cannot meet:
// a deleted overload per reason, so a requires-test is false and a call says
// why.
#define BEMAN_INSIDE_UNITS_SUM_DELETED(ret, op)                                   \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::dimensions)   \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different dimensions"); \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::kinds)        \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different kinds");      \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::pi)           \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) = delete (             \
        "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first");
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator+)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator-)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator%)
BEMAN_INSIDE_UNITS_SUM_DELETED(bool, operator==)
BEMAN_INSIDE_UNITS_SUM_DELETED(std::strong_ordering, operator<=>)
#undef BEMAN_INSIDE_UNITS_SUM_DELETED

template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, 1>()>(a.numerical_value() * b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, -1>()>(a.numerical_value() / b.numerical_value());
}

// A quantity scaled by a dimensionless number (an inside).
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() * v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '*'>
[[nodiscard]] constexpr auto operator*(const V& v, const quantity<A, VA>& a) {
    return detail::make_quantity<A>(v * a.numerical_value());
}
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() / v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '/'>
[[nodiscard]] constexpr auto operator/(const V& v, const quantity<A, VA>& a) {
    constexpr reference R = detail::product_reference<reference{nullptr, true, {}, unit{}}, A, -1>();
    return detail::make_quantity<R>(v / a.numerical_value());
}
// A quantity times a unit: (5 * m) / s.
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), 1>()>(a.numerical_value());
}
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), -1>()>(a.numerical_value());
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
    return quantity<reference{SpecTag::value, false, {}, R.U}, V>{q.numerical_value()};
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

// value_cast<Rep2>(q) — another representation, the same unit: Rep2's
// constructor (checked unless Rep2's policy says otherwise).
template <insidable Rep2, reference R, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity<R, Rep2> value_cast(const quantity<R, V>& q) {
    return quantity<R, Rep2>{Rep2(q.numerical_value())};
}
// value_cast<U, Rep2>(q) — in unit U with representation Rep2: the exact
// value in U, stored by Rep2's constructor. (A pi factor, degree ↔ radian,
// rounds once: units/math.hpp.)
template <auto U, insidable Rep2, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    constexpr units::unit u = decltype(U)::value;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    return quantity<to, Rep2>{Rep2(q.in(U).numerical_value())};
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
        static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr Q value_cast(const quantity<R, V>& q) {
    return Q{static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value())};
}
// try_value_cast<Q, F>(q) — the same through Q's Rep2::try_make<F>.
template <class Q, policy_flag F = none, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr std::expected<Q, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<Q>(Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value()));
}

// try_value_cast<Rep2, F>(q) — as Rep2::try_make<F>: an expected.
template <insidable Rep2, policy_flag F = none, reference R, class V>
    requires requires(const V& v) { Rep2::template try_make<F>(v); }
[[nodiscard]] constexpr std::expected<quantity<R, Rep2>, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<quantity<R, Rep2>>(Rep2::template try_make<F>(q.numerical_value()));
}

// try_value_cast<U, Rep2, F>(q) — in unit U, as Rep2::try_make<F>.
template <auto U, insidable Rep2, policy_flag F = none, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2::template try_make<F>(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto try_value_cast(const quantity<R, V>& q) {
    constexpr reference to{R.Spec, R.KindOf, R.Derived, decltype(U)::value};
    return detail::lift_to<quantity<to, Rep2>>(Rep2::template try_make<F>(q.in(U).numerical_value()));
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
        return quantity<reference_of(one), T>{x};
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
    return rescale<conversion_factor<P.U, out_reference<Out>().U>>(as_quantity(a).numerical_value());
}
} // namespace detail

// mul_into<Out, F>(a, b) / div_into<Out, F>(a, b): a·b or a/b rounded once
// into Out, a quantity type (or a plain inside for a dimensionless result).
// The unit factor is folded into the exact product: mul_into<usd>(eth, usd/eth).
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, 1, A, B> && requires(const A& a, const B& b) {
        beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, 1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto mul_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, 1>(a, b), detail::as_quantity(b).numerical_value()));
}
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, -1, A, B> && requires(const A& a, const B& b) {
        beman::inside::div_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, -1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto div_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::div_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, -1>(a, b), detail::as_quantity(b).numerical_value()));
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
[[nodiscard]] constexpr Q sum(Rng&& r) {
    return Q{beman::inside::sum<typename Q::rep>(std::forward<Rng>(r) | std::views::transform([](const auto& q) {
                                                     return q.template in<Q::reference.U>().numerical_value();
                                                 }))};
}

// conversion_overflows<Q>(v) / conversion_rounds / conversion_loses: v, a
// number in Q's unit, against Q's grid.
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    return beman::inside::conversion_overflows<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    return beman::inside::conversion_rounds<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_loses(A value) noexcept {
    return beman::inside::conversion_loses<typename Q::rep>(value);
}

// clamp_cast<Q, Round>(x) / wrap_cast<Q>(x): x a number in Q's unit, or a
// quantity converted to it exactly first.
namespace detail {
template <class Q, class X>
constexpr decltype(auto) number_in(const X& x) {
    if constexpr (quantity_like<X>)
        return number_in_unit<Q::reference.U>(x);
    else
        return (x);
}
template <class Q, class X>
concept castable_quantity =
    quantity_like<X> && implicit_v<X::reference, Q::reference> && exact_rescale_v<X::reference.U, Q::reference.U>;
template <class Q, class X>
concept castable_from = quantity_like<Q> && (castable_quantity<Q, X> || (!quantity_like<X> && numeric<X>));
} // namespace detail
template <class Q, policy_flag Round = none, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q clamp_cast(const X& x) {
    return Q{beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x))};
}
template <class Q, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q wrap_cast(const X& x) {
    return Q{beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x))};
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
template <quantity_like A, quantity_like B, insidable T>
[[nodiscard]] constexpr auto lerp(const A& a, const B& b, const T& t) -> decltype(a + (b - a) * t) {
    return a + (b - a) * t;
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
        return divmod_result<decltype(v.Quotient), M>{v.Quotient, M{v.Remainder}};
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
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_DETAIL_QUANTITY_HPP
